
#include "com_include.h"
#include "core_conf.h"
#include "util.h"
#include "shm.h"
#include "net.h"

#include <my_global.h>
#include <mysql.h>
#include <iconv.h>

#define MAX_SQL_SIZE    950
#define MAX_NAME_SIZE   200
#define MAX_VALU_SIZE   200

// g_op_cmdctl_state[0] : fms,op control
// g_op_cmdctl_state[1] : eqp event powor control
static volatile int g_op_cmdctl_state[2] = { EUT_RTU_STATE_NORMAL , EUT_RTU_STATE_NORMAL };


typedef enum _enumdbconn
{
    EDB_CONN_UPDATE = 0,    //ask
    EDB_CONN_SSRMONITOR,    // sns_data_res 
    EDB_CONN_EQPMONITOR,    // eqp_data_res
    EDB_CONN_CONTROL,       // sns eqp evt_fault , sns eqp ctl_thrld
    EDB_MAX_COUNT,

} enumdbconn;

static MYSQL * g_ptrconn[EDB_MAX_COUNT];

//////////////////////////////////////////////////////////////////////////////////////////////////////////

static conv_update_name_t g_conv_update_names[] =
{
    {EUT_FMCODE_EMBEDDE      , "fms_logger"     },
    {EUT_FMCODE_MCU          , "fms_mcu"        },
    {EUT_FMCODE_REMOTE       , "fms_remote"     },
    {EUT_FMCODE_TEMPER       , "fms_temper"     },
    {EUT_FMCODE_POWER        , "fsms_power"     },
    {EUT_FMCODE_RESERVED     , ""        },
    {EUT_FMCODE_EMBEDDE_OBSER, "fms_observer"   },
};

static packet_rule_t g_fms_packet_rule_list[] = {
    { EFT_NETWORK_CONFIRM_REQ   , EFT_NETWORK_CONFIRM_RES   , 15 , 0},
    { EFT_NETALIVECHK_REQ       , EFT_NETALIVECHK_RES       , 10 , 0},
    { EFT_EVT_BASEDATUPDATE_RES , EFT_EVT_BASEDATUPDATE_REQ , 15 , 0},
    { 0 , 0 , 0 , 0},
};

static packet_rule_t g_cmd_packet_rule_list[] = {
    { EFT_EVT_BASEDATUPDATE_RES , EFT_EVT_BASEDATUPDATE_REQ , 15 , 0},
    { 0 , 0 , 0 , 0},
};

static packet_rule_t g_update_packet_rule_list[] = {
    { EUT_VERSION_INFO_REQ  , EUT_VERSION_INFO_RSP  , 15 , 0},
    { EUT_DOWNLOAD_DATA_REQ , EUT_DOWNLOAD_DATA_RSP , 15 , 0},
    { EUT_DOWNLOAD_FINISH_REQ , EUT_DOWNLOAD_FINISH_RSP , 15 , 0},
    { EUT_UPDATE_FORCED_REQ , EUT_UPDATE_FORCED_RSP , 15 , 0},
    { EUT_UPDATE_FINISH_REQ , EUT_UPDATE_FINISH_RSP , 15 , 0},
    { 0 , 0 , 0 , 0},
};

char * get_conv_update_name(BYTE code)
{
    int ncount = sizeof(g_conv_update_names) / sizeof(conv_update_name_t);
    int i;
    for (i = 0; i < ncount; i++)
    {
        if (g_conv_update_names[i].code == code)
        {
            return g_conv_update_names[i].name;
        }
    }

    return NULL;
}


void set_op_cmdctl_state(int state, int idx)
{
    print_dbg(DBG_SAVE, 1, "OP CMDCTL STATE [%d] (%d) -> (%d) " , idx, g_op_cmdctl_state[idx], state);

    g_op_cmdctl_state[idx] = state;
}

int get_op_cmdctl_state()
{
    return (g_op_cmdctl_state[0] == EUT_RTU_STATE_NORMAL) ? g_op_cmdctl_state[1] : g_op_cmdctl_state[0];
}


int is_op_cmdctl_req(int code)
{
    switch (code)
    {
    //case EFT_SNSRCUDAT_REQ:
    //case EFT_SNSRCTL_POWER:
    //case EFT_SNSRCTL_THRLD:
    //case EFT_EQPCUDAT_REQ:
    case EFT_EQPCTL_POWER:
    //case EFT_EQPCTL_THRLD:
    case EFT_EVT_BASEDATUPDATE_REQ:
    case EFT_EVT_FIRMWAREUPDATE_REQ:
        return 1;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
static int connect_nonb(int sockfd, const struct sockaddr *saptr, int salen, int nsec)
{
    int flags, n, error;
    socklen_t len;
    fd_set rset, wset;
    struct timeval tval;

    flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    error = 0;
    if ( (n = connect(sockfd, (struct sockaddr *) saptr, salen)) < 0)
        if (errno != EINPROGRESS)
            return(-1);

    /* Do whatever we want while the connect is taking place. */

    if (n == 0)
        goto done;  /* connect completed immediately */

    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    wset = rset;
    tval.tv_sec = nsec;
    tval.tv_usec = 0;

    if ( (n = select(sockfd+1, &rset, &wset, NULL, nsec ? &tval : NULL)) == 0)
    {
            close(sockfd);      /* timeout */
            errno = ETIMEDOUT;
            return(-1);
    }

    if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset))
    {
        len = sizeof(error);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
            return(-1);         /* Solaris pending error */
    }
    else
    {
        print_dbg(DBG_ERR, 1, "select error: sockfd not set");
    }

done:
    fcntl(sockfd, F_SETFL, flags);  /* restore file status flags */
    if (error)
    {
        close(sockfd);      /* just in case */
        errno = error;
        return(-1);
    }
    return(0);
}
#endif
int net_connect_server(int port, int maxclnt)
{
    struct sockaddr_in serv_addr;
    int sock = -1;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == ERR_FAIL)
    {
        print_dbg(DBG_ERR, 1, "socket stream create error !!!\n");
        return ERR_FAIL;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)(&serv_addr), sizeof(serv_addr)) == ERR_FAIL)
    {
        print_dbg(DBG_ERR, 1, "binding error %d !!!\n", port);
        net_connect_close(sock);
        return ERR_FAIL;
    }

    if (listen(sock, maxclnt) == ERR_FAIL)
    {
        print_dbg(DBG_ERR, 1, "listen %d error %d !!!\n", maxclnt);
        net_connect_close(sock);
        return ERR_FAIL;
    }

    return sock;
}

int net_connect_client(char* szaddr, int port)
{
    int	so_reuseaddr;
    int so_keepalive;
    struct linger so_linger;
    struct sockaddr_in serv_addr;
    int sock = -1;

    if (szaddr == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrsock,szaddr[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_FAIL;
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == ERR_FAIL)
    {
        print_dbg(DBG_ERR, 1, "socket stream create error !!!\n");
        return ERR_FAIL;
    }

    so_reuseaddr = 1;
    so_keepalive = 1;
    so_linger.l_onoff	= 1;
    so_linger.l_linger	= 0;
    setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr,  sizeof(so_reuseaddr) );
    setsockopt( sock, SOL_SOCKET, SO_KEEPALIVE, &so_keepalive,  sizeof(so_keepalive) );
    setsockopt( sock, SOL_SOCKET, SO_LINGER,    &so_linger,	    sizeof(so_linger) );

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(szaddr);
    serv_addr.sin_port = htons(port);
    if (connect_with_timeout_poll(sock, (struct sockaddr *)(&serv_addr), sizeof(serv_addr), 2) <= 0)
    {
        net_connect_close(sock);
        return ERR_FAIL;
    }

    return sock;
}

int net_connect_close(int sock)
{
    if (sock != -1)
    {
        close(sock);
        return ERR_SUCCESS;
    }
    return ERR_FAIL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////


int net_packet_empty_que( net_packet_t * que, int nsize )
{
    int res = -1;
    int i;
    for (i = 0; i < nsize; i++)
    {
        if (que[i].state == 0)
        {
            res = i;
            break;
        }
    }
    return res;
}



void net_packet_rule_init(net_packet_t * packet)
{
    packet->sendid = 0;
    packet->recvid = 0;
    packet->sendtime = 0;
    packet->timeout = 0;
    packet->func = 0;
    packet->state = 0;// 0:null , 1:send , 2:recv , 3:timeout
    packet->cmd = 0;
}

int net_packet_rule_make(net_packet_t * packet, packet_rule_t * ptrrule, int cnt)
{
    int i;
    if (packet == NULL || ptrrule == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ... [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    for (i = 0; i < cnt; i++)
    {
        packet_rule_t * ptrdata = (ptrrule + i);

        if (packet->sendid == ptrdata->sendid)
        {
            packet->recvid = ptrdata->recvid;
            packet->timeout = ptrdata->timeout;
            packet->sendtime = (long)time(NULL);

            print_dbg(DBG_INFO, 1, "====>>>> net_packet_rule_make (%d)(%d)(%u)(%u)(%d)(%d)",
                packet->sendid, packet->recvid, packet->timeout, packet->sendtime, packet->cmd, cnt);


            return ERR_SUCCESS;
        }
    }

    return ERR_FAIL;

}

int net_packet_fms_rule_make(net_packet_t * packet)
{
    int max_cnt = ( sizeof( g_fms_packet_rule_list ) / sizeof( packet_rule_t ));
    
    return net_packet_rule_make(packet, g_fms_packet_rule_list, max_cnt);
}


int net_packet_cmd_rule_make(net_packet_t * packet)
{
    int max_cnt = ( sizeof( g_cmd_packet_rule_list ) / sizeof( packet_rule_t ));

    return net_packet_rule_make(packet, g_cmd_packet_rule_list, max_cnt);
}

int net_packet_update_rule_make(net_packet_t * packet)
{
    int max_cnt = ( sizeof( g_update_packet_rule_list ) / sizeof( packet_rule_t ));

    return net_packet_rule_make(packet, g_update_packet_rule_list, max_cnt);
}



int net_packet_timeoutcheck(net_packet_t * packet)
{
    if (packet->timeout > 0 && (time(NULL) - packet->sendtime) >= packet->timeout)
        return 1;
    return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////


const char * net_addr_to_string(char *str, struct in_addr addr, size_t len)
{
    return inet_ntop(AF_INET, &addr, str, len);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
void parse_rtupk_collectrslt_aidido(fms_collectrslt_aidido_t * ptrdata)
{
	if (get_print_debug_shm() == 1 )
		print_dbg(DBG_NONE, 1, "SITE_ID(%d) FCLT_ID(%d) FCLT(%d) DATETIME(%d) DBFCLT_TID(%s) FCLTSUBCOUNT(%d) "
				, ptrdata->tfcltelem.nsiteid
				, ptrdata->tfcltelem.nfcltid
				, ptrdata->tfcltelem.efclt
				, ptrdata->tdbtans.tdatetime 
				, ptrdata->tdbtans.sdbfclt_tid 
				, ptrdata->nfcltsubcount); 
}

void parse_rtupk_collectrsltai(fms_collectrsltai_t * ptrdata, int ncount)
{
    fms_collectrsltai_t * ptr;
    int i;
	if (get_print_debug_shm() == 1 )
	{
		for (i = 0; i < ncount; i++)
		{
			ptr = &(ptrdata[i]);
			print_dbg(DBG_NONE, 1, "    [%d] SNSRTYPE(%d) SNSRSUB(%d) IDXSUB(%d) CNNSTATUS(%d) RSLT(%f) "
					, i
					, ptr->esnsrtype 
					, ptr->esnsrsub 
					, ptr->idxsub   
					, ptr->ecnnstatus
					, ptr->frslt); 
		}
	}
}

void parse_rtupk_collectrsltdido(fms_collectrsltdido_t * ptrdata, int ncount)
{
    fms_collectrsltdido_t * ptr;
    int i;
	if (get_print_debug_shm() == 1 )
	{
		for (i = 0; i < ncount; i++)
		{
			ptr = &(ptrdata[i]);
			print_dbg(DBG_NONE, 1, "    [%d] SNSRTYPE(%d) SNSRSUB(%d) IDXSUB(%d) CNNSTATUS(%d) RSLT(%d) "
					, i
					, ptr->esnsrtype 
					, ptr->esnsrsub 
					, ptr->idxsub   
					, ptr->ecnnstatus
					, ptr->brslt);
		}
	}
}

void parse_rtupk_evt_faultaidi(fms_evt_faultaidi_t * ptrdata)
{
    print_dbg(DBG_NONE, 1, "SITE_ID(%d) FCLT_ID(%d) FCLT(%d) DATETIME(%d) DBFCLT_TID(%s) FCLTSUBCOUNT(%d) "
        , ptrdata->tfcltelem.nsiteid
        , ptrdata->tfcltelem.nfcltid
        , ptrdata->tfcltelem.efclt
        , ptrdata->tdbtans.tdatetime 
        , ptrdata->tdbtans.sdbfclt_tid
        , ptrdata->nfcltsubcount); 
}

void parse_rtupk_faultdataai(fms_faultdataai_t * ptrdata, int ncount)
{
    fms_faultdataai_t * ptr;
    int i;
    for (i = 0; i < ncount; i++)
    {
        ptr = &(ptrdata[i]);
        print_dbg(DBG_NONE, 1, "    [%d] SNSRTYPE(%d) SNSRSUB(%d) IDXSUB(%d) GRADE(%d) CNNSTATUS(%d) RSLT(%f) "
            , i
            , ptr->esnsrtype 
            , ptr->esnsrsub  
            , ptr->idxsub   
            , ptr->egrade   
            , ptr->ecnnstatus 
            , ptr->frslt); 
    }
}

void parse_rtupk_faultdatadi(fms_faultdatadi_t * ptrdata, int ncount)
{
    fms_faultdatadi_t * ptr;
    int i;
    for (i = 0; i < ncount; i++)
    {
        ptr = &(ptrdata[i]);
        print_dbg(DBG_NONE, 1, "    [%d] SNSRTYPE(%d) SNSRSUB(%d) IDXSUB(%d) GRADE(%d) CNNSTATUS(%d) RSLT(%d) "
            , i
            , ptr->esnsrtype 
            , ptr->esnsrsub  
            , ptr->idxsub   
            , ptr->egrade   
            , ptr->ecnnstatus 
            , ptr->brslt); 
    }
}


void parse_rtupk_ctl_base(fms_ctl_base_t * ptrdata)
{
	char szip[ MAX_IP_SIZE ];

	memset( szip, 0, sizeof( szip ));

	cnv_addr_to_strip( ptrdata->op_ip, szip );

    print_dbg(DBG_NONE, 1, "SITE_ID(%d) FCLT_ID(%d) FCLT(%d) SNSRTYPE(%d) SNSRSUB(%d) IDXSUB(%d) DATETIME(%d) DBFCLT_TID(%s) USERID(%s) CTLTYPE(%d) CTLCMD(%d) , OP_IP:%s"
        , ptrdata->tfcltelem.nsiteid 
        , ptrdata->tfcltelem.nfcltid 
        , ptrdata->tfcltelem.efclt 
        , ptrdata->esnsrtype       
        , ptrdata->esnsrsub	   
        , ptrdata->idxsub        
        , ptrdata->tdbtans.tdatetime 
        , ptrdata->tdbtans.sdbfclt_tid 
        , ptrdata->suserid
        , ptrdata->ectltype 
        , ptrdata->ectlcmd
		, szip );   
}

void parse_rtupk_thrlddataai(fms_thrlddataai_t * ptrdata)
{
    print_dbg(DBG_NONE, 1, "NAME(%s) NOTIFY(%d) OFFSETVAL(%f) MIN(%f) MAX(%f) RANGE [0](%f) [1](%f) [2](%f) [3](%f) [4](%f) [5](%f) "
        , ptrdata->sname      
        , ptrdata->bnotify   
        , ptrdata->foffsetval 
        , ptrdata->fmin 
        , ptrdata->fmax   
        , ptrdata->frange[0], ptrdata->frange[1], ptrdata->frange[2]
        , ptrdata->frange[3], ptrdata->frange[4], ptrdata->frange[5]
        ); 

}

void parse_rtupk_thrlddatadi(fms_thrlddatadi_t * ptrdata)
{
    print_dbg(DBG_NONE, 1, "NAME(%s) NOTIFY(%d) GRADE(%d) NORMALVAL(%d) BFAULTVAL(%d) NORMALNAME(%s) FAULTNAME(%s) "
        , ptrdata->sname    
        , ptrdata->bnotify 
        , ptrdata->egrade     
        , ptrdata->bnormalval    
        , ptrdata->bfaultval  
        , ptrdata->snormalname 
        , ptrdata->sfaultname); 
}

void parse_rtupk_fclt_reqtimedata(fclt_reqtimedata_t * ptrdata)
{
    print_dbg(DBG_NONE, 1, "site_id(%d) fclt_id(%d) fclt(%d) timet(%d) "
        , ptrdata->tfcltelem.nsiteid
        , ptrdata->tfcltelem.nfcltid
        , ptrdata->tfcltelem.efclt
        , ptrdata->timet);
}
void parse_rtupk_eqp_sts_resp( fclt_ctlstatus_eqponoffrslt_t * ptrdata )
{
    print_dbg(DBG_NONE, 1, "site_id(%d) fclt_id(%d) fcltcd(%d) ctlcmd(%d), maxstep(%d), curstep(%d), curstatus(%d), result(%d) "
        , ptrdata->tfcltelem.nsiteid
        , ptrdata->tfcltelem.nfcltid
        , ptrdata->tfcltelem.efclt
		, ptrdata->ectlcmd
        , ptrdata->emaxstep
        , ptrdata->ecurstep
        , ptrdata->ecurstatus
        , ptrdata->ersult );
}

void parse_rtupk_eqp_net_sts_resp( fclt_cnnstatus_eqprslt_t * ptrdata )
{
    print_dbg(DBG_NONE, 1, "site_id(%d) fclt_id(%d) fclt(%d) connetstatus(%d) "
        , ptrdata->tfcltelem.nsiteid
        , ptrdata->tfcltelem.nfcltid
        , ptrdata->tfcltelem.efclt
        , ptrdata->ecnnstatus);
}

void parse_rtupk_eqp_off_opt_resp( fclt_ctrl_eqponoff_optionoff_t* ptrdata )
{
    print_dbg(DBG_NONE, 1, "OFF OPT enumcmd(%d) SW1(%d) SW2(%d) SW3(%d) SW4(%d) SW5(%d) STEP1(%d) OS_END(%d)"
        , ptrdata->idelay_powerswitch_ms[ 0 ]
        , ptrdata->idelay_powerswitch_ms[ 1 ]
        , ptrdata->idelay_powerswitch_ms[ 2 ]
        , ptrdata->idelay_powerswitch_ms[ 3 ]
        , ptrdata->idelay_powerswitch_ms[ 4 ]
        , ptrdata->iwaittime_step1_ms
        , ptrdata->iwaittime_eqposend_ms );
}

void parse_rtupk_eqp_on_opt_resp( fclt_ctrl_eqponoff_optionon_t* ptrdata )
{
    print_dbg(DBG_NONE, 1, "ON OPT enumcmd(%d) SW1(%d) SW2(%d) SW3(%d) SW4(%d) SW5(%d) OS_START(%d) D_CNN(%d) STEP1(%d) STEP2(%d)"
        , ptrdata->idelay_powerswitch_ms[ 0 ]
        , ptrdata->idelay_powerswitch_ms[ 1 ]
        , ptrdata->idelay_powerswitch_ms[ 2 ]
        , ptrdata->idelay_powerswitch_ms[ 3 ]
        , ptrdata->idelay_powerswitch_ms[ 4 ]
        , ptrdata->iwaittime_eqposstart_ms
        , ptrdata->iwaittime_eqpdmncnn_ms
        , ptrdata->iwaittime_step1_ms
        , ptrdata->iwaittime_step2_ms );
}


void parse_rtupk_fclt_evt_dbdataupdate(fclt_evt_dbdataupdate_t * ptrdata)
{
    print_dbg(DBG_NONE, 1, "SITE_ID(%d) FCLT_ID(%d) FCLT(%d) TYPE(%d) CMD(%d) INFO(%d) "
        , ptrdata->tfcltelem.nsiteid
        , ptrdata->tfcltelem.nfcltid
        , ptrdata->tfcltelem.efclt
        , ptrdata->ectltype
        , ptrdata->ectlcmd
        , ptrdata->edbeditinfo);    //  [10/21/2016 redIce]
}

void parse_rtupk_fclt_ctlthrldcompleted(fclt_ctlthrldcompleted_t * ptrdata)
{
    print_dbg(DBG_NONE, 1, "SITE_ID(%d) FCLT_ID(%d) FCLT(%d) SNSRTYPE(%d) SNSRSUB(%d) IDXSUB(%d) RESULT(%d)  "
        , ptrdata->tfcltelem.nsiteid
        , ptrdata->tfcltelem.nfcltid
        , ptrdata->tfcltelem.efclt
        , ptrdata->esnsrtype
        , ptrdata->esnsrsub
        , ptrdata->idxsub
        , ptrdata->iresult);
}

void parse_rtupk_fclt_evt_faultfclt(fclt_evt_faultfclt_t * ptrdata)
{
    print_dbg(DBG_NONE, 1, "SITE_ID(%d) FCLT_ID(%d) FCLT(%d) STATUS(%d)  "
        , ptrdata->tfcltelem.nsiteid
        , ptrdata->tfcltelem.nfcltid
        , ptrdata->tfcltelem.efclt
        , ptrdata->ecnnstatus);
}


int parse_rtupk_get_snsrtype(void * ptrdata, int pos)
{
    enumsnsr_type esnsrtype;

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    memcpy(&esnsrtype, ptrdata + pos, sizeof(esnsrtype));

    return esnsrtype;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

int ctl_sns_data_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = (inter_msg_t *) ptrdata;
    int datatype = 0;

    print_dbg( DBG_NONE, 1, "ctl_sns_data_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_collectrslt_ai_t , fclt_collectrslt_dido_t

    datatype = parse_rtupk_get_snsrtype(ptrmsg->ptrbudy, sizeof(fms_collectrslt_aidido_t));

    switch (datatype)
    {
    case ESNSRTYP_AI:
        {
            fms_packet_header_t     head;
            fclt_collectrslt_ai_t   body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_collectrslt_aidido(&body.aidido);
            parse_rtupk_collectrsltai(body.rslt, body.aidido.nfcltsubcount);

            insert_db_ai_hist(&body);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    case ESNSRTYP_DI:
    case ESNSRTYP_DO:
        {
            fms_packet_header_t     head;
            fclt_collectrslt_dido_t body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_collectrslt_aidido(&body.aidido);
            parse_rtupk_collectrsltdido(body.rslt, body.aidido.nfcltsubcount);

            insert_db_dido_hist(&body);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    }
    return ERR_SUCCESS;
}

int ctl_sns_cudata_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = (inter_msg_t *) ptrdata;
    int datatype = 0;

    print_dbg( DBG_NONE, 1, "ctl_sns_cudata_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    //fclt_collectrslt_dido_t , fclt_collectrslt_ai_t

    datatype = parse_rtupk_get_snsrtype(ptrmsg->ptrbudy, sizeof(fms_collectrslt_aidido_t));

    switch (datatype)
    {
    case ESNSRTYP_AI:
        {
            fms_packet_header_t     head;
            fclt_collectrslt_ai_t   body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_collectrslt_aidido(&body.aidido);
            parse_rtupk_collectrsltai(body.rslt, body.aidido.nfcltsubcount);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    case ESNSRTYP_DI:
    case ESNSRTYP_DO:
        {
            fms_packet_header_t     head;
            fclt_collectrslt_dido_t body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_collectrslt_aidido(&body.aidido);
            parse_rtupk_collectrsltdido(body.rslt, body.aidido.nfcltsubcount);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    }

    return ERR_SUCCESS;
}

int ctl_sns_evt_fault(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = (inter_msg_t *) ptrdata;
    int datatype = 0;

    print_dbg( DBG_NONE, 1, "ctl_sns_evt_fault [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_evt_faultai , fclt_evt_faultdi

    datatype = parse_rtupk_get_snsrtype(ptrmsg->ptrbudy, sizeof(fms_evt_faultaidi_t));

    switch (datatype)
    {
    case ESNSRTYP_AI:
        {
            fms_packet_header_t head;
            fclt_evt_faultai_t  body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_evt_faultaidi(&body.fault_aidi);
            parse_rtupk_faultdataai(body.rslt, body.fault_aidi.nfcltsubcount);

            insert_db_event(datatype, &body, ptrmsg->fault_snsr_id, MAX_GRP_RSLT_AI, ptrmsg->szdata, ptrmsg->ndata);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    case ESNSRTYP_DI:
    case ESNSRTYP_DO:
        {
            fms_packet_header_t head;
            fclt_evt_faultdi_t  body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_evt_faultaidi(&body.fault_aidi);
            parse_rtupk_faultdatadi(body.rslt, body.fault_aidi.nfcltsubcount);

            insert_db_event(datatype, &body, ptrmsg->fault_snsr_id, MAX_GRP_RSLT_DI, ptrmsg->szdata, ptrmsg->ndata);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    }

    return ERR_SUCCESS;
}

int ctl_sns_ctl_power(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;
    int snsr_id = -1;

    print_dbg( DBG_NONE,1,"ctl_sns_ctl_power [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    //fclt_ctrl_snsronoff_t
    {
        fms_packet_header_t     head;
        fclt_ctrl_snsronoff_t   body;
        snsr_info_t snsrinfo;
        memset(&snsrinfo, 0, sizeof(snsrinfo));

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));
        memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

        parse_rtupk_ctl_base(&body.ctl_base);
        print_dbg(DBG_NONE, 1, "SNSRHW(%d) ", body.isnsrhwproperty); // 하드웨어 종류
        
        // DB이력인풋 - 센서 아이디 검색 쉐이드 메모리에서 GET 변경 [10/26/2016 redIce] 
        if (get_db_col_info_shm(
            body.ctl_base.esnsrtype, body.ctl_base.esnsrsub, body.ctl_base.idxsub,
            &snsrinfo) != ERR_FAIL)
        {
            snsr_id = snsrinfo.snsr_id;
            insert_db_sns_ctl_hist_onoff(&body, snsr_id);
        }

        ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
        ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
    }

    return ERR_SUCCESS;
}

int ctl_sns_ctl_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;
    int datatype = 0;

    print_dbg( DBG_NONE,1,"ctl_sns_ctl_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_collectrslt_ai_t,  fclt_collectrslt_dido_t

    datatype = parse_rtupk_get_snsrtype(ptrmsg->ptrbudy, sizeof(fms_collectrslt_aidido_t));

    switch (datatype)
    {
    case ESNSRTYP_AI:
        {
            fms_packet_header_t     head;
            fclt_collectrslt_ai_t   body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_collectrslt_aidido(&body.aidido);
            parse_rtupk_collectrsltai(body.rslt, body.aidido.nfcltsubcount);

            insert_db_ai_hist(&body);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    case ESNSRTYP_DI:
    case ESNSRTYP_DO:
        {
            fms_packet_header_t     head;
            fclt_collectrslt_dido_t body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_collectrslt_aidido(&body.aidido);
            parse_rtupk_collectrsltdido(body.rslt, body.aidido.nfcltsubcount);

            insert_db_dido_hist(&body);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    }

    return ERR_SUCCESS;
}

int ctl_sns_ctl_thrld(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;
    int datatype = 0;
    int snsr_id = -1;

    print_dbg( DBG_NONE,1,"ctl_sns_ctl_thrld [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_ctl_thrldai_t , fclt_ctl_thrlddi_t 

    datatype = parse_rtupk_get_snsrtype(ptrmsg->ptrbudy, sizeof(fms_elementfclt_t));

    switch (datatype)
    {
    case ESNSRTYP_AI:
        {
            fms_packet_header_t head;
            fclt_ctl_thrldai_t  body;
            snsr_info_t snsrinfo;
            memset(&snsrinfo, 0, sizeof(snsrinfo));

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_ctl_base(&body.ctl_base);
            parse_rtupk_thrlddataai(&body.tthrlddtai);

            // DB이력인풋 - 센서 아이디 검색 쉐이드 메모리에서 GET 변경 [10/26/2016 redIce] 
            if (get_db_col_info_shm(
                body.ctl_base.esnsrtype, body.ctl_base.esnsrsub, body.ctl_base.idxsub,
                &snsrinfo) != ERR_FAIL)
            {
                snsr_id = snsrinfo.snsr_id;
                insert_db_sns_ctl_hist(&body, snsr_id);
                insert_db_aithrld(&body, snsr_id);
            }
            else
            {
                print_dbg( DBG_ERR, 1, "SnsrId not found [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
            }

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    case ESNSRTYP_DI:
    case ESNSRTYP_DO:
        {
            fms_packet_header_t head;
            fclt_ctl_thrlddi_t  body;
            snsr_info_t snsrinfo;
            memset(&snsrinfo, 0, sizeof(snsrinfo));

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_ctl_base(&body.ctl_base);
            parse_rtupk_thrlddatadi(&body.tthrlddtdi);

            // DB이력인풋 - 센서 아이디 검색 쉐이드 메모리에서 GET 변경 [10/26/2016 redIce] 
            if (get_db_col_info_shm(
                body.ctl_base.esnsrtype, body.ctl_base.esnsrsub, body.ctl_base.idxsub,
                &snsrinfo) != ERR_FAIL)
            {
                snsr_id = snsrinfo.snsr_id;
                insert_db_sns_ctl_hist(&body, snsr_id);
                insert_db_didothrld(&body, snsr_id);
            }
            else
            {
                print_dbg( DBG_ERR, 1, "SnsrId not found [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
            }

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    }

    return ERR_SUCCESS;
}

int ctl_sns_completed_thrld(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;

    print_dbg( DBG_NONE,1,"ctl_sns_completed_thrld [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_ctl_thrldai_t , fclt_ctl_thrlddi_t
    {
        fms_packet_header_t      head;
        fclt_ctlthrldcompleted_t body;

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));
        memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

        parse_rtupk_fclt_ctlthrldcompleted(&body);

        ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
        ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);

    }

    return ERR_SUCCESS;
}

int ctl_eqp_data_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;
    int datatype = 0;

	if (get_print_debug_shm() == 1 )
		print_dbg( DBG_NONE,1,"ctl_eqp_data_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

	if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
	{
		if (ptrcomm == NULL)
			print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
		else
			print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
					ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
		return ERR_RETURN;
	}

	if (get_print_debug_shm() == 1 )
    	print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    //fclt_collectrslt_dido_t , fclt_collectrslt_ai_t

    datatype = parse_rtupk_get_snsrtype(ptrmsg->ptrbudy, sizeof(fms_collectrslt_aidido_t));

    switch (datatype)
    {
    case ESNSRTYP_AI:
        {
            fms_packet_header_t     head;
            fclt_collectrslt_ai_t   body;

            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_collectrslt_aidido(&body.aidido);
            parse_rtupk_collectrsltai(body.rslt, body.aidido.nfcltsubcount);

            insert_db_eqp_hist(datatype, &body);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    case ESNSRTYP_DI:
    case ESNSRTYP_DO:
        {
            fms_packet_header_t     head;
            fclt_collectrslt_dido_t body;

            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_collectrslt_aidido(&body.aidido);
            parse_rtupk_collectrsltdido(body.rslt, body.aidido.nfcltsubcount);

            insert_db_eqp_hist(datatype, &body);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    }

    return ERR_SUCCESS;
}

int ctl_eqp_cudata_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;
    int datatype = 0;

    print_dbg( DBG_NONE,1,"ctl_eqp_cudata_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    //fclt_collectrslt_dido_t , fclt_collectrslt_ai_t

    datatype = parse_rtupk_get_snsrtype(ptrmsg->ptrbudy, sizeof(fms_collectrslt_aidido_t));

    switch (datatype)
    {
    case ESNSRTYP_AI:
        {
            fms_packet_header_t     head;
            fclt_collectrslt_ai_t   body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_collectrslt_aidido(&body.aidido);
            parse_rtupk_collectrsltai(body.rslt, body.aidido.nfcltsubcount);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    case ESNSRTYP_DI:
    case ESNSRTYP_DO:
        {
            fms_packet_header_t     head;
            fclt_collectrslt_dido_t body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_collectrslt_aidido(&body.aidido);
            parse_rtupk_collectrsltdido(body.rslt, body.aidido.nfcltsubcount);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    }

    return ERR_SUCCESS;
}

int ctl_eqp_evt_fault(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;
    int datatype = 0;

    print_dbg( DBG_NONE,1,"ctl_eqp_evt_fault [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_evt_faultdi_t , fclt_evt_faultai_t

    datatype = parse_rtupk_get_snsrtype(ptrmsg->ptrbudy, sizeof(fms_evt_faultaidi_t));

    switch (datatype)
    {
    case ESNSRTYP_AI:
        {
            fms_packet_header_t head;
            fclt_evt_faultai_t  body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_evt_faultaidi(&body.fault_aidi);
            parse_rtupk_faultdataai(body.rslt, body.fault_aidi.nfcltsubcount);

            insert_db_event(datatype, &body, ptrmsg->fault_snsr_id, MAX_GRP_RSLT_AI, ptrmsg->szdata, ptrmsg->ndata);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    case ESNSRTYP_DI:
    case ESNSRTYP_DO:
        {
            fms_packet_header_t head;
            fclt_evt_faultdi_t  body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_evt_faultaidi(&body.fault_aidi);
            parse_rtupk_faultdatadi(body.rslt, body.fault_aidi.nfcltsubcount);

            insert_db_event(datatype, &body, ptrmsg->fault_snsr_id, MAX_GRP_RSLT_DI, ptrmsg->szdata, ptrmsg->ndata);

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    }

    return ERR_SUCCESS;
}

int ctl_eqp_ctl_power(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;
    int snsr_id = -1;

    print_dbg( DBG_NONE,1,"ctl_eqp_ctl_power [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_ctrl_eqponoff_t
    {
        fms_packet_header_t     head;
        fclt_ctrl_eqponoff_t    body;
        snsr_info_t snsrinfo;
        memset(&snsrinfo, 0, sizeof(snsrinfo));

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));
        memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

        parse_rtupk_ctl_base(&body.ctl_base);
        print_dbg( DBG_NONE, 1, "AUTOMODE(%d) SNSRHW(%d) "
            , body.bautomode   // 고정감시 운영sw 측에서 자동으로 off->on 하고자 하는 경우 ★★★
            , body.isnsrhwproperty);   // "하드웨어 종류" *************

        if (inter_msg == EFT_EQPCTL_POWER)
        {
            if (ptrmsg->ndata == 9) // eqp power contol auto event
                set_op_cmdctl_state(EUT_RTU_STATE_CONTROL, 1);
            else
                set_op_cmdctl_state(EUT_RTU_STATE_CONTROL, 0);
        }

        // DB이력인풋 - 센서 아이디 검색 쉐이드 메모리에서 GET 변경 [10/26/2016 redIce] 
        if (get_db_col_info_shm(
            body.ctl_base.esnsrtype, body.ctl_base.esnsrsub, body.ctl_base.idxsub,
            &snsrinfo) != ERR_FAIL)
        {
            snsr_id = snsrinfo.snsr_id;
            insert_db_eqp_ctl_hist_onoff(&body, snsr_id);
        }

        ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
        ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
    }
    return ERR_SUCCESS;
}


int ctl_eqp_ctl_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;
    int snsr_id = -1;

    print_dbg( DBG_NONE,1,"ctl_eqp_ctl_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_ctrl_eqponoffrslt_t
    {
        fms_packet_header_t      head;
        fclt_ctrl_eqponoffrslt_t body;
        snsr_info_t snsrinfo;
        memset(&snsrinfo, 0, sizeof(snsrinfo));

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));
        memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

        parse_rtupk_ctl_base(&body.ctl_base);

        print_dbg( DBG_NONE,1,"MAXSTEP(%d) CURSTEP(%d) CURSTATUS(%d) ERSULT(%d) "
            , body.emaxstep
            , body.ecurstep
            , body.ecurstatus
			, body.ersult);

        // DB이력인풋 - 센서 아이디 검색 쉐이드 메모리에서 GET 변경 [10/26/2016 redIce] 
        if (get_db_col_info_shm(
            body.ctl_base.esnsrtype, body.ctl_base.esnsrsub, body.ctl_base.idxsub,
            &snsrinfo) != ERR_FAIL)
        {
            snsr_id = snsrinfo.snsr_id;
            insert_db_eqp_ctl_hist_onoffrslt(&body, snsr_id);

			//printf("=================ssssssssss1\r\n");
        }
        else
        {
            print_dbg( DBG_ERR, 1, "SnsrId not found [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        }

        ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
        ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);

        // OP 제어해제 intermsg ndata 1 : 제어해제 , 0 : 제어진행 [10/19/2016 redIce]
        if (ptrmsg->ndata == 1)//&& body.emaxstep == body.ecurstep)//   body.ecurstatus == EFCLTCTLST_END)
        {
            if (g_op_cmdctl_state[1] == EUT_RTU_STATE_CONTROL)
                set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 1);
            else
                set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
        }
    }
    return ERR_SUCCESS;
}

int ctl_eqp_ctl_thrld(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;
    int snsr_id = -1;
    int datatype = 0;

    print_dbg( DBG_NONE,1,"ctl_eqp_ctl_thrld [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_ctl_thrldai_t , fclt_ctl_thrlddi_t
    datatype = parse_rtupk_get_snsrtype(ptrmsg->ptrbudy, sizeof(fms_elementfclt_t));

    switch (datatype)
    {
    case ESNSRTYP_AI:
        {
            fms_packet_header_t head;
            fclt_ctl_thrldai_t  body;
            snsr_info_t snsrinfo;
            memset(&snsrinfo, 0, sizeof(snsrinfo));

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_ctl_base(&body.ctl_base);
            parse_rtupk_thrlddataai(&body.tthrlddtai);

            // DB이력인풋 - 센서 아이디 검색 쉐이드 메모리에서 GET 변경 [10/26/2016 redIce] 
            if (get_db_col_info_shm(
                body.ctl_base.esnsrtype, body.ctl_base.esnsrsub, body.ctl_base.idxsub,
                &snsrinfo) != ERR_FAIL)
            {
                snsr_id = snsrinfo.snsr_id;
                // DB이력인풋 [10/24/2016 redIce]
                insert_db_eqp_ctl_hist(&body, snsr_id);
                insert_db_aithrld(&body, snsr_id);
            }
            else
            {
                print_dbg( DBG_ERR, 1, "SnsrId not found [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
            }

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    case ESNSRTYP_DI:
    case ESNSRTYP_DO:
        {
            fms_packet_header_t head;
            fclt_ctl_thrlddi_t  body;
            snsr_info_t snsrinfo;
            memset(&snsrinfo, 0, sizeof(snsrinfo));

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

            parse_rtupk_ctl_base(&body.ctl_base);
            parse_rtupk_thrlddatadi(&body.tthrlddtdi);

            // DB이력인풋 - 센서 아이디 검색 쉐이드 메모리에서 GET 변경 [10/26/2016 redIce] 
            if (get_db_col_info_shm(
                body.ctl_base.esnsrtype, body.ctl_base.esnsrsub, body.ctl_base.idxsub,
                &snsrinfo) != ERR_FAIL)
            {
                snsr_id = snsrinfo.snsr_id;
                // DB이력인풋 [10/24/2016 redIce]
                insert_db_eqp_ctl_hist(&body, snsr_id);
                insert_db_didothrld(&body, snsr_id);
            }
            else
            {
                print_dbg( DBG_ERR, 1, "SnsrId not found [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
            }

            ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
        }
        break;
    }

    return ERR_SUCCESS;
}

int ctl_eqp_completed_thrld(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;
    //int snsr_id = -1;

    print_dbg( DBG_NONE,1,"ctl_eqp_completed_thrld [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_ctl_thrldai_t , fclt_ctl_thrlddi_t
    {
        fms_packet_header_t      head;
        fclt_ctlthrldcompleted_t body;
        snsr_info_t snsrinfo;
        memset(&snsrinfo, 0, sizeof(snsrinfo));

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));
        memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

        parse_rtupk_fclt_ctlthrldcompleted(&body);

        // DB이력인풋 - 센서 아이디 검색 쉐이드 메모리에서 GET 변경 [10/26/2016 redIce] 
#if 0
        if (get_db_col_info_shm(
            body.esnsrtype, body.esnsrsub, body.idxsub,
            &snsrinfo) != ERR_FAIL)
        {
            snsr_id = snsrinfo.snsr_id;
            insert_db_eqp_ctl_hist(&body, snsr_id);
        }
#endif
        ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
        ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);

    }

    return ERR_SUCCESS;
}

int ctl_timedat_req(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;

    print_dbg( DBG_NONE,1,"ctl_timedat_req [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_reqtimedata_t
    {
        fms_packet_header_t head;
        fclt_reqtimedata_t  body;

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));
        memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

        parse_rtupk_fclt_reqtimedata(&body);

        ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
        ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
    }

    return ERR_SUCCESS;
}


int ctl_database_update_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;

    print_dbg( DBG_NONE,1,"ctl_database_update_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_evt_dbdataupdate_t
    {
        fms_packet_header_t     head;
        fclt_evt_dbdataupdate_t body;

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));
        memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

        parse_rtupk_fclt_evt_dbdataupdate(&body);

        // send_packet에서 처리 [10/28/2016 redIce]
        //set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);

        // 완료수신메시지를 받고 바로 리스폰 메시지발송
        ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
        ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
    }

    return ERR_SUCCESS;
}

int ctl_evt_network_err(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;

    print_dbg( DBG_NONE,1,"ctl_evt_network_err [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
                ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_evt_dbdataupdate_t
    {
        fms_packet_header_t     head;
        fclt_evt_faultfclt_t    body;
        db_syshist_t            syshist;

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));
        memset(&syshist, 0, sizeof(syshist));
        memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

        parse_rtupk_fclt_evt_faultfclt(&body);

        memcpy(&syshist.tfcltelem, &(body.tfcltelem), sizeof(syshist.tfcltelem));
        memcpy(&syshist.tdbtans, &(body.tdbtans), sizeof(syshist.tdbtans));
        syshist.errcd = body.ecnnstatus;
        memcpy(&syshist.ipaddr, ptrmsg->szdata, sizeof(syshist.ipaddr));
        syshist.port = ptrmsg->ndata;

        insert_db_sys_hist(&syshist);

        ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
        ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);

    }

    return ERR_SUCCESS;
}

int ctl_eqp_sts_resp(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;

    print_dbg( DBG_NONE,1,"ctl_eqp_sts_resp [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_reqtimedata_t
    {
        fms_packet_header_t head;
        fclt_ctlstatus_eqponoffrslt_t body;

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));
        memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

        parse_rtupk_eqp_sts_resp(&body);

        ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
        ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
    }

    return ERR_SUCCESS;

}

int ctl_eqp_net_sts_resp(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;

    print_dbg( DBG_NONE,1,"ctl_eqp_net_sts_resp [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_reqtimedata_t
    {
        fms_packet_header_t head;
        fclt_cnnstatus_eqprslt_t body;

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));
        memcpy(&body, ptrmsg->ptrbudy, sizeof(body));

        parse_rtupk_eqp_net_sts_resp(&body);

        ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
        ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
    }

    return ERR_SUCCESS;
}

int ctl_eqp_off_opt_resp(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;

    print_dbg( DBG_NONE,1,"ctl_eqp_off_opt_resp [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_reqtimedata_t
    {
        fms_packet_header_t head;
        fclt_ctrl_eqponoff_optionoff_t body;

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));
        memcpy(&body, ptrmsg->ptrbudy, sizeof(body));
        
		parse_rtupk_eqp_off_opt_resp(&body);

        ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
        ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
    }

    return ERR_SUCCESS;

}

int ctl_eqp_on_opt_resp(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm)
{
    inter_msg_t * ptrmsg = ( inter_msg_t * ) ptrdata;

    print_dbg( DBG_NONE,1,"ctl_eqp_on_opt_resp [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrcomm == NULL || ptrcomm->make_header == NULL || ptrcomm->send_packet == NULL || ptrcomm->ptrsock < 0)
    {
        if (ptrcomm == NULL)
            print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
        else
            print_dbg( DBG_ERR, 1, "Is null make_header(%p) send_packet(%p) ptrsock(%d) [%s:%d:%s]",
            ptrcomm->make_header, ptrcomm->send_packet, ptrcomm->ptrsock, __FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    // fclt_reqtimedata_t
    {
        fms_packet_header_t head;
        fclt_ctrl_eqponoff_optionon_t body;

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));
        memcpy(&body, ptrmsg->ptrbudy, sizeof(body));
        
		parse_rtupk_eqp_on_opt_resp(&body);

        ptrcomm->make_header(&head, ptrmsg->msg, sizeof(body));
        ptrcomm->send_packet(ptrcomm->ptrsock , &head, &body);
    }

    return ERR_SUCCESS;

}



int net_packet_parser(fms_packet_header_t* ptrhead, void * ptrbody, net_comm_t * ptrcomm)
{
    if ( ptrcomm == NULL || ptrcomm->send_ctlmsg == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrcomm [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    // OP 명령처리 상태확인 RTU 업데이트 , EQP 전원제어 (AUTO 전원제어 ndata = 9), 데이터베이스 업데이트 제어 [10/21/2016 redIce]
    if (get_op_cmdctl_state() == EUT_RTU_STATE_CONTROL)
    {
        if (is_op_cmdctl_req(ptrhead->shopcode) == 1 && ptrhead->shopcode == EFT_EQPCTL_POWER)
        {
            fms_packet_header_t      head;
            fclt_ctrl_eqponoffrslt_t body;
            print_dbg(DBG_INFO, 1, "########### op client command skip sock (%d) [%s:%d]",
                ptrcomm->ptrsock->clntsock, ptrcomm->ptrsock->clntip, ptrcomm->ptrsock->clntport);

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));

            print_dbg(DBG_INFO, 1, "@@@@@@@@@@@@@@ [PACKET RECV] EFT_EQPCTL_POWER [ SKIP !!! ] ( cmdctl state %d )", get_op_cmdctl_state());

            memcpy(&body.ctl_base, &(((fclt_ctrl_eqponoff_t *)(ptrbody))->ctl_base), sizeof(body.ctl_base));
            //body.ctl_base.tfcltelem.nsiteid = get_my_siteid_shm();
            //body.ctl_base.tfcltelem.nfcltid = get_my_rtu_fcltid_shm();
            //body.ctl_base.tfcltelem.efclt = EFCLT_RTU;
            //body.emaxstep = 0;
            //body.ecurstep = 0;
            body.ersult = EEQPONOFFRSLT_ERR_OVERLAPCMD;

            ptrcomm->make_header(&head, EFT_EQPCTL_RES, sizeof(body));
            ptrcomm->send_packet(ptrcomm->ptrsock, &head, &body);
            return ERR_SUCCESS;
        }

        if (is_op_cmdctl_req(ptrhead->shopcode) == 1 && ptrhead->shopcode == EFT_EVT_BASEDATUPDATE_REQ)
        {
            fms_packet_header_t     head;
            fclt_evt_dbdataupdate_t body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));

            memcpy(&body, ptrbody, ptrhead->nsize);

            if (body.ectlcmd == EFCLTCTLCMD_WORKREADY)
            {
                print_dbg(DBG_INFO, 1, "@@@@@@@@@@@@@@ [PACKET RECV] EFT_EVT_BASEDATUPDATE_REQ [ SKIP !!! ] ( cmdctl state %d ) (WORKREADY)", get_op_cmdctl_state());

                body.tfcltelem.nsiteid = get_my_siteid_shm();
                body.tfcltelem.nfcltid = get_my_rtu_fcltid_shm();
                body.tfcltelem.efclt = EFCLT_RTU;
                body.ectlcmd = EFCLTCTLCMD_WORKREADYFAIL;

                ptrcomm->make_header(&head, EFT_EVT_BASEDATUPDATE_RES, sizeof(body));
                ptrcomm->send_packet(ptrcomm->ptrsock, &head, &body);
                return ERR_SUCCESS;
            }
        }
    } // if (get_op_cmdctl_state() == EUT_RTU_STATE_CONTROL)



    // 패킷파싱
    switch(ptrhead->shopcode)
    {
    case ECMD_NONE:
        return ERR_SUCCESS;
    case EFT_NETALIVECHK_REQ:
        return ERR_SUCCESS;
    case EFT_NETALIVECHK_RES:
        {
            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_NETALIVECHK_RES");
        }
        return ERR_SUCCESS;
        //환경감시
    case EFT_SNSRDAT_RES: //RTU센서 모니터링 데이터 수집
        return ERR_SUCCESS;
    case EFT_SNSRDAT_RESACK:
        // 수신 확인 : FMS_SVR -> FMS_RTU
        {
            UINT16 cmd;
            fclt_svrtransactionack_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_SNSRDAT_RESACK");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);

            update_db_ai_hist(&body);
            update_db_dido_hist(&body);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_SNSRCUDAT_REQ: //RTU센서 모니터링 데이터 실시간 수집
        // FMS_OP --> FMS_RTU
        {
            UINT16 cmd;
            fclt_ctl_requestrsltdata_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_SNSRCUDAT_REQ");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_SNSRCUDAT_RES:
        return ERR_SUCCESS;
    case EFT_SNSREVT_FAULT: //장애(FAULT)이벤트
        return ERR_SUCCESS;
    case EFT_SNSREVT_FAULTACK:
        // 수신 확인 : FMS_SVR -> FMS_RTU
        {
            UINT16 cmd;
            fclt_svrtransactionack_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_SNSREVT_FAULTACK");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            update_db_event(&body);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_SNSRCTL_POWER: //전원제어 (ON/OFF)
        {
            UINT16 cmd;
            inter_msg_t inter_msg;
            fclt_ctrl_snsronoff_t body;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_SNSRCTL_POWER");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;

            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_SNSRCTL_POWERACK:
        // 수신 확인 : FMS_SVR -> FMS_RTU
        {
            UINT16 cmd;
            fclt_svrtransactionack_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_SNSRCTL_POWERACK");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            update_db_ctl_hist(&body);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_SNSRCTL_RES: //전원제어에 대한 결과
        return ERR_SUCCESS;
    case EFT_SNSRCTL_RESACK:
        // 수신 확인 : FMS_SVR -> FMS_RTU
        {
            UINT16 cmd;
            fclt_svrtransactionack_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_SNSRCTL_RESACK");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            update_db_ctl_hist(&body);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_SNSRCTL_THRLD: //RTU센서 임계기준 설정
        {
            UINT16 cmd;
            fclt_ctl_thrldai_t reqai;
            fclt_ctl_thrlddi_t reqdi;
            int datatype = 0;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_SNSRCTL_THRLD");

            //net_db_insert(NULL , server_net_sts);

            memset(&reqai, 0, sizeof(reqai));
            memset(&reqdi, 0, sizeof(reqdi));
            memset(&inter_msg, 0, sizeof(inter_msg));

            datatype = parse_rtupk_get_snsrtype(ptrbody, 0);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;

            if (datatype == ESNSRTYP_AI)
            {
                memcpy(&reqai, ptrbody, ptrhead->nsize);
                inter_msg.ptrbudy = &reqai;
            }
            else
            {
                memcpy(&reqdi, ptrbody, ptrhead->nsize);
                inter_msg.ptrbudy = &reqdi;
            }
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_SNSRCTL_THRLDACK:
        // 수신 확인 : FMS_SVR -> FMS_RTU
        {
            UINT16 cmd;
            fclt_svrtransactionack_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_SNSRCTL_THRLDACK");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            update_db_aithrld(&body);
            update_db_didothrld(&body);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_SNSRCTLCOMPLETED_THRLD:
        return ERR_SUCCESS;
        //감시장비 연계
    case EFT_EQPDAT_REQ: //감시장비 모니터링 데이터
        return ERR_SUCCESS;
    case EFT_EQPDAT_RES:
        return ERR_SUCCESS;
    case EFT_EQPDAT_RESACK:
        // 수신 확인 : FMS_SVR -> FMS_RTU
        {
            UINT16 cmd;
            fclt_svrtransactionack_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EQPDAT_RESACK");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            update_db_eqp_hist(&body);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_EQPDAT_ACK: //장비 : RTU가 감시장비 모니터링 결과 수신 했음을 확인
        return ERR_SUCCESS;
    case EFT_EQPCUDAT_REQ: //상태 정보(실시간) 송신 프로세스 시작/종료
        // FMS_OP --> FMS_RTU
        {
            UINT16 cmd;
            fclt_ctl_requestrsltdata_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EQPCUDAT_REQ");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_EQPCUDAT_RES:
        return ERR_SUCCESS;
    case EFT_EQPEVT_FAULT: //장애(FAULT)이벤트
        return ERR_SUCCESS;
    case EFT_EQPEVT_FAULTACK:
        // 수신 확인 : FMS_SVR -> FMS_RTU
        {
            UINT16 cmd;
            fclt_svrtransactionack_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EQPEVT_FAULTACK");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            update_db_event(&body);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_EQPCTL_POWER: //감시장비 전원제어 (ON/OFF)
        {
            UINT16 cmd;
            fclt_ctrl_eqponoff_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EQPCTL_POWER");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);

            set_op_cmdctl_state(EUT_RTU_STATE_CONTROL, 0);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            if (ptrcomm->send_ctlmsg(cmd, &inter_msg) != 0)
            {
                set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
            }
        }
        return ERR_SUCCESS;
    case EFT_EQPCTL_POWERACK: //감시장비 전원제어
        // 수신 확인 : FMS_SVR -> FMS_RTU
        {
            UINT16 cmd;
            fclt_svrtransactionack_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EQPCTL_POWERACK");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            update_db_ctl_hist(&body);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_EQPCTL_RES: //전원제어에 대한 진행상태 정보
        return ERR_SUCCESS;
    case EFT_EQPCTL_RESACK:
        // 수신 확인 : FMS_SVR -> FMS_RTU
        {
            UINT16 cmd;
            fclt_svrtransactionack_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EQPCTL_RESACK");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            update_db_ctl_hist(&body);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_EQPCTL_THRLD: //감시장비 항목별 임계 설정
        {
            UINT16 cmd;
            fclt_ctl_thrlddi_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EQPCTL_THRLD");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_EQPCTL_THRLDACK:
        // 수신 확인 : FMS_SVR -> FMS_RTU
        {
            UINT16 cmd;
            fclt_svrtransactionack_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EQPCTL_THRLDACK");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            update_db_didothrld(&body);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_EQPCTLCOMPLETED_THRLD:
        return ERR_SUCCESS;
        //기타 - 시간동기화
    case EFT_TIMEDAT_REQ: //시간동기화 : RTU - SVR - OP 간 시간 동기화
        return ERR_SUCCESS;
    case EFT_TIMEDAT_RES:
        // FMS_SVR --> FMS_RTU, FMS_FOP --> FMS_RTU
        {
            UINT16 cmd;
            fclt_timersltdata_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_TIMEDAT_RES");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_EVT_BASEDATUPDATE_REQ: //공통으로 사용하는 DB 기본테이블(RTU, 감시장비, 센서, 사용자, 공통코드) 등이 변경된 경우 RTU 에게 알려준다.
        // FMS_OP, FMS_SVR --> FMS_RTU
        {
            UINT16 cmd;
            fms_packet_header_t     head;
            fclt_evt_dbdataupdate_t body;
            inter_msg_t inter_msg;

            // 데이터베이스 업데이트 시나리오 변경 [10/21/2016 redIce]
            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);

            set_op_cmdctl_state(EUT_RTU_STATE_CONTROL, 0);

            if (body.ectlcmd == EFCLTCTLCMD_WORKREADY)
            {
                print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EVT_BASEDATUPDATE_REQ -> EFCLTCTLCMD_WORK_READY");

                body.tfcltelem.nsiteid = get_my_siteid_shm();
                body.tfcltelem.nfcltid = get_my_rtu_fcltid_shm();
                body.tfcltelem.efclt = EFCLT_RTU;
                body.ectlcmd = EFCLTCTLCMD_WORKREADYEND;

                ptrcomm->make_header(&head, EFT_EVT_BASEDATUPDATE_RES, sizeof(body));
                ptrcomm->send_packet(ptrcomm->ptrsock, &head, &body);
            }
            else if (body.ectlcmd == EFCLTCTLCMD_WORKRUN)
            {
                print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EVT_BASEDATUPDATE_REQ -> EFCLTCTLCMD_WORK_RUN");

                cmd = ptrhead->shopcode;
                inter_msg.client_id = ptrcomm->ptrsock->clntsock;
                inter_msg.msg = cmd;
                inter_msg.ptrbudy = &body;
                if (ptrcomm->send_ctlmsg(cmd, &inter_msg) != 0)
                {
                    set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
                }
            }
            else
            {
                print_dbg(DBG_ERR, 1, "database update cmd error ... [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
            }
        }
        return ERR_SUCCESS;
    case EFT_EVT_BASEDATUPDATE_RES:
        return ERR_SUCCESS;
        //기타 - firmware update
    case EFT_EVT_FIRMWAREUPDATE_REQ:
        {
            set_op_cmdctl_state(EUT_RTU_STATE_UPDATE, 0);
        }
        return ERR_SUCCESS;
    case EFT_EVT_FIRMWAREUPDATE_NOTI:
        return ERR_SUCCESS;
        // 설비(고정감시 운영SW 가 구동되는 PC) IP/PORT 정보를 요청
    case EFT_FCLTNETINFO_REQ:
        {
            UINT16 cmd;
            fclt_evt_fcltnetworkinfo_req_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_FCLTNETINFO_REQ");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
    case EFT_FCLTNETINFO_RES:
        return ERR_SUCCESS;
        //네트워크 장애
    case EFT_EVT_FCLTNETWORK_ERR:
        return ERR_SUCCESS;
    case EFT_EVT_FCLTNETWORK_ERRACK:
        {
            UINT16 cmd;
            fclt_svrtransactionack_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EVT_FCLTNETWORK_ERRACK");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);
            update_db_sys_hist(&body);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
        }
        return ERR_SUCCESS;
	
	/* 2016 11 14 NEW Protocol */
	case EFT_EQPCTL_POWEROPT_OFF:
		{
            UINT16 cmd;
            fclt_ctrl_eqponoff_optionoff_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EQP POWER OFF OPTION");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);

		}
		return ERR_SUCCESS;

	case EFT_EQPCTL_POWEROPT_ON:
		{
            UINT16 cmd;
            fclt_ctrl_eqponoff_optionon_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EQP POWER ON OPTION");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
		}
		return ERR_SUCCESS;
	/* req rtu version */
	case EFT_FIRMWARE_VERSION_REQ:
		{
			fms_packet_header_t     head;
			fclt_firmware_versionrslt_t body;
			ver_info_t * ptrver = NULL;

			memset(&head, 0, sizeof(head));
			memset(&body, 0, sizeof(body));
			
			ptrver = get_rtu_version_shm();
			if ( ptrver == NULL )
			{
				print_dbg( DBG_ERR, 1, "pterver is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
				return ERR_RETURN;
			}

			print_dbg(DBG_INFO, 1, "[PACKET RECV] EFT_FIRMWARE_VERSION_REQ : Cur Version:[%d.%d]", 
									ptrver->app_ver[ 0 ], ptrver->app_ver[ 1 ] );

			body.nsiteid = get_my_siteid_shm();
			sprintf( body.szversion, "%d.%d", ptrver->app_ver[ 0 ], ptrver->app_ver[ 1 ] );

			ptrcomm->make_header(&head, EFT_FIRMWARE_VERSION_RES, sizeof(body));
			ptrcomm->send_packet(ptrcomm->ptrsock, &head, &body);
		}

		return ERR_SUCCESS;
	/* req rtu info */
	case EFT_RTUSETTING_INFO_REQ:
		{
			fms_packet_header_t     head;

			fclt_rtusetting_info_req_t body;
			fclt_rtusetting_inforslt_t body2;
			net_info_t * ptrnet= NULL;

			memset(&head, 0, sizeof(head));
			memset(&body, 0, sizeof(body));
			memset(&body2, 0, sizeof(body2));
            
			memcpy(&body, ptrbody, ptrhead->nsize);
			ptrnet = get_rtu_net_shm();
			if ( ptrnet == NULL )
			{
				print_dbg( DBG_ERR, 1, "ptrnet is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
				return ERR_RETURN;
			}


			body2.nsiteid = body.nsiteid;
			
			memcpy( body2.szip, ptrnet->ip, MAX_IP_SIZE );
			memcpy( body2.szsubnetmask, ptrnet->netmask, MAX_IP_SIZE );
			memcpy( body2.szgateway, ptrnet->gateway, MAX_IP_SIZE );
			body2.nsiteid_rslt = get_my_siteid_shm();

			ptrcomm->make_header(&head, EFT_RTUSETTING_INFO_RES, sizeof(body2));
			ptrcomm->send_packet(ptrcomm->ptrsock, &head, &body2);

			print_dbg(DBG_INFO, 1, "[PACKET RECV] EFT_RTUSETTING_INFO_REQ: RECV SITEID:%d, RTU SITEID:%d, IP:%s, Gateway:%s, Netmask:%s", 
									body.nsiteid, body2.nsiteid_rslt, ptrnet->ip, ptrnet->gateway, ptrnet->netmask );

		}
		return ERR_SUCCESS;
	
		/* req eqp status  */
	case EFT_EQPCTL_STATUS_REQ:
		{
            UINT16 cmd;
            fclt_ctlstatus_eqponoff_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EQPCTL_STATUS_REQ");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
		}
		return ERR_SUCCESS;

	case EFT_EQPCTL_CNNSTATUS_REQ:
		{
            UINT16 cmd;
            fclt_cnnstatus_eqprslt_t body;
            inter_msg_t inter_msg;

            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_EQPCTL_CNNSTATUS_REQ");

            memset(&body, 0, sizeof(body));
            memset(&inter_msg, 0, sizeof(inter_msg));

            memcpy(&body, ptrbody, ptrhead->nsize);

            cmd = ptrhead->shopcode;
            inter_msg.client_id = ptrcomm->ptrsock->clntsock;
            inter_msg.msg = cmd;
            inter_msg.ptrbudy = &body;
            ptrcomm->send_ctlmsg(cmd, &inter_msg);
		}
		return ERR_SUCCESS;

    }

    return ERR_RETURN;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// data base


int net_close_db()
{
    int i;
    for (i = 0; i < EDB_MAX_COUNT; i++)
    {
        if (g_ptrconn[i] != NULL)
        {
            mysql_close( g_ptrconn[i] );
            g_ptrconn[i] = NULL;
        }
    }

    return ERR_SUCCESS;
}

int net_connect_db()
{
    db_info_t db_info;
    int ret = ERR_SUCCESS;
    int i;

    memset(&db_info, 0, sizeof(db_info));

    get_db_info_shm(&db_info);

    if (strlen( db_info.szdbuser ) == 0 ||
        strlen( db_info.szdbuser ) == 0 ||
        strlen( db_info.szdbuser ) == 0  )
    {
        print_dbg( DBG_ERR, 1, "Invalid DB Info User:%s, Pwr:%s, Name:%s",
            db_info.szdbuser, db_info.szdbpwr, db_info.szdbname );
        return ERR_RETURN;
    }
#if 0
    print_dbg( DBG_INFO, 1, "DB Info User:%s, Pwr:%s, Name:%s",
        db_info.szdbuser, db_info.szdbpwr, db_info.szdbname );
#endif

    memset(g_ptrconn, 0, sizeof(g_ptrconn));

    for (i = 0; i < EDB_MAX_COUNT; i++)
    {
        if ( (g_ptrconn[i] = mysql_init( NULL )) == NULL )
        {
            print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
            return ERR_RETURN;
        }

        mysql_options(g_ptrconn[i], MYSQL_OPT_LOCAL_INFILE, (void *)"1"); /* mysql reconnect enable mysql_ping() */ 

        if (mysql_real_connect( g_ptrconn[i], "127.0.0.1", db_info.szdbuser, db_info.szdbpwr, db_info.szdbname, 3306, NULL, 0) == NULL)
        {
            print_dbg( DBG_ERR,1, "Cannot Connect LocalDB [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
            return ERR_RETURN;
        }
    }

    return ret ;
}

static int db_mysqlping(int edbconnidx)
{
    if (g_ptrconn[edbconnidx] == NULL)
    {
        return ERR_FAIL;
    }

    if (mysql_ping(g_ptrconn[edbconnidx]) != 0)
    {
        db_info_t db_info;

        print_dbg(DBG_ERR, 1, "mysql unconnector (%d) ...[%s:%d:%s]", edbconnidx ,__FILE__, __LINE__, __FUNCTION__);

        memset(&db_info, 0, sizeof(db_info));

        get_db_info_shm(&db_info);

        if (strlen( db_info.szdbuser ) == 0 ||
            strlen( db_info.szdbuser ) == 0 ||
            strlen( db_info.szdbuser ) == 0  )
        {
            print_dbg( DBG_ERR, 1, "Invalid DB Info User:%s, Pwr:%s, Name:%s",
                db_info.szdbuser, db_info.szdbpwr, db_info.szdbname );
            return ERR_RETURN;
        }

        if (g_ptrconn[edbconnidx] != NULL)
        {
            mysql_close( g_ptrconn[edbconnidx] );
            g_ptrconn[edbconnidx] = NULL;
        }

        if ((g_ptrconn[edbconnidx] = mysql_init( NULL )) == NULL)
        {
            print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
            return ERR_RETURN;
        }

        if (mysql_real_connect(g_ptrconn[edbconnidx], "127.0.0.1", db_info.szdbuser, db_info.szdbpwr, db_info.szdbname, 3306, NULL, 0) == NULL)
        {
            print_dbg( DBG_ERR,1, "Cannot Connect LocalDB [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
            return ERR_RETURN;
        }

        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////

int select_snsr_id(int nsiteid, int nfcltid, int esnsrtype, int esnsrsub, int idxsub)
{
    MYSQL_RES * ptrsql_result = NULL;
    MYSQL_ROW row;
    int snsr_id;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    sprintf(szsql, "\
select SNSR_ID from TFMS_SNSR_MST \
where FCLT_ID='%04d' and SITE_ID='%04d' and SNSR_IDX='%d' \
and SNSR_TYPE='%02d' and SNSR_SUB_TYPE='%03d' and USE_YN = 'Y' limit 1"
                   , nfcltid
                   , nsiteid
                   , idxsub
                   , esnsrtype
                   , esnsrsub
                   );

    if (db_mysqlping(EDB_CONN_SSRMONITOR) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_SSRMONITOR], szsql) != 0)
    {
        print_dbg( DBG_INFO,1, "Cannot Select %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_SSRMONITOR]), __FILE__, __LINE__,__FUNCTION__);
        return ERR_FAIL;
    }

    ptrsql_result = mysql_store_result( g_ptrconn[EDB_CONN_SSRMONITOR] );	

    if (ptrsql_result == NULL)
    {
        print_dbg( DBG_INFO,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
        return ERR_FAIL;
    }

    if ((row = mysql_fetch_row(ptrsql_result)))
    {
        snsr_id = atoi(row[0]);
    }
    else
    {
        print_dbg( DBG_INFO,1, "Cannot SnsrID Fild [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
        return ERR_FAIL;
    }

    return snsr_id;
}


// 제어 히스토리
int update_db_ctl_hist(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
update TFMS_CTLHIST_DTL set \
TRANS_CHK='Y' \
where CTL_ID='%s'"
                   , ptr->tdbtans.sdbfclt_tid
           );

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

int insert_db_eqp_ctl_hist_onoff(void * ptrdata, int snsr_id)
{
    fclt_ctrl_eqponoff_t * ptr = (fclt_ctrl_eqponoff_t *) ptrdata;
    char suserid[MAX_PATH_SIZE], *ptrcov_suserid;
    char *ptreuc_suserid;
    iconv_t cd;
    size_t neucsuserid, ncovsuserid;
	
    int status = 1; //제어상태
    int resval = 0; //응답값  (알림용)
    int reqcd  = 0; //요청코드(알림용)
    static char szsql[ MAX_SQL_SIZE ];
	char szip[ MAX_IP_SIZE ];

	UINT32 op_ip = 0;

    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if ((cd = iconv_open("UTF-8" , "EUC-KR")) == (iconv_t)(-1))
    {
        print_dbg(DBG_ERR, 1, "iconv open error... [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    neucsuserid 		= strlen(ptr->ctl_base.suserid);
    ncovsuserid 		= sizeof(suserid);
    ptreuc_suserid 		= ptr->ctl_base.suserid;
    ptrcov_suserid 		= suserid;
	op_ip 				= ptr->ctl_base.op_ip;
	
	memset( szip, 0, sizeof( szip ));
	cnv_addr_to_strip( op_ip, szip );

    if (iconv(cd, &ptreuc_suserid, &neucsuserid, &ptrcov_suserid, &ncovsuserid) < 0)
    {
        print_dbg(DBG_INFO, 1, "iconv fail euckr -> utf8 [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    iconv_close(cd);

    sprintf(szsql, "\
insert into TFMS_CTLHIST_DTL \
(CTL_ID , CTL_TIME \
, SNSR_ID , CTL_TYPE_CD \
, CTL_CMD_CD , CTL_STATUS , RES_VAL \
, USER_ID , OP_IP \
, REQ_CD , TRANS_CHK) \
values \
('%s' , from_unixtime('%u') \
, '%04d' , '%02d' \
, '%02d' , '%d' , '%d' \
, '%s', '%s' \
, '%d' , 'N')"
                   , ptr->ctl_base.tdbtans.sdbfclt_tid, ptr->ctl_base.tdbtans.tdatetime
                   , snsr_id, ptr->ctl_base.ectltype
                   , ptr->ctl_base.ectlcmd, status, resval
                   , suserid
				   , szip 
                   , reqcd
                   );

    if (db_mysqlping(EDB_CONN_CONTROL) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_CONTROL], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_CONTROL]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

int insert_db_eqp_ctl_hist_onoffrslt(void * ptrdata, int snsr_id)
{
    fclt_ctrl_eqponoffrslt_t * ptr = (fclt_ctrl_eqponoffrslt_t *) ptrdata;
    char suserid[MAX_PATH_SIZE], *ptrcov_suserid;
    char *ptreuc_suserid;
    iconv_t cd;
    size_t neucsuserid, ncovsuserid;

    char status[7] = {0,}; //제어상태
    int resval = 0; //응답값  (알림용)
    int reqcd  = 0; //요청코드(알림용)
    static char szsql[ MAX_SQL_SIZE ];
	UINT32 op_ip =0;
	char szip[ MAX_IP_SIZE ];

    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if ((cd = iconv_open("UTF-8" , "EUC-KR")) == (iconv_t)(-1))
    {
        print_dbg(DBG_ERR, 1, "iconv open error... [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    neucsuserid 	= strlen(ptr->ctl_base.suserid);
    ncovsuserid 	= sizeof(suserid);
    ptreuc_suserid 	= ptr->ctl_base.suserid;
    ptrcov_suserid 	= suserid;
	op_ip			= ptr->ctl_base.op_ip;
	resval			= ptr->ersult;
	//resval			= -1;
	memset( szip, 0, sizeof( szip ));
	cnv_addr_to_strip( op_ip, szip );

    
	sprintf(status, "%02d,%02d", ptr->emaxstep, ptr->ecurstep);

    if (iconv(cd, &ptreuc_suserid, &neucsuserid, &ptrcov_suserid, &ncovsuserid) < 0)
    {
        print_dbg(DBG_INFO, 1, "iconv fail euckr -> utf8 [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    iconv_close(cd);

    sprintf(szsql, "\
insert into TFMS_CTLHIST_DTL \
(CTL_ID , CTL_TIME \
, SNSR_ID , CTL_TYPE_CD \
, CTL_CMD_CD , CTL_STATUS , RES_VAL \
, USER_ID, OP_IP \
, REQ_CD , TRANS_CHK) \
values \
('%s' , from_unixtime('%u') \
, '%04d' , '%02d' \
, '%02d' , '%s' , '%d' \
, '%s' , '%s' \
, '%d' , 'N')"
                   , ptr->ctl_base.tdbtans.sdbfclt_tid, ptr->ctl_base.tdbtans.tdatetime
                   , snsr_id, ptr->ctl_base.ectltype
                   , ptr->ctl_base.ectlcmd, status, resval
                   , suserid
                   , szip 
                   , reqcd
                   );
//printf("===%s==\r\n", szsql );
    if (db_mysqlping(EDB_CONN_CONTROL) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_CONTROL], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_CONTROL]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

int insert_db_sns_ctl_hist_onoff(void * ptrdata, int snsr_id)
{
    fclt_ctrl_snsronoff_t * ptr = (fclt_ctrl_snsronoff_t *) ptrdata;
    char suserid[MAX_PATH_SIZE], *ptrcov_suserid;
    char *ptreuc_suserid;
    iconv_t cd;
    size_t neucsuserid, ncovsuserid;

    int status = 1; //제어상태
    int resval = 0; //응답값  (알림용)
    int reqcd  = 0; //요청코드(알림용)
    static char szsql[ MAX_SQL_SIZE ];
	UINT32 op_ip =0;
	
	char szip[ MAX_IP_SIZE ];

    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if ((cd = iconv_open("UTF-8" , "EUC-KR")) == (iconv_t)(-1))
    {
        print_dbg(DBG_ERR, 1, "iconv open error... [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    neucsuserid 	= strlen(ptr->ctl_base.suserid);
    ncovsuserid 	= sizeof(suserid);
    ptreuc_suserid 	= ptr->ctl_base.suserid;
    ptrcov_suserid 	= suserid;
	op_ip			= ptr->ctl_base.op_ip;

	memset( szip, 0, sizeof( szip ));
	cnv_addr_to_strip( op_ip, szip );

    if (iconv(cd, &ptreuc_suserid, &neucsuserid, &ptrcov_suserid, &ncovsuserid) < 0)
    {
        print_dbg(DBG_INFO, 1, "iconv fail euckr -> utf8 [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    iconv_close(cd);

    sprintf(szsql, "\
insert into TFMS_CTLHIST_DTL \
(CTL_ID , CTL_TIME \
, SNSR_ID , CTL_TYPE_CD \
, CTL_CMD_CD , CTL_STATUS , RES_VAL \
, USER_ID, OP_IP \
, REQ_CD , TRANS_CHK) \
values \
('%s' , from_unixtime('%u') \
, '%04d' , '%02d' \
, '%02d' , '%d' , '%d' \
, '%s' , '%s' \
, '%d' , 'N')"
                   , ptr->ctl_base.tdbtans.sdbfclt_tid, ptr->ctl_base.tdbtans.tdatetime
                   , snsr_id, ptr->ctl_base.ectltype
                   , ptr->ctl_base.ectlcmd, status, resval
                   , suserid
                   , szip 
                   , reqcd
                   );

    if (db_mysqlping(EDB_CONN_CONTROL) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }
	
	//printf("kk%skk\r\n", szsql );

    if (mysql_query(g_ptrconn[EDB_CONN_CONTROL], szsql) != 0 )
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_CONTROL]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

int insert_db_sns_ctl_hist(void * ptrdata, int snsr_id)
{
    fms_ctl_base_t * ptr = (fms_ctl_base_t *) ptrdata;
    char suserid[MAX_PATH_SIZE], *ptrcov_suserid;
    char *ptreuc_suserid;
    iconv_t cd;
    size_t neucsuserid, ncovsuserid;

    int status = 1; //제어상태
    int resval = 0; //응답값  (알림용)
    int reqcd  = 0; //요청코드(알림용)
    static char szsql[ MAX_SQL_SIZE ];
	UINT32 op_ip = 0;
	char szip[ MAX_IP_SIZE ];

    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if ((cd = iconv_open("UTF-8" , "EUC-KR")) == (iconv_t)(-1))
    {
        print_dbg(DBG_ERR, 1, "iconv open error... [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    neucsuserid 	= strlen(ptr->suserid);
    ncovsuserid 	= sizeof(suserid);
    ptreuc_suserid 	= ptr->suserid;
    ptrcov_suserid 	= suserid;
	op_ip			= ptr->op_ip; 

	memset( szip, 0, sizeof( szip ));
	cnv_addr_to_strip( op_ip, szip );

    if (iconv(cd, &ptreuc_suserid, &neucsuserid, &ptrcov_suserid, &ncovsuserid) < 0)
    {
        print_dbg(DBG_INFO, 1, "iconv fail euckr -> utf8 [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    iconv_close(cd);

    sprintf(szsql, "\
insert into TFMS_CTLHIST_DTL \
(CTL_ID , CTL_TIME \
, SNSR_ID , CTL_TYPE_CD \
, CTL_CMD_CD , CTL_STATUS , RES_VAL \
, USER_ID , OP_IP\
, REQ_CD , TRANS_CHK) \
values \
('%s' , from_unixtime('%u') \
, '%04d' , '%02d' \
, '%02d' , '%d' , '%d' \
, '%s', '%s' \
, '%d' , 'N')"
                   , ptr->tdbtans.sdbfclt_tid, ptr->tdbtans.tdatetime
                   , snsr_id, ptr->ectltype
                   , ptr->ectlcmd, status, resval
                   , suserid
                   , szip 
                   , reqcd
                   );

    print_dbg(DBG_INFO, 1, "@@@@@@@ mysql sql %s [%s:%d:%s]", szsql, __FILE__, __LINE__, __FUNCTION__);

    if (db_mysqlping(EDB_CONN_CONTROL) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_CONTROL], szsql) != 0 )
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_CONTROL]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}


int insert_db_eqp_ctl_hist(void * ptrdata, int snsr_id)
{
    return insert_db_sns_ctl_hist(ptrdata, snsr_id);
}


// ai임계치설정 , 히스토리
int update_db_aithrld(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    static char szbuf[ 5 ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    memset(szbuf, 0, sizeof(szbuf));
    memcpy(szbuf, &(ptr->tdbtans.sdbfclt_tid[20]), sizeof(szbuf));

    sprintf(szsql, "\
update TFMS_AITHRLD_DTL set \
TRANS_CHK='Y' \
where SNSR_ID='%s'"
                   , szbuf
                   );

    print_dbg(DBG_INFO, 1, "@@@@@@@ mysql sql %s [%s:%d:%s]", szsql, __FILE__, __LINE__, __FUNCTION__);

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    update_db_aithrld_hist(ptrdata);

    return ERR_SUCCESS;
}

int insert_db_aithrld(void * ptrdata, int snsr_id)
{
    fclt_ctl_thrldai_t * ptr = (fclt_ctl_thrldai_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
update TFMS_AITHRLD_DTL \
set MIN_VAL='%0.2f' \
, DA_LOW ='%0.2f', WA_LOW ='%0.2f', CA_LOW ='%0.2f' \
, CA_HIGH='%0.2f', WA_HIGH='%0.2f', DA_HIGH='%0.2f' \
, MAX_VAL='%0.2f' \
, OFFSET='%0.2f', TRANS_CHK='N' \
where SNSR_ID='%04d'"
                   , ptr->tthrlddtai.fmin
                   , ptr->tthrlddtai.frange[0], ptr->tthrlddtai.frange[1], ptr->tthrlddtai.frange[2]
                   , ptr->tthrlddtai.frange[3], ptr->tthrlddtai.frange[4], ptr->tthrlddtai.frange[5]
                   , ptr->tthrlddtai.fmax
                   , ptr->tthrlddtai.foffsetval
                   , snsr_id
                   );

    print_dbg(DBG_INFO, 1, "@@@@@@@ mysql sql %s [%s:%d:%s]", szsql, __FILE__, __LINE__, __FUNCTION__);

    if (db_mysqlping(EDB_CONN_SSRMONITOR) != ERR_SUCCESS)
    {
       print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_SSRMONITOR], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_SSRMONITOR]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    insert_db_aithrld_hist(ptrdata, snsr_id);

    return ERR_SUCCESS;
}

int update_db_aithrld_hist(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
update TFMS_AITHRLDHIST_DTL set \
TRANS_CHK='Y' \
where CTL_ID='%s'"
                   , ptr->tdbtans.sdbfclt_tid
                   );

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

int insert_db_aithrld_hist(void * ptrdata, int snsr_id)
{
    fclt_ctl_thrldai_t * ptr = (fclt_ctl_thrldai_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
insert into TFMS_AITHRLDHIST_DTL \
(CTL_ID , CTL_TIME \
, SNSR_ID , MIN_VAL \
, DA_LOW  , WA_LOW  , CA_LOW \
, CA_HIGH , WA_HIGH , DA_HIGH \
, MAX_VAL , OFFSET , TRANS_CHK) \
values \
('%s', from_unixtime('%u') \
, '%04d' , '%0.2f' \
, '%0.2f' , '%0.2f' , '%0.2f' \
, '%0.2f' , '%0.2f' , '%0.2f' \
, '%0.2f' , '%0.2f' , 'N')"
                   , ptr->ctl_base.tdbtans.sdbfclt_tid, ptr->ctl_base.tdbtans.tdatetime
                   , snsr_id, ptr->tthrlddtai.fmin
                   , ptr->tthrlddtai.frange[0], ptr->tthrlddtai.frange[1], ptr->tthrlddtai.frange[2]
                   , ptr->tthrlddtai.frange[3], ptr->tthrlddtai.frange[4], ptr->tthrlddtai.frange[5]
                   , ptr->tthrlddtai.fmax, ptr->tthrlddtai.foffsetval
        );

    //printf("####### mysql sql %s [%s:%d:%s]\n", szsql, __FILE__, __LINE__, __FUNCTION__);
    print_dbg(DBG_INFO, 1, "@@@@@@@ mysql sql %s [%s:%d:%s]", szsql, __FILE__, __LINE__, __FUNCTION__);

    if (db_mysqlping(EDB_CONN_SSRMONITOR) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_SSRMONITOR], szsql ) != 0 )
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_SSRMONITOR]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}


// 이벤트 히스토리
int update_db_event(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
update TFMS_EVENT_DTL set \
TRANS_CHK='Y' \
where EVT_ID='%s'"
                   , ptr->tdbtans.sdbfclt_tid
                   );

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    update_db_sys_hist(ptrdata);

    return ERR_SUCCESS;
}


int insert_db_event(int snsrtype, void * ptrdata, int* snsr_id, int snsr_cnt, char * szdata, int ndata)
{
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));
	int fcltid = 0;

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if (snsrtype == ESNSRTYP_AI)
    {
        fclt_evt_faultai_t * ptr = (fclt_evt_faultai_t *) ptrdata;
        int i;

        if (snsr_cnt < ptr->fault_aidi.nfcltsubcount)
        {
            print_dbg(DBG_ERR, 1, "SNSR_CNT < FCLTSUB_CNT [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
            return ERR_FAIL;
        }
		
		fcltid = ptr->fault_aidi.tfcltelem.nfcltid;

        for (i = 0; i < ptr->fault_aidi.nfcltsubcount; i++)
        {
            sprintf(szsql, "\
insert into TFMS_EVENT_DTL \
(EVT_ID , DETECT_TIME \
, FCLT_ID \
, SNSR_ID , SNSR_VAL \
, EVT_GRADE_CD \
, EVT_STATUS_CD \
, FAULT_CD \
, ALARM_YN , TRANS_CHK) \
values \
('%s' , from_unixtime('%u') \
, '%04d' \
, '%04d' , '%0.2f' \
, '%02d' \
, '%02d' \
, '%02d' \
, '%c' , 'N')"
                           , ptr->fault_aidi.tdbtans.sdbfclt_tid, ptr->fault_aidi.tdbtans.tdatetime
                           , ptr->fault_aidi.tfcltelem.nfcltid
                           , snsr_id[i], ptr->rslt[i].frslt
                           , ptr->rslt[i].egrade
                           , ptr->rslt[i].egrade != 0 ? 1 : 0 //진행상태 0 복구
                           , ptr->rslt[i].ecnnstatus
                           , ptr->rslt[i].egrade != 0 ? 'Y':'N'
                           );
        }
    }
    else
    {
        fclt_evt_faultdi_t * ptr = (fclt_evt_faultdi_t *) ptrdata;
        int i;

        if (snsr_cnt < ptr->fault_aidi.nfcltsubcount)
        {
            print_dbg(DBG_ERR, 1, "SNSR_CNT < FCLTSUB_CNT [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
            return ERR_FAIL;
        }

		fcltid = ptr->fault_aidi.tfcltelem.nfcltid;
        for (i = 0; i < ptr->fault_aidi.nfcltsubcount; i++)
        {
            sprintf(szsql, "\
insert into TFMS_EVENT_DTL \
(EVT_ID , DETECT_TIME \
, FCLT_ID \
, SNSR_ID , SNSR_VAL \
, EVT_GRADE_CD \
, EVT_STATUS_CD \
, FAULT_CD \
, ALARM_YN , TRANS_CHK) \
values \
('%s' , from_unixtime('%u') \
, '%04d' \
, '%04d' , '%02d' \
, '%02d' \
, '%02d' \
, '%02d' \
, '%c' , 'N')"
                           , ptr->fault_aidi.tdbtans.sdbfclt_tid, ptr->fault_aidi.tdbtans.tdatetime
                           , ptr->fault_aidi.tfcltelem.nfcltid
                           , snsr_id[i], ptr->rslt[i].brslt
                           , ptr->rslt[i].egrade
                           , ptr->rslt[i].egrade != 0 ? 1 : 0 //진행상태 0 복구
                           , ptr->rslt[i].ecnnstatus
                           , ptr->rslt[i].egrade != 0 ? 'Y':'N'
                           );
        }
    }

    if (db_mysqlping(EDB_CONN_CONTROL) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_CONTROL], szsql ) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s,fcltid:%d[%s:%d:%s], ", mysql_error(g_ptrconn[EDB_CONN_CONTROL]),fcltid,  __FILE__, __LINE__, __FUNCTION__ );
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}


// dido임계치설정 , 히스토리
int update_db_didothrld(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    static char szbuf[ 5 ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    memset(szbuf, 0, sizeof(szbuf));
    memcpy(szbuf, &(ptr->tdbtans.sdbfclt_tid[20]), sizeof(szbuf));

    sprintf(szsql, "\
update TFMS_DIDOTHRLD_DTL set \
TRANS_CHK='Y' \
where SNSR_ID='%s'"
                   , szbuf
                   );

    print_dbg(DBG_INFO, 1, "@@@@@@@ mysql sql %s [%s:%d:%s]", szsql, __FILE__, __LINE__, __FUNCTION__);

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return update_db_didothrld_hist(ptrdata);
}

int insert_db_didothrld(void * ptrdata, int snsr_id)
{
    fclt_ctl_thrlddi_t * ptr = (fclt_ctl_thrlddi_t *) ptrdata;
    static char utf_snormalname[MAX_PATH_SIZE], *ptrcov_snormalname;
    static char utf_sfaultname [MAX_PATH_SIZE], *ptrcov_sfaultname;
    static char euc_snormalname[SZ_MAX_NAME  ], *ptreuc_snormalname;	// ON 이름
    static char	euc_sfaultname [SZ_MAX_NAME  ], *ptreuc_sfaultname;	    // OFF 이름
    iconv_t cd;
    size_t neucsnormal, ncovsnormal;
    size_t neucsfault , ncovsfault;

    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if ((cd = iconv_open("UTF-8" , "EUC-KR")) == (iconv_t)(-1))
    {
        print_dbg(DBG_ERR, 1, "iconv open error... [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    memset(utf_snormalname, 0, sizeof(utf_snormalname));
    memset(utf_sfaultname , 0, sizeof(utf_sfaultname ));
    memset(euc_snormalname, 0, sizeof(euc_snormalname));
    memset(euc_sfaultname , 0, sizeof(euc_sfaultname ));

    strcpy(euc_snormalname, ptr->tthrlddtdi.snormalname);
    strcpy(euc_sfaultname , ptr->tthrlddtdi.sfaultname );

    print_dbg(DBG_INFO, 1, "@@@@@@@@@ DB UPDATE EUC-KR [%s][%s] [%s:%d:%s]", euc_snormalname, euc_sfaultname, __FILE__, __LINE__, __FUNCTION__);

    neucsnormal = strlen(euc_snormalname);
    ncovsnormal = sizeof(utf_snormalname);
    neucsfault  = strlen(euc_sfaultname);
    ncovsfault  = sizeof(utf_sfaultname);

    ptreuc_snormalname = euc_snormalname;
    ptreuc_sfaultname  = euc_sfaultname;
    ptrcov_snormalname = utf_snormalname;
    ptrcov_sfaultname  = utf_sfaultname;

    if (iconv(cd, &ptreuc_snormalname, &neucsnormal, &ptrcov_snormalname, &ncovsnormal) < 0)
    {
        print_dbg(DBG_INFO, 1, "iconv fail euckr -> utf8 [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (iconv(cd, &ptreuc_sfaultname, &neucsfault, &ptrcov_sfaultname, &ncovsfault) < 0)
    {
        print_dbg(DBG_INFO, 1, "iconv fail euckr -> utf8 [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    iconv_close(cd);

    sprintf(szsql, "\
update TFMS_DIDOTHRLD_DTL \
set NORMAL='%d' \
, LABEL0='%s' \
, LABEL1='%s' \
, EVT_GRADE_CD='%02d', TRANS_CHK='N' \
where SNSR_ID='%04d'"
                   , ptr->tthrlddtdi.bnormalval
                   , utf_sfaultname
                   , utf_snormalname
                   , ptr->tthrlddtdi.egrade
                   , snsr_id
                   );

    print_dbg(DBG_INFO, 1, "@@@@@@@ mysql sql %s [%s:%d:%s]", szsql, __FILE__, __LINE__, __FUNCTION__);

    if (db_mysqlping(EDB_CONN_SSRMONITOR) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_SSRMONITOR], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_SSRMONITOR]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    insert_db_didothrld_hist(ptrdata, snsr_id);

    return ERR_SUCCESS;
}

int update_db_didothrld_hist(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
update TFMS_DIDOTHRLDHIST_DTL set \
TRANS_CHK='Y' \
where CTL_ID='%s'"
                   , ptr->tdbtans.sdbfclt_tid
                   );

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql ) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

int insert_db_didothrld_hist(void * ptrdata, int snsr_id)
{
    fclt_ctl_thrlddi_t * ptr = (fclt_ctl_thrlddi_t *) ptrdata;
    static char utf_snormalname[MAX_PATH_SIZE], *ptrcov_snormalname;
    static char utf_sfaultname [MAX_PATH_SIZE], *ptrcov_sfaultname;
    static char euc_snormalname[SZ_MAX_NAME  ], *ptreuc_snormalname;	// ON 이름
    static char	euc_sfaultname [SZ_MAX_NAME  ], *ptreuc_sfaultname;	    // OFF 이름
    iconv_t cd;
    size_t neucsnormal, ncovsnormal;
    size_t neucsfault , ncovsfault;

    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }


    if ((cd = iconv_open("UTF-8" , "EUC-KR")) == (iconv_t)(-1))
    {
        print_dbg(DBG_ERR, 1, "iconv open error... [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    memset(utf_snormalname, 0, sizeof(utf_snormalname));
    memset(utf_sfaultname , 0, sizeof(utf_sfaultname ));
    memset(euc_snormalname, 0, sizeof(euc_snormalname));
    memset(euc_sfaultname , 0, sizeof(euc_sfaultname ));

    strcpy(euc_snormalname, ptr->tthrlddtdi.snormalname);
    strcpy(euc_sfaultname , ptr->tthrlddtdi.sfaultname );

    print_dbg(DBG_INFO, 1, "@@@@@@@@@ DB INSERT EUC-KR [%s][%s] [%s:%d:%s]", euc_snormalname, euc_sfaultname, __FILE__, __LINE__, __FUNCTION__);

    neucsnormal = strlen(euc_snormalname);
    ncovsnormal = sizeof(utf_snormalname);
    neucsfault  = strlen(euc_sfaultname);
    ncovsfault  = sizeof(utf_sfaultname);

    ptreuc_snormalname = euc_snormalname;
    ptreuc_sfaultname  = euc_sfaultname;
    ptrcov_snormalname = utf_snormalname;
    ptrcov_sfaultname  = utf_sfaultname;

    if (iconv(cd, &ptreuc_snormalname, &neucsnormal, &ptrcov_snormalname, &ncovsnormal) < 0)
    {
        print_dbg(DBG_INFO, 1, "iconv fail euckr -> utf8 [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (iconv(cd, &ptreuc_sfaultname, &neucsfault, &ptrcov_sfaultname, &ncovsfault) < 0)
    {
        print_dbg(DBG_INFO, 1, "iconv fail euckr -> utf8 [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    iconv_close(cd);

    sprintf(szsql, "\
insert into TFMS_DIDOTHRLDHIST_DTL \
(CTL_ID , CTL_TIME \
, SNSR_ID , NORMAL \
, LABEL0 \
, LABEL1 \
, EVT_GRADE_CD , TRANS_CHK) \
values \
('%s' , from_unixtime('%u') \
, '%04d' , '%d' \
, '%s' \
, '%s' \
, '%02d' , 'N')"
                   , ptr->ctl_base.tdbtans.sdbfclt_tid, ptr->ctl_base.tdbtans.tdatetime
                   , snsr_id, ptr->tthrlddtdi.bnormalval
                   , utf_sfaultname
                   , utf_snormalname
                   , ptr->tthrlddtdi.egrade
                   );

    print_dbg(DBG_INFO, 1, "@@@@@@@ mysql sql %s [%s:%d:%s]", szsql, __FILE__, __LINE__, __FUNCTION__);


    if (db_mysqlping(EDB_CONN_SSRMONITOR) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_SSRMONITOR], szsql ) != 0 )
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_SSRMONITOR]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}


// ai 히스토리 (모니터링)
int update_db_ai_hist(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
update TFMS_AIHIST_DTL set \
TRANS_CHK='Y' \
where HIST_ID='%s'"
                   , ptr->tdbtans.sdbfclt_tid
                   );

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql ) != 0 )
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

int insert_db_ai_hist(void * ptrdata)
{
    fclt_collectrslt_ai_t * ptr = (fclt_collectrslt_ai_t *) ptrdata;
    int i, esnsrtype, esnsrsub, idxsub;
    static char szsql[ MAX_SQL_SIZE ];
    static char sztxt[ MAX_NAME_SIZE ];
    static char szval[ MAX_VALU_SIZE ];
    static char sztmp[64];
    snsr_info_t snsrinfo;
    memset(szsql, 0, sizeof(szsql));
    memset(sztxt, 0, sizeof(sztxt));
    memset(szval, 0, sizeof(szval));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if (ptr->aidido.nfcltsubcount <= 0)
    {
        print_dbg(DBG_ERR, 1, "Is zero fcltsubcount [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    for (i = 0; i < ptr->aidido.nfcltsubcount; i++)
    {
        memset(&snsrinfo, 0, sizeof(snsrinfo));
        memset(&sztmp   , 0, sizeof(sztmp   ));

        esnsrtype = ptr->rslt[i].esnsrtype;
        esnsrsub  = ptr->rslt[i].esnsrsub;
        idxsub    = ptr->rslt[i].idxsub;

        if (get_db_col_info_shm(esnsrtype, esnsrsub, idxsub, &snsrinfo) != ERR_SUCCESS)
        {
            print_dbg(DBG_ERR, 1, "Snsr Info Not Found (%d)(%d)(%d) [%s:%d:%s]"
                , esnsrtype, esnsrsub, idxsub, __FILE__, __LINE__, __FUNCTION__);
            continue;
        }
        
        strcat (sztxt, snsrinfo.szcol);
        sprintf(sztmp, "'%0.2f'", ptr->rslt[i].frslt);
        strcat (szval, sztmp);
        if ((i + 1) < ptr->aidido.nfcltsubcount)
        {
            strcat(sztxt, ",");
            strcat(szval, ",");
        }
    }

    sprintf(szsql, "\
insert into TFMS_AIHIST_DTL \
(HIST_ID , FCLT_ID \
, SITE_ID , TIME \
, %s \
, TRANS_CHK) \
values \
('%s' , '%04d' \
, '%04d' , from_unixtime('%u') \
, %s \
, 'N')"
                   , sztxt
                   , ptr->aidido.tdbtans.sdbfclt_tid, ptr->aidido.tfcltelem.nfcltid
                   , ptr->aidido.tfcltelem.nsiteid, ptr->aidido.tdbtans.tdatetime
                   , szval
                   );

    if (db_mysqlping(EDB_CONN_SSRMONITOR) != ERR_SUCCESS)
    {
       print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_SSRMONITOR], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_SSRMONITOR]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}


// dido 히스토리 (모니터링)
int update_db_dido_hist(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
update TFMS_DIDOHIST_DTL set \
TRANS_CHK='Y' \
where HIST_ID='%s'"
                   , ptr->tdbtans.sdbfclt_tid
                   );

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

int insert_db_dido_hist(void * ptrdata)
{
    fclt_collectrslt_dido_t * ptr = (fclt_collectrslt_dido_t *) ptrdata;
    int i, esnsrtype, esnsrsub, idxsub;
    static char szsql[ MAX_SQL_SIZE ];
    static char sztxt[ MAX_NAME_SIZE ];
    static char szval[ MAX_VALU_SIZE ];
    static char sztmp[64];
    snsr_info_t snsrinfo;
    memset(szsql, 0, sizeof(szsql));
    memset(sztxt, 0, sizeof(sztxt));
    memset(szval, 0, sizeof(szval));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if (ptr->aidido.nfcltsubcount <= 0)
    {
        print_dbg(DBG_ERR, 1, "Is zero fcltsubcount [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    for (i = 0; i < ptr->aidido.nfcltsubcount; i++)
    {
        memset(&snsrinfo, 0, sizeof(snsrinfo));
        memset(&sztmp   , 0, sizeof(sztmp   ));

        esnsrtype = ptr->rslt[i].esnsrtype;
        esnsrsub  = ptr->rslt[i].esnsrsub;
        idxsub    = ptr->rslt[i].idxsub;

        if (get_db_col_info_shm(esnsrtype, esnsrsub, idxsub, &snsrinfo) != ERR_SUCCESS)
        {
            print_dbg(DBG_ERR, 1, "Snsr Info Not Found (%d)(%d)(%d) [%s:%d:%s]"
                , esnsrtype, esnsrsub, idxsub, __FILE__, __LINE__, __FUNCTION__);
            continue;
        }

        strcat (sztxt, snsrinfo.szcol);
        sprintf(sztmp, "'%d'", ptr->rslt[i].brslt);
        strcat (szval, sztmp);
        if ((i + 1) < ptr->aidido.nfcltsubcount)
        {
            strcat(sztxt, ",");
            strcat(szval, ",");
        }
    }

    sprintf(szsql, "\
insert into TFMS_DIDOHIST_DTL \
(HIST_ID , FCLT_ID \
, SITE_ID , TIME \
, %s \
, TRANS_CHK) \
values \
('%s' , '%04d' \
, '%04d' , from_unixtime('%u') \
, %s \
, 'N')"
                   , sztxt
                   , ptr->aidido.tdbtans.sdbfclt_tid, ptr->aidido.tfcltelem.nfcltid
                   , ptr->aidido.tfcltelem.nsiteid, ptr->aidido.tdbtans.tdatetime
                   , szval
                   );


    if (db_mysqlping(EDB_CONN_SSRMONITOR) != ERR_SUCCESS)
    {
       print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_SSRMONITOR], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_SSRMONITOR]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

// 고정, 운영, 방탐, 준고정
int update_db_eqp_hist(void * ptrdata)
{
    fms_elementfclt_t * ptr = (fms_elementfclt_t *) ptrdata;
    //int nsiteid = 0;// 원격국 id
    //int nfcltid = 0;// fclt id
    int efclt = EFCLT_NONE;// fclt cd 설비구분(대분류 또는 시스템 구분)

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    efclt = ptr->efclt;

    switch (efclt)
    {
    case EFCLT_EQP01 + 0: //감시장비 11 -- FRE(고정감시 장비)
        update_db_fre_hist(ptrdata);
        break;
    case EFCLT_EQP01 + 1: //감시장비 12 -- FRS(고정감시 운영SW)
        update_db_frs_hist(ptrdata);
        break;
    case EFCLT_EQP01 + 2: //감시장비 13 -- SRE(준고정 장비)
        update_db_sre_hist(ptrdata);
        break;
    case EFCLT_EQP01 + 3: //감시장비 14 -- FDE(고정방탐 장비)
        update_db_fde_hist(ptrdata);
        break;
    }

    return ERR_SUCCESS;
}

int insert_db_eqp_hist(int snsrtype, void * ptrdata)
{
    fms_elementfclt_t * ptr = (fms_elementfclt_t *) ptrdata;
    //int nsiteid = 0;// 원격국 id
    //int nfcltid = 0;// fclt id
    int efclt = EFCLT_NONE;// fclt cd 설비구분(대분류 또는 시스템 구분)

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    efclt = ptr->efclt;

    switch (efclt)
    {
    case EFCLT_EQP01 + 0: //감시장비 11 -- FRE(고정감시 장비)
        insert_db_fre_hist(ptrdata);
        break;
    case EFCLT_EQP01 + 1: //감시장비 12 -- FRS(고정감시 운영SW)
        insert_db_frs_hist(ptrdata);
        break;
    case EFCLT_EQP01 + 2: //감시장비 13 -- SRE(준고정 장비)
        insert_db_sre_hist(ptrdata);
        break;
    case EFCLT_EQP01 + 3: //감시장비 14 -- FDE(고정방탐 장비)
        insert_db_fde_hist(ptrdata);
        break;
    }

    return ERR_SUCCESS;
}

// 고정감시 히스토리 (모니터링)
int update_db_fre_hist(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
update TFMS_FREHIST_DTL set \
TRANS_CHK='Y' \
where HIST_ID='%s'"
                   , ptr->tdbtans.sdbfclt_tid
                   );

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

int insert_db_fre_hist(void * ptrdata)
{
    fclt_collectrslt_dido_t * ptr = (fclt_collectrslt_dido_t *) ptrdata;
    int i, esnsrtype, esnsrsub, idxsub;
    static char szsql[ MAX_SQL_SIZE ];
    static char sztxt[ MAX_NAME_SIZE ];
    static char szval[ MAX_VALU_SIZE ];
    static char sztmp[64];
    snsr_info_t snsrinfo;
    memset(szsql, 0, sizeof(szsql));
    memset(sztxt, 0, sizeof(sztxt));
    memset(szval, 0, sizeof(szval));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if (ptr->aidido.nfcltsubcount <= 0)
    {
        print_dbg(DBG_ERR, 1, "Is zero fcltsubcount [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    for (i = 0; i < ptr->aidido.nfcltsubcount; i++)
    {
        memset(&snsrinfo, 0, sizeof(snsrinfo));
        memset(&sztmp   , 0, sizeof(sztmp   ));

        esnsrtype = ptr->rslt[i].esnsrtype;
        esnsrsub  = ptr->rslt[i].esnsrsub;
        idxsub    = ptr->rslt[i].idxsub;

        if (get_db_col_info_shm(esnsrtype, esnsrsub, idxsub, &snsrinfo) != ERR_SUCCESS)
        {
            print_dbg(DBG_ERR, 1, "Snsr Info Not Found (%d)(%d)(%d) [%s:%d:%s]"
                , esnsrtype, esnsrsub, idxsub, __FILE__, __LINE__, __FUNCTION__);
            continue;
        }

        strcat (sztxt, snsrinfo.szcol);
        sprintf(sztmp, "'%d'", ptr->rslt[i].brslt);
        strcat (szval, sztmp);
        if ((i + 1) < ptr->aidido.nfcltsubcount)
        {
            strcat(sztxt, ",");
            strcat(szval, ",");
        }
    }

    sprintf(szsql, "\
insert into TFMS_FREHIST_DTL \
(HIST_ID , FCLT_ID \
, SITE_ID , TIME \
, %s \
, TRANS_CHK) \
values \
('%s' , '%04d' \
, '%04d' , from_unixtime('%u') \
, %s \
, 'N')"
                   , sztxt
                   , ptr->aidido.tdbtans.sdbfclt_tid, ptr->aidido.tfcltelem.nfcltid
                   , ptr->aidido.tfcltelem.nsiteid, ptr->aidido.tdbtans.tdatetime
                   , szval
                   );

    if (db_mysqlping(EDB_CONN_EQPMONITOR) != ERR_SUCCESS)
    {
       print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_EQPMONITOR], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_EQPMONITOR]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;

}


// 고정감시운영SW 히스토리 (모니터링)
int update_db_frs_hist(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
update TFMS_FRSHIST_DTL set \
TRANS_CHK='Y' \
where HIST_ID='%s'"
                   , ptr->tdbtans.sdbfclt_tid
                   );

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

int insert_db_frs_hist(void * ptrdata)
{
    fclt_collectrslt_dido_t * ptr = (fclt_collectrslt_dido_t *) ptrdata;
    int i, esnsrtype, esnsrsub, idxsub;
    static char szsql[ MAX_SQL_SIZE ];
    static char sztxt[ MAX_NAME_SIZE ];
    static char szval[ MAX_VALU_SIZE ];
    static char sztmp[64];
    snsr_info_t snsrinfo;
    memset(szsql, 0, sizeof(szsql));
    memset(sztxt, 0, sizeof(sztxt));
    memset(szval, 0, sizeof(szval));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if (ptr->aidido.nfcltsubcount <= 0)
    {
        print_dbg(DBG_ERR, 1, "Is zero fcltsubcount [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    for (i = 0; i < ptr->aidido.nfcltsubcount; i++)
    {
        memset(&snsrinfo, 0, sizeof(snsrinfo));
        memset(&sztmp   , 0, sizeof(sztmp   ));

        esnsrtype = ptr->rslt[i].esnsrtype;
        esnsrsub  = ptr->rslt[i].esnsrsub;
        idxsub    = ptr->rslt[i].idxsub;

        if (get_db_col_info_shm(esnsrtype, esnsrsub, idxsub, &snsrinfo) != ERR_SUCCESS)
        {
            print_dbg(DBG_ERR, 1, "Snsr Info Not Found (%d)(%d)(%d) [%s:%d:%s]"
                , esnsrtype, esnsrsub, idxsub, __FILE__, __LINE__, __FUNCTION__);
            continue;
        }

        strcat (sztxt, snsrinfo.szcol);
        sprintf(sztmp, "'%d'", ptr->rslt[i].brslt);
        strcat (szval, sztmp);
        if ((i + 1) < ptr->aidido.nfcltsubcount)
        {
            strcat(sztxt, ",");
            strcat(szval, ",");
        }
    }

    sprintf(szsql, "\
insert into TFMS_FRSHIST_DTL \
(HIST_ID , FCLT_ID \
, SITE_ID , TIME \
, %s \
, TRANS_CHK) \
values \
('%s' , '%04d' \
, '%04d' , from_unixtime('%u') \
, %s \
, 'N')"
                   , sztxt
                   , ptr->aidido.tdbtans.sdbfclt_tid, ptr->aidido.tfcltelem.nfcltid
                   , ptr->aidido.tfcltelem.nsiteid, ptr->aidido.tdbtans.tdatetime
                   , szval
                   );

    if (db_mysqlping(EDB_CONN_EQPMONITOR) != ERR_SUCCESS)
    {
       print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_EQPMONITOR], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_EQPMONITOR]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}


// 고정방탐 히스토리 (모니터링)
int update_db_fde_hist(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
update TFMS_FDEHIST_DTL set \
TRANS_CHK='Y' \
where HIST_ID='%s'"
                   , ptr->tdbtans.sdbfclt_tid
                   );

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

int insert_db_fde_hist(void * ptrdata)
{
    fclt_collectrslt_dido_t * ptr = (fclt_collectrslt_dido_t *) ptrdata;
    int i, esnsrtype, esnsrsub, idxsub;
    static char szsql[ MAX_SQL_SIZE ];
    static char sztxt[ MAX_NAME_SIZE ];
    static char szval[ MAX_VALU_SIZE ];
    static char sztmp[64];
    snsr_info_t snsrinfo;
    memset(szsql, 0, sizeof(szsql));
    memset(sztxt, 0, sizeof(sztxt));
    memset(szval, 0, sizeof(szval));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if (ptr->aidido.nfcltsubcount <= 0)
    {
        print_dbg(DBG_ERR, 1, "Is zero fcltsubcount [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    for (i = 0; i < ptr->aidido.nfcltsubcount; i++)
    {
        memset(&snsrinfo, 0, sizeof(snsrinfo));
        memset(&sztmp   , 0, sizeof(sztmp   ));

        esnsrtype = ptr->rslt[i].esnsrtype;
        esnsrsub  = ptr->rslt[i].esnsrsub;
        idxsub    = ptr->rslt[i].idxsub;

        if (get_db_col_info_shm(esnsrtype, esnsrsub, idxsub, &snsrinfo) != ERR_SUCCESS)
        {
            print_dbg(DBG_ERR, 1, "Snsr Info Not Found (%d)(%d)(%d) [%s:%d:%s]"
                , esnsrtype, esnsrsub, idxsub, __FILE__, __LINE__, __FUNCTION__);
            continue;
        }

        strcat (sztxt, snsrinfo.szcol);
        sprintf(sztmp, "'%d'", ptr->rslt[i].brslt);
        strcat (szval, sztmp);
        if ((i + 1) < ptr->aidido.nfcltsubcount)
        {
            strcat(sztxt, ",");
            strcat(szval, ",");
        }
    }

    sprintf(szsql, "\
insert into TFMS_FDEHIST_DTL \
(HIST_ID , FCLT_ID \
, SITE_ID , TIME \
, %s \
, TRANS_CHK) \
values \
('%s' , '%04d' \
, '%04d' , from_unixtime('%u') \
, %s \
, 'N')"
                   , sztxt
                   , ptr->aidido.tdbtans.sdbfclt_tid, ptr->aidido.tfcltelem.nfcltid
                   , ptr->aidido.tfcltelem.nsiteid, ptr->aidido.tdbtans.tdatetime
                   , szval
                   );

    if (db_mysqlping(EDB_CONN_EQPMONITOR) != ERR_SUCCESS)
    {
       print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_EQPMONITOR], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_EQPMONITOR]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}


// 준고정 히스토리 (모니터링)
int update_db_sre_hist(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
update TFMS_SREHIST_DTL set \
TRANS_CHK='Y' \
where HIST_ID='%s'"
                   , ptr->tdbtans.sdbfclt_tid
                   );

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

int insert_db_sre_hist(void * ptrdata)
{
    fclt_collectrslt_dido_t * ptr = (fclt_collectrslt_dido_t *) ptrdata;
    int i, esnsrtype, esnsrsub, idxsub;
    static char szsql[ MAX_SQL_SIZE ];
    static char sztxt[ MAX_NAME_SIZE ];
    static char szval[ MAX_VALU_SIZE ];
    static char sztmp[64];
    snsr_info_t snsrinfo;
    memset(szsql, 0, sizeof(szsql));
    memset(sztxt, 0, sizeof(sztxt));
    memset(szval, 0, sizeof(szval));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if (ptr->aidido.nfcltsubcount <= 0)
    {
        print_dbg(DBG_ERR, 1, "Is zero fcltsubcount [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    for (i = 0; i < ptr->aidido.nfcltsubcount; i++)
    {
        memset(&snsrinfo, 0, sizeof(snsrinfo));
        memset(&sztmp   , 0, sizeof(sztmp   ));

        esnsrtype = ptr->rslt[i].esnsrtype;
        esnsrsub  = ptr->rslt[i].esnsrsub;
        idxsub    = ptr->rslt[i].idxsub;

        if (get_db_col_info_shm(esnsrtype, esnsrsub, idxsub, &snsrinfo) != ERR_SUCCESS)
        {
            print_dbg(DBG_ERR, 1, "Snsr Info Not Found (%d)(%d)(%d) [%s:%d:%s]"
                , esnsrtype, esnsrsub, idxsub, __FILE__, __LINE__, __FUNCTION__);
            continue;
        }

        strcat (sztxt, snsrinfo.szcol);
        sprintf(sztmp, "'%d'", ptr->rslt[i].brslt);
        strcat (szval, sztmp);
        if ((i + 1) < ptr->aidido.nfcltsubcount)
        {
            strcat(sztxt, ",");
            strcat(szval, ",");
        }
    }

    sprintf(szsql, "\
insert into TFMS_SREHIST_DTL \
(HIST_ID , FCLT_ID \
, SITE_ID , TIME \
, %s \
, TRANS_CHK) \
values \
('%s' , '%04d' \
, '%04d' , from_unixtime('%u') \
, %s \
, 'N')"
                   , sztxt
                   , ptr->aidido.tdbtans.sdbfclt_tid, ptr->aidido.tfcltelem.nfcltid
                   , ptr->aidido.tfcltelem.nsiteid, ptr->aidido.tdbtans.tdatetime
                   , szval
                   );

    if (db_mysqlping(EDB_CONN_EQPMONITOR) != ERR_SUCCESS)
    {
       print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_EQPMONITOR], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_EQPMONITOR]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}


// 시스템장애 히스토리
int update_db_sys_hist(void * ptrdata)
{
    fclt_svrtransactionack_t * ptr = (fclt_svrtransactionack_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
update TFMS_SYSHIST_DTL set \
TRANS_CHK='Y' \
where HIST_ID='%s'"
                   , ptr->tdbtans.sdbfclt_tid
                   );

    if (db_mysqlping(EDB_CONN_UPDATE) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_UPDATE], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_UPDATE]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}


int insert_db_sys_hist(void * ptrdata)
{
    db_syshist_t * ptr = (db_syshist_t *) ptrdata;
    static char szsql[ MAX_SQL_SIZE ];
    memset(szsql, 0, sizeof(szsql));

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

    sprintf(szsql, "\
insert into TFMS_SYSHIST_DTL \
(HIST_ID , FCLT_ID \
, SITE_ID , TIME \
, ERROR_CD , IP , PORT \
, RTU_VAL , NOTI_CD \
, TRANS_CHK) \
values \
('%s' , '%04d' \
, '%04d' , from_unixtime('%u') \
, '%d' , '%s' , '%d' \
, '0' , '0' \
, 'N')"
                   , ptr->tdbtans.sdbfclt_tid, ptr->tfcltelem.nfcltid
                   , ptr->tfcltelem.nsiteid, ptr->tdbtans.tdatetime
                   , ptr->errcd, ptr->ipaddr, ptr->port
                   );

    if (db_mysqlping(EDB_CONN_EQPMONITOR) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "mysql unconnector ...[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
    }

    if (mysql_query(g_ptrconn[EDB_CONN_EQPMONITOR], szsql) != 0)
    {
        print_dbg( DBG_ERR,1, "Cannot %s[%s:%d:%s]", mysql_error(g_ptrconn[EDB_CONN_EQPMONITOR]), __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }
    return ERR_SUCCESS;

}
