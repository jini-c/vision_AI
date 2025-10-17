/*********************************************************************
	file : fms_client.c
	description :
	product by tory45( tory45@empal.com) ( 2016.08.05 )
	update by ?? ( . . . )
	
	- Control에서 fms_server로 보낼 데이터를 받아 fms_server로 보내는 기능
	- fms_server에서 데이터를 받아 Control로 정보를 보내는 기능 
 ********************************************************************/
#include "com_include.h"
#include "sem.h"
#include "shm.h"
#include "fms_client.h"
#include "net.h"

/*================================================================================================
DEFINE
================================================================================================*/
#define NET_FMS_ALIVE_CHECK_TIME  (30)

/*================================================================================================
 전역 변수 
 1.static 적용 
 2.숫자형 데이터 일경우 volatile 적용 
 3.초기값 적용.
================================================================================================*/
static volatile int g_sem_key = FMS_CLIENT_KEY;					/* FMS Client용 세마포어 키 */
static volatile int g_sem_id = 0;								/* FMS Client용 세마포어 아이디 */
static send_data_func  g_ptrsend_data_control_func = NULL;		/* CONTROL 모듈  설정,제어, 알람  정보를 넘기는 함수 포인트 */

/* thread ( fms client )*/
static volatile int g_start_fms_pthread 	= 0; 
static volatile int g_end_fms_pthread 		= 1; 
static pthread_t fms_pthread;
static void * fms_pthread_idle( void * ptrdata );
static volatile int g_release				= 0;

static net_packet_t * g_fms_packet_que = NULL;

static volatile long g_fms_alive_check_send_time = 0;
static volatile long g_fms_alive_check_send_cnt  = 0;
static volatile long g_fms_connect_check_time = 0;
static volatile int  g_fms_confirm_state = 0;

static net_comm_t g_netcomm;
static net_sock_t g_netsock;
static net_sock_t g_svrsock; // 콘트롤로 서버정보 전달시 사용


/*================================================================================================
  내부 함수 선언
  static 적용 
================================================================================================*/
static net_packet_t * empty_packet_que();
static int recv_packet(char * data, int len);
static int send_fms_packet(void * ptrsock, void * ptrhead, void * ptrbody);
static int make_fms_packet_head(void * ptrhead, int cmd, int nsize);

static int create_fms_thread( void );

static void* fms_pthread_idle( void * ptrdata );

static int parser_packet(fms_packet_header_t* ptrhead, void * ptrbody);

static int create_fms_packet_que( void );
static int release_fms_packet_que( void );
static int init_fms_packet_que( void );

//////////////////////////////////////////////////////////////////////////////////////////////////////////

static int send_fms_confirm_req();
static int send_fms_alive_check();

//////////////////////////////////////////////////////////////////////////////////////////////////////////

static int send_data_control( UINT16 inter_msg, void * ptrdata );

static int internal_init( void );
static int internal_idle( void * ptr );
static int internal_release( void );

/* Proc 함수 [ typedef int ( * proc_func )( unsigned short , void * ); ] */ 
static int rtusvr_sns_data_res( unsigned short inter_msg, void * ptrdata );
static int rtusvr_sns_cudata_res(unsigned short inter_msg, void * ptrdata);
static int rtusvr_sns_evt_fault(unsigned short inter_msg, void * ptrdata);
static int rtusvr_sns_ctl_power(unsigned short inter_msg, void * ptrdata);
static int rtusvr_sns_ctl_res(unsigned short inter_msg, void * ptrdata);
static int rtusvr_sns_ctl_thrld(unsigned short inter_msg, void * ptrdata);
static int rtusvr_sns_completed_thrld(unsigned short inter_msg, void * ptrdata);
static int rtusvr_eqp_data_res(unsigned short inter_msg, void * ptrdata);
static int rtusvr_eqp_cudata_res(unsigned short inter_msg, void * ptrdata);
static int rtusvr_eqp_evt_fault(unsigned short inter_msg, void * ptrdata);
static int rtusvr_eqp_ctl_power(unsigned short inter_msg, void * ptrdata);
static int rtusvr_eqp_ctl_res(unsigned short inter_msg, void * ptrdata);
static int rtusvr_eqp_ctl_thrld(unsigned short inter_msg, void * ptrdata);
static int rtusvr_eqp_completed_thrld(unsigned short inter_msg, void * ptrdata);
static int rtusvr_timedat_req(unsigned short inter_msg, void * ptrdata);
static int rtusvr_database_update_res(unsigned short inter_msg, void * ptrdata);
static int rtusvr_evt_allim_msg(unsigned short inter_msg, void * ptrdata);
static int rtusvr_evt_network_err(unsigned short inter_msg, void * ptrdata);

/*================================================================================================
  Proc Function Point 구조체
 	Messsage ID 	: 내부 Message ID
	Proc Function 	: Message ID 처리 함수 
	Control에서 받은 데이터를 처리할 함수를 아래의 형식으로 등록하시면 됩니다. 
	typedef int ( * proc_func )( unsigned short , void * );
	이해가되시면 주석은 삭제하셔도 됩니다. 
================================================================================================*/
static proc_func_t g_proc_func_list []= 
{
	/* inter msg id   		,  			proc function point */
	/*=============================================*/
    { EFT_SNSRDAT_RES, 			rtusvr_sns_data_res },	//RTU센서 모니터링 데이터 수집
    { EFT_SNSRCUDAT_RES,		rtusvr_sns_cudata_res },//RTU센서 모니터링 데이터 실시간 수집 (FOP 전용)
    { EFT_SNSREVT_FAULT,		rtusvr_sns_evt_fault }, //장애(FAULT)이벤트
    { EFT_SNSRCTL_POWER,		rtusvr_sns_ctl_power }, //전원제어 (ON/OFF)
    { EFT_SNSRCTL_RES,			rtusvr_sns_ctl_res },   //전원제어에 대한 결과
    { EFT_SNSRCTL_THRLD,        rtusvr_sns_ctl_thrld }, //RTU센서 임계기준 설정
    { EFT_SNSRCTLCOMPLETED_THRLD,rtusvr_sns_completed_thrld }, //임계기준 수정 완료를 OP에게 알려줌.
    { EFT_EQPDAT_RES,			rtusvr_eqp_data_res },//감시장비 모니터링 데이터
    { EFT_EQPCUDAT_RES,			rtusvr_eqp_cudata_res }, //상태 정보(실시간) 송신 프로세스 시작/종료
    { EFT_EQPEVT_FAULT,			rtusvr_eqp_evt_fault }, //장애(FAULT)이벤트
    { EFT_EQPCTL_POWER,			rtusvr_eqp_ctl_power }, //감시장비 전원제어 (ON/OFF)
    { EFT_EQPCTL_RES,			rtusvr_eqp_ctl_res }, //전원제어에 대한 진행상태 정보
    { EFT_EQPCTL_THRLD,			rtusvr_eqp_ctl_thrld }, //감시장비 항목별 임계 설정
    { EFT_EQPCTLCOMPLETED_THRLD,rtusvr_eqp_completed_thrld }, //임계기준 수정 완료를 OP에게 알려줌.
    { EFT_TIMEDAT_REQ,			rtusvr_timedat_req }, //시간동기화 : RTU - SVR - OP 간 시간 동기화
    { EFT_EVT_BASEDATUPDATE_RES,rtusvr_database_update_res },
    { EFT_ALLIM_MSG,            rtusvr_evt_allim_msg },
    { EFT_EVT_FCLTNETWORK_ERR,  rtusvr_evt_network_err },
	{ 0, NULL }
};

/*================================================================================================
내부 함수  정의 
매개변수가 pointer일경우 반드시 null 체크 적용 
================================================================================================*/

static int connect_fmssvr( void )
{
	static int cnt = 0;
	char * ptrip = NULL;

    g_fms_confirm_state = 0;
    g_fms_alive_check_send_time = 0;
    g_fms_alive_check_send_cnt  = 0;

	ptrip = get_fms_server_ip_shm();

	if ( ptrip == NULL || strlen( ptrip ) < 5 )
	{
        g_netsock.isalive = 0;
        set_fms_server_net_sts(NET_FMS_STATE_ERRORS);
		return ERR_SUCCESS;
	}

    strcpy(g_netsock.clntip, ptrip );
    g_netsock.clntport = get_fms_server_port_shm();
    g_netsock.clntsock = net_connect_client(g_netsock.clntip, g_netsock.clntport);


    memset(&g_svrsock, 0, sizeof(g_svrsock));
    memcpy(&g_svrsock, &g_netsock, sizeof(g_svrsock));
    g_netcomm.ptrsock = &g_svrsock;
    g_netcomm.ptrsock->clntsock = 0; // FMS서버 콘트롤과 통신시 아이디 소켓 0

    if (g_netsock.clntsock > 0)
    {
    	print_line();
        print_dbg(DBG_SAVE, 1, "connect_fmssvr() Success[%s:%d] [%s:%d:%s]",
            g_netsock.clntip, g_netsock.clntport, __FILE__, __LINE__, __FUNCTION__);
        g_netsock.isalive = 1;
        g_fms_connect_check_time = time(NULL);
        set_fms_server_net_sts(NET_FMS_STATE_NORMAL);
        send_fms_confirm_req();
    }
    else
    {
		if ( cnt++ > 4 )
		{
    		print_line();
			print_dbg(DBG_ERR, 1, "connect_fmssvr error... [%s:%d] [%s:%d:%s]",
					g_netsock.clntip, g_netsock.clntport, __FILE__, __LINE__, __FUNCTION__);
			cnt = 0;
		}

        g_netsock.isalive = 0;
        set_fms_server_net_sts(NET_FMS_STATE_ERRORS);
        return ERR_FAIL;
    }
    
    return ERR_SUCCESS;
}

static int connect_close( void )
{
    if (g_netsock.clntsock > 0)
    {
        print_line();
        print_dbg(DBG_ERR, 1, "close_fmssvr [%s:%d] [%s:%d:%s]",
            g_netsock.clntip, g_netsock.clntport, __FILE__, __LINE__, __FUNCTION__);
        g_start_fms_pthread = 0;
        g_fms_confirm_state = 0;
        net_connect_close(g_netsock.clntsock);
        g_netsock.clntsock = -1;
        g_netsock.isalive  = 0;
    }
    return ERR_SUCCESS;
}

static int create_fms_thread( void )
{
    int ret = ERR_SUCCESS;

    //print_dbg(DBG_NONE, 1, "create_fms_thread()");

    if ( g_release == 0 && g_start_fms_pthread == 0 && g_end_fms_pthread == 1 )
    {

        g_start_fms_pthread = 1;

        if ( pthread_create( &fms_pthread, NULL, fms_pthread_idle , NULL ) < 0 )
        {
            g_start_fms_pthread = 0;
            print_dbg( DBG_ERR, 1, "Fail to Create fms client Thread" );
            ret = ERR_RETURN;
        }
        else
        {
            print_dbg( DBG_NONE, 1, "Create fms client Thread Create OK. ");
        }
    }

    return ret;
}

static void* fms_pthread_idle( void * ptrdata )
{
    ( void ) ptrdata;

    int max_fd = -1;
    fd_set readsets;
    struct timeval tv;
    int n;
    char recvdata[512];
    int recvsize;

    int status;

    g_end_fms_pthread = 0;
#if 1
    while ( g_release == 0 && g_start_fms_pthread == 1 )
    {
        FD_ZERO(&readsets);
        FD_SET(g_netsock.clntsock, &readsets);

        max_fd = g_netsock.clntsock;

        tv.tv_sec  = 1;	/* 1sec */
        tv.tv_usec = 0;

        n = select( max_fd + 1 , &readsets, NULL, NULL , &tv );

        if (n == -1) /*select 함수 오류 발생 */
        { 
            print_dbg( DBG_ERR, 1, "select function error [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
            sys_sleep(1000);
            continue;
        }
        else if (n == 0) /* time-out에 의한 리턴 */
        { 
            //print_dbg( DBG_NONE, 1,"===>> 시간이 초과 되었습니다 : sock %d", g_sock);
        }
        else /* 파일 디스크립터 변화에 의한 리턴 */
        { 
            //print_dbg( DBG_NONE, 1,"===>> fms read event : sock %d", g_sock);

            if (FD_ISSET(g_netsock.clntsock, &readsets))
            {
                recvsize = read(g_netsock.clntsock, recvdata, 512);

                if (recvsize > 0)
                {
                    print_dbg( DBG_NONE, 1, "fms recv datasize : %d  [%s:%d:%s]", recvsize, __FILE__, __LINE__,__FUNCTION__ );

                    if (recv_packet(recvdata, recvsize) != ERR_SUCCESS)
                    {
                        ;;;
                    }
                    // select init set
                    FD_ZERO(&readsets);
                    FD_SET(g_netsock.clntsock, &readsets);
                }
                else
                {
                    print_dbg( DBG_SAVE, 1,"fms close event : %d  [%s:%d:%s]", g_netsock.clntsock, __FILE__, __LINE__,__FUNCTION__ );
                    // server socket cloase
                    FD_CLR(g_netsock.clntsock, &readsets);
                    net_connect_close(g_netsock.clntsock);
                    g_netsock.clntsock = -1;
                    g_netsock.isalive  = 0;
                    g_fms_confirm_state = 0;
                    g_start_fms_pthread = 0;
                }
            }
        }
        sys_sleep(1);
    }
#endif
    if( g_end_fms_pthread == 0 )
    {
        net_packet_t* ptrque = NULL;
        int i;
        pthread_join( fms_pthread , ( void **)&status );
        g_start_fms_pthread 	= 0;
        g_end_fms_pthread 		= 1;

        print_dbg(DBG_INFO, 1, "####### ====== >>> pthread_join end !!!!!!!!!! [%s:%s]", __FILE__, __FUNCTION__);

        for (i = 0; i < NET_FMS_PACKET_QUE; i++)
        {
            ptrque = &(g_fms_packet_que[i]);

            if (ptrque->sendid == EFT_EVT_BASEDATUPDATE_RES)
            {
                set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
            }
        }

    }

    return ERR_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////


static int create_fms_packet_que( void )
{
    if (g_fms_packet_que != NULL)
    {
        print_dbg(DBG_ERR, 1, "fms_packet_que not null... [%s%s] ", __FILE__, __FUNCTION__);
        return ERR_FAIL;
    }

    g_fms_packet_que = A_ALLOC(net_packet_t, NET_FMS_PACKET_QUE);

    return ERR_SUCCESS;
}

static int release_fms_packet_que( void )
{
    A_FREE(g_fms_packet_que);

    return ERR_SUCCESS;
}

static int init_fms_packet_que( void )
{
    int i;
    for (i = 0; i < NET_FMS_PACKET_QUE; i++)
    {
        net_packet_rule_init(&(g_fms_packet_que[i]));
    }
    return ERR_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////


static int recv_packet(char * data, int len)
{
    recv_data_buffer_t * ptrbuf = NULL;
    fms_packet_header_t headerdata;
    int headersize = sizeof(headerdata);
    int datasize = 0;

    if (data == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_FAIL;
    }

    ptrbuf = &(g_netsock.recvbuf);

    if ((ptrbuf->posw + len) > NET_RECV_BUFFER_MAX)
    {
        print_dbg( DBG_ERR, 1, "recv data buffer overflow [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        ptrbuf->posw = 0;
        ptrbuf->posr = 0;
        return ERR_OVERFLOW;
    }

    memcpy(ptrbuf->buf + ptrbuf->posw, data, len);
    ptrbuf->posw += len;

    while ((datasize = ptrbuf->posw - ptrbuf->posr) >= headersize)
    {
        int packetsize = 0;

        memcpy(&headerdata, ptrbuf->buf + ptrbuf->posr, headersize);

        packetsize = headersize + headerdata.nsize;

        // 리시브 데이터 크기와 패킷사이즈 확인
        if (packetsize > datasize)
        {
            return ERR_SUCCESS;
        }

        // 패킷데이터 파서
        print_dbg(DBG_INFO, 1, "[RECV BUFFER] POSR(%d) POSW(%d) PACKETSIZE(%d) [%s:%s]", ptrbuf->posr, ptrbuf->posw, packetsize, __FILE__, __FUNCTION__);
        print_dbg(DBG_NONE, 1, "shprotocol(%X) shopcode(%d) nsize(%d) shdirection(%d)", headerdata.shprotocol, headerdata.shopcode, headerdata.nsize, headerdata.shdirection);
        parser_packet(&headerdata, ptrbuf->buf + (ptrbuf->posr + headersize));

        // 데이터버퍼 읽은 위치 이동
        ptrbuf->posr += packetsize;

        // 미처리된 데이터 처리
        if (ptrbuf->posw <= ptrbuf->posr)
        {
            ptrbuf->posr = 0;
            ptrbuf->posw = 0;
        }
        else
        {
            int srcpos, dstpos = 0;
            for (srcpos = ptrbuf->posr; srcpos < ptrbuf->posw;)
            {
                ptrbuf->buf[dstpos++] = ptrbuf->buf[srcpos];
                ptrbuf->buf[srcpos++] = 0;
            }
            ptrbuf->posr = 0;
            ptrbuf->posw = dstpos;
        }
    }//while ((datasize = ptrbuf->posw - ptrbuf->posr) > headersize)

    return ERR_SUCCESS;
}

static int parser_packet(fms_packet_header_t* ptrhead, void * ptrbody)
{
    net_packet_t* ptrque = NULL;
    int i;

    if ( ptrhead == NULL || ptrbody == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    set_fms_server_net_sts(NET_FMS_STATE_NORMAL);

    for (i = 0; i < NET_FMS_PACKET_QUE; i++)
    {
        ptrque = &(g_fms_packet_que[i]);
        
        if (ptrque->state == 1 && ptrque->recvid == ptrhead->shopcode)
        {
            g_fms_alive_check_send_cnt = 0;
            // recv packet delete
            ptrque->state = 0;
            break;
        }
    }

    // 프로토콜 확인
    if ((ptrhead->shprotocol & 0xffff) != OP_FMS_PROT)
    {
        print_dbg(DBG_INFO, 1, "###########  shprotocol(%04X) error ....!!!", (ptrhead->shprotocol & 0xff));
        return ERR_RETURN;
    }

    // 패킷파싱
    switch(ptrhead->shopcode)
    {
    case EFT_NETWORK_CONFIRM_REQ:
        return ERR_SUCCESS;
    case EFT_NETWORK_CONFIRM_RES:
        {
            print_dbg(DBG_NONE, 1, "[PACKET RECV] EFT_NETWORK_CONFIRM_RES");
            g_fms_confirm_state = 2;
            send_fms_alive_check();
        }
        return ERR_SUCCESS;
    }

    // recv packet delete
    if (ptrhead->shopcode == EFT_EVT_BASEDATUPDATE_REQ)
    {
        int ectlcmd = ((fclt_evt_dbdataupdate_t*)ptrbody)->ectlcmd;
        if (ectlcmd == EFCLTCTLCMD_WORKRUN)
        {
            ptrque = empty_packet_que();

            print_dbg(DBG_INFO, 1, "EFT_EVT_BASEDATUPDATE_REQ CMD(%d) TIMEOUT SET", ectlcmd);
            net_packet_rule_init(ptrque);
            ptrque->sendid = EFT_EVT_BASEDATUPDATE_RES;
            ptrque->state = 1;
            net_packet_fms_rule_make(ptrque);
        }
    }


    if (net_packet_parser(ptrhead, ptrbody, &g_netcomm) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "parser packet protocol error .... %d [%s:%s] ", ptrhead->shopcode, __FILE__,__FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
static int send_fms_packet(void * ptrsock, void * ptrhead, void * ptrbody)
{
    net_sock_t * ptrclnt = (&g_netsock);
    net_packet_t * ptrque = NULL;
    int i;

    if ( ptrsock == NULL || ptrhead == NULL || ptrbody == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_FAIL;
    }

    print_dbg( DBG_INFO, 1, "packet send socket(%d)[%s:%d] start..... [%s:%d:%s] ", ptrclnt->clntsock, ptrclnt->clntip, ptrclnt->clntport, __FILE__, __LINE__, __FUNCTION__ );

    if (g_fms_confirm_state == 1)
    {
        print_dbg( DBG_NONE, 1, "packet send skip..... fms confirm state (%d)", g_fms_confirm_state);
        return ERR_RETURN;
    }

    if (((fms_packet_header_t*)ptrhead)->shopcode == EFT_EVT_BASEDATUPDATE_RES)
    {
        // 데이터베이스 명령응답시 타임아웃해제
        int ectlcmd = ((fclt_evt_dbdataupdate_t*)ptrbody)->ectlcmd;
        if (ectlcmd == EFCLTCTLCMD_WORKCOMPLETE)
        {
            print_dbg(DBG_INFO, 1, "EFT_EVT_BASEDATUPDATE_REQ CMD(%d)", ectlcmd);
            set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
            for (i = 0; i < NET_FMS_PACKET_QUE; i++)
            {
                ptrque = &(g_fms_packet_que[i]);
                if (ptrque->state == 1 && ptrque->sendid == EFT_EVT_BASEDATUPDATE_RES)
                {
                    net_packet_rule_init(ptrque);
                }
            }
        }
        if (ptrclnt->clntsock != -1)
        {
            send(ptrclnt->clntsock, ptrhead, sizeof(fms_packet_header_t), 0);
            send(ptrclnt->clntsock, ptrbody, ((fms_packet_header_t *)ptrhead)->nsize, 0);
        }
    }
    else
    {
        ptrque = empty_packet_que();

        if ( ptrque != NULL && ptrclnt->clntsock != -1)
        {
            ptrque->sendid = ((fms_packet_header_t*)ptrhead)->shopcode;
            ptrque->state = 1;
            net_packet_fms_rule_make(ptrque);

            send(ptrclnt->clntsock, ptrhead, sizeof(fms_packet_header_t), 0);
            send(ptrclnt->clntsock, ptrbody, ((fms_packet_header_t *)ptrhead)->nsize, 0);
        }

    }

    print_dbg( DBG_NONE, 1, "packet send end..... fms confirm state (%d)", g_fms_confirm_state);

    return ERR_SUCCESS;
}


static int send_fms_confirm_req()
{
    fms_packet_header_t     head;
    fclt_network_confirm_t  body;

    print_dbg(DBG_NONE, 1, "[PACKET SEND] EFT_NETWORK_CONFIRM_REQ");

    memset(&head, 0, sizeof(head));
    memset(&body, 0, sizeof(body));

    body.nsiteid = get_my_siteid_shm();
    body.eremoteid = EFMSREMOTE_RTU;
    body.iresult = 0;

    //g_fms_alive_check_send_time = time(NULL);

    make_fms_packet_head(&head, EFT_NETWORK_CONFIRM_REQ, sizeof(body));

    send_fms_packet(&g_netsock, &head, &body);

    g_fms_confirm_state = 1;

    return ERR_SUCCESS;
}

static int send_fms_alive_check()
{
    fms_packet_header_t head;
    fclt_fmssvr_alivecheckack_t body;

    print_dbg(DBG_NONE, 1, "[PACKET SEND] EFT_NETALIVECHK_REQ");

    memset(&head, 0, sizeof(head));
    memset(&body, 0, sizeof(body));

    body.eremoteid = EFMSREMOTE_RTU;
    body.iresult = 0;

    g_fms_alive_check_send_time = time(NULL);
    g_fms_alive_check_send_cnt += 1;

    print_dbg(DBG_NONE, 1, "NET ALIVE CHK CNT(%d)", g_fms_alive_check_send_cnt);

    make_fms_packet_head(&head, EFT_NETALIVECHK_REQ, sizeof(body));

    send_fms_packet(&g_netsock, &head, &body);

    return ERR_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////


/* CONTROL module에  보낼 데이터(제어, 설정, 알람)가  있을 경우 호출 */
static int send_data_control( UINT16 inter_msg, void * ptrdata  )
{
	if ( g_ptrsend_data_control_func != NULL )
	{
		return g_ptrsend_data_control_func( inter_msg, ptrdata );
	}

	return ERR_RETURN;
}

static int make_fms_packet_head(void * ptrhead, int cmd, int nsize)
{
    fms_packet_header_t * ptr = ( fms_packet_header_t * ) ptrhead;
    ptr->shprotocol = OP_FMS_PROT;   // (2 BYTE) op_fms_prot
    ptr->shopcode = cmd;     // (2 BYTE) ecmd_rms_eqptatus_req,, 
    ptr->nsize = nsize;        // (4 BYTE) 패킷헤더를 뺀 나머지 body 크기
    ptr->shdirection = ESYSD_RTUSVR;  // (2 BYTE) enumfms_sysdir

    return ERR_SUCCESS;
}

static net_packet_t * empty_packet_que()
{
    net_packet_t * ptrque = NULL;
    int ique = -1;

    if (g_fms_packet_que != NULL)
    {
        lock_sem( g_sem_id, g_sem_key );

        if ((ique = net_packet_empty_que(g_fms_packet_que, NET_FMS_PACKET_QUE)) == -1)
        {
            print_dbg( DBG_ERR, 1, "Is full packet que [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
            unlock_sem( g_sem_id , g_sem_key );
            return NULL;
        }

        ptrque = &(g_fms_packet_que[ique]);

        net_packet_rule_init(ptrque);

        unlock_sem( g_sem_id , g_sem_key );
    }

    return ptrque;
}

//////////////////////////////////////////////////////////////////////////

/* control로 부터 sensor data를 받아 처리하는 함수 */
static int rtusvr_sns_data_res( unsigned short inter_msg, void * ptrdata )
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_sns_data_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_sns_data_res(inter_msg, ptrdata, &g_netcomm);
}

/* RTU센서 모니터링 데이터 실시간 수집 */
static int rtusvr_sns_cudata_res( unsigned short inter_msg, void * ptrdata )
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_sns_cudata_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_sns_cudata_res(inter_msg, ptrdata, &g_netcomm);
}


//장애(FAULT)이벤트
static int rtusvr_sns_evt_fault(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_sns_evt_fault [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_sns_evt_fault(inter_msg, ptrdata, &g_netcomm);
}

//전원제어 (ON/OFF)
static int rtusvr_sns_ctl_power(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_sns_ctl_power [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_sns_ctl_power(inter_msg, ptrdata, &g_netcomm);

}

//전원제어에 대한 결과
static int rtusvr_sns_ctl_res( unsigned short inter_msg, void * ptrdata )
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_sns_ctl_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_sns_ctl_res(inter_msg, ptrdata, &g_netcomm);
}



//RTU센서 임계기준 설정
static int rtusvr_sns_ctl_thrld(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_sns_ctl_thrld [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_sns_ctl_thrld(inter_msg, ptrdata, &g_netcomm);
}

static int rtusvr_sns_completed_thrld(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_sns_completed_thrld [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_sns_completed_thrld(inter_msg, ptrdata, &g_netcomm);
}

/* 감시장비 모니터링 데이터 */
static int rtusvr_eqp_data_res( unsigned short inter_msg, void * ptrdata )
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_eqp_data_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_eqp_data_res(inter_msg, ptrdata, &g_netcomm);
}

//상태 정보(실시간) 송신 프로세스 시작/종료
static int rtusvr_eqp_cudata_res( unsigned short inter_msg, void * ptrdata )
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_eqp_cudata_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_eqp_cudata_res(inter_msg, ptrdata, &g_netcomm);
}

//장애(FAULT)이벤트
static int rtusvr_eqp_evt_fault(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_eqp_evt_fault [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_eqp_evt_fault(inter_msg, ptrdata, &g_netcomm);
}

//감시장비 전원제어 (ON/OFF)
static int rtusvr_eqp_ctl_power(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_eqp_ctl_power [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_eqp_ctl_power(inter_msg, ptrdata, &g_netcomm);
}


//전원제어에 대한 진행상태 정보
static int rtusvr_eqp_ctl_res( unsigned short inter_msg, void * ptrdata )
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_eqp_ctl_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_eqp_ctl_res(inter_msg, ptrdata, &g_netcomm);
}

//감시장비 항목별 임계 설정
static int rtusvr_eqp_ctl_thrld(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_eqp_ctl_thrld [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_eqp_ctl_thrld(inter_msg, ptrdata, &g_netcomm);
}

//임계기준 수정 완료를 OP에게 알려줌.
static int rtusvr_eqp_completed_thrld(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_eqp_completed_thrld [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_eqp_completed_thrld(inter_msg, ptrdata, &g_netcomm);
}


//시간동기화 : RTU - SVR - OP 간 시간 동기화
static int rtusvr_timedat_req(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_timedat_req [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_timedat_req(inter_msg, ptrdata, &g_netcomm);
}

// 데이터베이스 변경
static int rtusvr_database_update_res(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_database_update_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_database_update_res(inter_msg, ptrdata, &g_netcomm);
}


static int rtusvr_evt_allim_msg(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;
    allim_msg_t * ptrbody = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_evt_allim_msg [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrbody = ( allim_msg_t * ) ptrmsg->ptrbudy;

    switch (ptrbody->allim_msg)
    {
    case ALM_RTU_NET_CHG_NOTI_MSG: /* Control -> transfer(FMS, OP, Update ) */
        {
            // RTU장비 네트워크 아이피 또는 포트 변경이 일어난경우
            // 시스템 리부팅 해제처리 [10/22/2016 redIce]
        }
        break;
    case ALM_FMS_NET_CHG_NOTI_MSG:
        {
            connect_close();
            set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
        }
        break;
    }

    return ERR_SUCCESS;
}

static int rtusvr_evt_network_err(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"rtusvr_evt_network_err [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    return ctl_evt_network_err(inter_msg, ptrdata, &g_netcomm);
}


/* CONTROL Module로 부터 수집 데이터, 알람, 장애  정보를 받는 내부 함수로
   등록된 proc function list 중에서 control에서  넘겨 받은 inter_msg와 동일한 msg가 존재할 경우
   해당  함수를 호출하도록 한다. 
 */
static int internal_recv_sns_data( unsigned short inter_msg, void * ptrdata )
{
	int i;
	int proc_func_cnt = 0;
	
	proc_func  ptrtemp_proc_func = NULL;
	UINT16 list_msg = 0;

	if( ptrdata == NULL )
	{
		print_dbg( DBG_INFO, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* proc function list에 등록된 개수 파악 */
	proc_func_cnt= ( sizeof( g_proc_func_list ) / sizeof( proc_func_t ));

	/* 등록된 proc function에서 inter_msg가 동일한 function 찾기 */
	for( i = 0 ; i < proc_func_cnt ; i++ )
	{    
		list_msg 			= g_proc_func_list[ i ].inter_msg; 	/* function list에 등록된 msg id */
		ptrtemp_proc_func 	= g_proc_func_list[ i ].ptrproc;	/* function list에 등록된 function point */

		if ( ptrtemp_proc_func != NULL && list_msg == inter_msg )
		{    
			ptrtemp_proc_func( inter_msg, ptrdata );
			break;
		}    
	} 

	return ERR_SUCCESS;
}

/* fms_client module 초기화 내부 작업 */
static int internal_init( void )
{
    int ret = ERR_SUCCESS;

	/* 데이터 동기화를 위한  세마포어를 생성한다 
		사용법 
		lock_sem( g_sem_id, g_sem_key );
		data 처리 
		unlock_sem( g_sem_id , g_sem_key );
	 */
	g_sem_id = create_sem( g_sem_key );
	print_dbg( DBG_INFO, 1, "FMS Client SEMA : %d", g_sem_id );

	/* warring을 없애기 위한 함수 호출 */
	send_data_control( 0, NULL );
    //send_packet(NULL, NULL);

    memset(&g_netsock, 0, sizeof(g_netsock));
    memset(&g_netcomm, 0, sizeof(g_netcomm));
    g_netsock.clntsock = -1;
    g_netcomm.ptrsock = &g_svrsock;
    g_netcomm.make_header = make_fms_packet_head;
    g_netcomm.send_packet = send_fms_packet;
    g_netcomm.send_ctlmsg = send_data_control;

	print_dbg(DBG_INFO, 1, "Success of FMS Client Initialize");

    net_connect_db();

    create_fms_packet_que();
    init_fms_packet_que();

    if ( ret == ERR_SUCCESS )
    {
        if ((ret = connect_fmssvr()) == ERR_SUCCESS)
        {
            ret = create_fms_thread( );
        }
    }
	return ERR_SUCCESS;
}

/* fms_client module loop idle 내부 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
static int internal_idle( void * ptr )
{
    int ret = ERR_SUCCESS;
	static int cnt = 0;
	( void )ptr;

    if( g_release == 0 )
    {
        // fms server state check
        if (get_fms_server_net_sts() != NET_FMS_STATE_NORMAL || g_netsock.clntsock <= 0)
        {
            if ((time(NULL) - g_fms_connect_check_time) > 3)
            {
				if ( cnt++ >4 )
				{
					if ( get_print_debug_shm() == 1 )
					{
						print_line();
						print_dbg(DBG_INFO, 1, "fms server connect retry !!!!");
					}
					cnt = 0;
				}

                if ((ret = connect_fmssvr()) == ERR_SUCCESS)
                {
                    ret = create_fms_thread( );
                }
            }
        }

        // packet que check proc
        {
            net_packet_t* ptrque = NULL;
            int i;
            for (i = 0; i < NET_FMS_PACKET_QUE; i++)
            {
                ptrque = &(g_fms_packet_que[i]);

                if (ptrque->sendid == 0 || ptrque->recvid == 0 || ptrque->timeout == 0)
                {
                    net_packet_rule_init(ptrque);
                }

                // send pakcet time out check
                if (ptrque->state == 1 && net_packet_timeoutcheck(ptrque))
                {
                    // timeout proc
                    if (ptrque->sendid == EFT_EVT_BASEDATUPDATE_RES)
                    {
                        set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
                    }

                    ptrque->state = 0;
                    print_dbg(DBG_INFO, 1, "[PACKET TIMEOUT] PACKID(%d) [%s:%d:%s]", ptrque->sendid, __FILE__, __LINE__, __FUNCTION__);
                    switch (ptrque->sendid)
                    {
                    case EFT_NETALIVECHK_REQ:
                        if (g_fms_alive_check_send_cnt < 3)
                        {
                            continue;
                        }
                    default:
                        {
                            connect_close();
                            init_fms_packet_que();
                            set_fms_server_net_sts(NET_FMS_STATE_ERRORS);
                        }
                        break;
                    }
                }
            }
        }

        // network fmserver alive check send message
        {
            if (g_netsock.clntsock > 0 && (time(NULL) - g_fms_alive_check_send_time) > NET_FMS_ALIVE_CHECK_TIME)
            {
                send_fms_alive_check();
            }
        }

#if 0
		static int test_cnt = 0;
		if (g_netsock.clntsock > 0)
		{
			if ( test_cnt++ > 20 )
			{
				connect_close();
				init_fms_packet_que();
				set_fms_server_net_sts(NET_FMS_STATE_ERRORS);
				test_cnt = 0;
			}
		}
#endif
	}

	return ret;
}

/* fms_client module의 release 내부 작업 */
static int internal_release( void )
{
    connect_close();

    g_release = 1;

	if ( g_sem_key > 0 )
	{
		destroy_sem( g_sem_id, g_sem_key );
		g_sem_key = -1;
	}

	print_dbg(DBG_INFO, 1, "Release of FMS Client");

    release_fms_packet_que();

    net_close_db();

	return ERR_SUCCESS;
}

/*================================================================================================
외부  함수  정의 
================================================================================================*/
int init_fms_client( send_data_func ptrsend_data )
{
	/* CONTROL module의 함수 포인트 등록 */
	g_ptrsend_data_control_func = ptrsend_data ;	/* CONTROL module로 설정, 제어, 알람 , 장애  데이터를 보낼 수 있는 함수 포인터 */

	/* 기타 초기화 작업 */
	internal_init();

	return ERR_SUCCESS;
}

/* fms_client module loop idle 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
int idle_fms_client( void * ptr )
{
	return internal_idle( ptr );
}

/* 동적 메모리, fd 등 resouce release함수 */
int release_fms_client( void )
{
	/* release 작업  */
	return internal_release(); 
}

/* CONTROL Module로 부터 수집 데이터, 장애, 알람를 받는 외부 함수 */
int recv_sns_data_fms_client( unsigned short inter_msg, void * ptrdata )
{
	return internal_recv_sns_data( inter_msg, ptrdata );
}
