/*********************************************************************
	file : op_server.c
	description :
	product by tory45( tory45@empal.com) ( 2016.08.05 )
	update by ?? ( . . . )
	
	- Control에서 fms_server로 보낼 데이터를 받아 fms_server로 보내는 기능
	- fms_server에서 데이터를 받아 Control로 정보를 보내는 기능 
 ********************************************************************/
#include "com_include.h"
#include <sys/epoll.h>
#include <netinet/tcp.h>

#include "sem.h"
#include "net.h"
#include "op_server.h"
#include "update_client.h"

//#define USE_ET
//#define USE_ETONESHOT

/*================================================================================================
 전역 변수 
 1.static 적용 
 2.숫자형 데이터 일경우 volatile 적용 
 3.초기값 적용.
================================================================================================*/
static volatile int g_sem_key = OP_SERVER_KEY;					/* OP Server용 세마포어 키 */
static volatile int g_sem_id = 0;								/* OP Server용 세마포어 아이디 */
static send_data_func  g_ptrsend_data_control_func = NULL;		/* CONTROL 모듈  설정,제어, 알람  정보를 넘기는 함수 포인트 */

//static net_sock_t g_client_que[NET_MAX_CLIENT];// = NULL;
static net_sock_t * g_client_que = NULL;
static struct epoll_event * g_epollevents; 
static volatile int g_serverfd = -1;
static volatile int g_epollfd = -1; 
static volatile int g_server_port = 0;
static volatile int g_dbchange_check = 0;

/* thread ( fop client )*/
static volatile int g_start_fop_pthread 	= 0; 
static volatile int g_end_fop_pthread 		= 1; 
static pthread_t fop_pthread;
static void * fop_pthread_idle( void * ptrdata );
static volatile int g_release				= 0;

static net_comm_t g_netcomm;
static net_packet_t g_cmd_packet_que;



/*================================================================================================
  내부 함수 선언
  static 적용 
================================================================================================*/
static net_sock_t * empty_client_que( void );
static int recv_packet(net_sock_t * ptrclnt, char * data, int len);
static int send_fop_packet(void * ptrsock, void * ptrhead, void * ptrbody);
static int make_fop_packet_head(void * ptrhead, int cmd, int nsize);

static int init_netcomm(net_sock_t * ptrclnt);

static net_sock_t * get_client(int sock);

static void print_accept_client_list();
#ifdef USE_ET 
static int setnonblock(int sock);
#endif
static int settcpnodelay(int sock);
static int setreuseaddr(int sock);
static int settcpkeepalive(int sock);
static int init_epoll(int eventsize);
static int init_acceptsock(unsigned short port); 
static int do_use_fd(int efd, struct epoll_event ev);
static int do_accept(int efd, int sfd);
int do_add_fd(int efd, int cfd);
int do_modify_fd(int efd, int cfd);
int do_del_fd(int efd, int cfd);
static int init_client_que(net_sock_t * ptrdata);
static int create_client_que( void );
static int release_client_que( void );
static int close_client(int efd, int cfd, int readn);
static int connet_server( void );
static int close_server( void );
static void* fop_pthread_idle( void * ptrdata );
static int create_fop_thread( void );

static int parser_packet(net_sock_t * ptrclnt, fms_packet_header_t* ptrhead, void * ptrbody);

//////////////////////////////////////////////////////////////////////////

static int send_data_control_op_close_noti( int clntsock );

//////////////////////////////////////////////////////////////////////////

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

static int rtusvr_eqp_sts_resp(unsigned short inter_msg, void * ptrdata);
static int rtusvr_eqp_net_sts_resp(unsigned short inter_msg, void * ptrdata);
static int rtusvr_eqp_off_opt_resp(unsigned short inter_msg, void * ptrdata);
static int rtusvr_eqp_on_opt_resp(unsigned short inter_msg, void * ptrdata);
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
    { EFT_SNSRCTLCOMPLETED_THRLD,   rtusvr_sns_completed_thrld }, //임계기준 수정 완료를 OP에게 알려줌.
    { EFT_EQPDAT_RES,			rtusvr_eqp_data_res },//감시장비 모니터링 데이터
    { EFT_EQPCUDAT_RES,			rtusvr_eqp_cudata_res }, //상태 정보(실시간) 송신 프로세스 시작/종료
    { EFT_EQPEVT_FAULT,			rtusvr_eqp_evt_fault }, //장애(FAULT)이벤트
    { EFT_EQPCTL_POWER,			rtusvr_eqp_ctl_power }, //감시장비 전원제어 (ON/OFF)
    { EFT_EQPCTL_RES,			rtusvr_eqp_ctl_res }, //전원제어에 대한 진행상태 정보
    { EFT_EQPCTL_THRLD,			rtusvr_eqp_ctl_thrld }, //감시장비 항목별 임계 설정
    { EFT_EQPCTLCOMPLETED_THRLD,    rtusvr_eqp_completed_thrld }, //임계기준 수정 완료를 OP에게 알려줌.
    { EFT_TIMEDAT_REQ,			rtusvr_timedat_req }, //시간동기화 : RTU - SVR - OP 간 시간 동기화
    { EFT_EVT_BASEDATUPDATE_RES , rtusvr_database_update_res },
    { EFT_ALLIM_MSG,            rtusvr_evt_allim_msg },
    { EFT_EVT_FCLTNETWORK_ERR,  rtusvr_evt_network_err },

    { EFT_EQPCTL_STATUS_RES,  	rtusvr_eqp_sts_resp },
    { EFT_EQPCTL_CNNSTATUS_RES, rtusvr_eqp_net_sts_resp },
    { EFT_EQPCTL_POWEROPT_OFFRES,  rtusvr_eqp_off_opt_resp },
    { EFT_EQPCTL_POWEROPT_ONRES,  rtusvr_eqp_on_opt_resp },

	{ 0, NULL }
};

/*================================================================================================
내부 함수  정의 
매개변수가 pointer일경우 반드시 NULL 체크 적용 
================================================================================================*/

static int send_data_control_op_close_noti( int clntsock )
{
    //#define ALM_OP_CLOSE_NOTI_MSG         0x0102  /* Op ->Control */

    inter_msg_t inter_msg;
    allim_msg_t allim_msg;
    UINT16 cmd = EFT_ALLIM_MSG;

    memset( &inter_msg, 0, sizeof( inter_msg ));
    memset( &allim_msg, 0, sizeof( allim_msg ));

    /* allim msg */
    allim_msg.allim_msg = ALM_OP_CLOSE_NOTI_MSG;

    /* intr msg */
    inter_msg.client_id    = clntsock;
    inter_msg.msg          = cmd;
    inter_msg.ptrbudy      = &allim_msg;

    send_data_control(cmd, &inter_msg);

    return ERR_RETURN;
}


/* CONTROL module에  보낼 데이터(제어, 설정, 알람)가  있을 경우 호출 */
static int send_data_control( UINT16 inter_msg, void * ptrdata  )
{
	if ( g_ptrsend_data_control_func != NULL )
	{
		return g_ptrsend_data_control_func( inter_msg, ptrdata );
	}

	return ERR_RETURN;
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

/* op_server module 초기화 내부 작업 */
static int internal_init( void )
{
	/* 데이터 동기화를 위한  세마포어를 생성한다 
		사용법 
		lock_sem( g_sem_id, g_sem_key );
		data 처리 
		unlock_sem( g_sem_id , g_sem_key );
	 */
	g_sem_id = create_sem( g_sem_key );
	print_dbg( DBG_INFO, 1, "OP Server  SEMA : %d", g_sem_id );

	/* warring을 없애기 위한 함수 호출 */
	send_data_control( 0, NULL );

    memset(&g_netcomm, 0, sizeof(g_netcomm));
    g_netcomm.ptrsock = NULL;
    g_netcomm.make_header = make_fop_packet_head;
    g_netcomm.send_packet = send_fop_packet;
    g_netcomm.send_ctlmsg = send_data_control;

    net_packet_rule_init(&(g_cmd_packet_que));

	print_dbg(DBG_INFO, 1, "Success of OP Server Initialize");

    connet_server();

    create_fop_thread();

	return ERR_SUCCESS;
}

/* op_server module loop idle 내부 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
static int internal_idle( void * ptr )
{
    int ret = ERR_SUCCESS;

	( void )ptr;

    if( g_release == 0 )
    {
        // op server connect client list
        {
            print_accept_client_list();
        }

        // send pakcet time out check
        {
            net_packet_t * ptrque = &(g_cmd_packet_que);

            if (ptrque->state == 1 && net_packet_timeoutcheck(ptrque))
            {
                // timeout proc
                print_dbg(DBG_INFO, 1, "[PACKET TIMEOUT] PACKID(%d) [%s:%d:%s]", ptrque->sendid, __FILE__, __LINE__, __FUNCTION__);
                set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
                net_packet_rule_init(ptrque);
            }
        }
    }

	return ret;
}

/* op_server module의 release 내부 작업 */
static int internal_release( void )
{
    g_release = 1;

	if ( g_sem_key > 0 )
	{
		destroy_sem( g_sem_id, g_sem_key );
		g_sem_key = -1;
	}

	print_dbg(DBG_INFO, 1, "Release of OP Server");

    close_server();

	return ERR_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////

static int init_netcomm(net_sock_t * ptrclnt)
{
    if (ptrclnt <= 0 )
    {
		if ( get_print_debug_shm() == 1 )
	        print_dbg(DBG_INFO, 1, "Is null ptrclnt (%d).... [%s:%d:%s]", ptrclnt, __FILE__, __LINE__, __FUNCTION__);
    }
    g_netcomm.ptrsock = ptrclnt;
    g_netcomm.make_header = make_fop_packet_head;
    g_netcomm.send_packet = send_fop_packet;
    g_netcomm.send_ctlmsg = send_data_control;
    return ERR_SUCCESS;
}

static void print_accept_client_list()
{
    static volatile long accept_check_time = 0;

    int i, cnt = 0;

    if (g_client_que != NULL && (time(NULL) - accept_check_time) > 10)
    {
        time_t cur;
        struct tm tm;

        accept_check_time = cur = time(NULL);
        memset( &tm, 0, sizeof( struct tm ));
        localtime_r( &cur, &tm );

        print_line();

        for (i = 0; i < NET_MAX_CLIENT; i++)
        {
            if (g_client_que[i].isalive == 1)
            {
                cnt++;
            }
        }
        
        print_dbg(DBG_INFO, 1, "ACCEPT SOCK COUNT(%d)  [ %04d-%02d-%02d %02d:%02d:%02d ] ########",
            cnt, 
            tm.tm_year + 1900, tm.tm_mon + 1,tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec); 

        for (i = 0; i < NET_MAX_CLIENT; i++)
        {
            if (g_client_que[i].isalive == 1)
            {
                print_dbg(DBG_NONE, 1, "   IDX(%d) CLNTFD(%d) IP(%s) PORT(%d)", i, g_client_que[i].clntsock, g_client_que[i].clntip, g_client_que[i].clntport); 
            }
        }


        print_dbg(DBG_NONE, 1, "");

        print_line();
    }

}

static int get_accept_client_list( void )
{
    int i, cnt = 0;

    if ( g_client_que != NULL )
    {
        for (i = 0; i < NET_MAX_CLIENT; i++)
        {
            if (g_client_que[i].isalive == 1)
            {
                cnt++;
            }
        }
    }
	
	print_dbg( DBG_INFO,1, "Count of Accepted OP CNT:%d", cnt );
	return cnt;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef USE_ET 
static int setnonblock(int sock) 
{ 
    int flags = fcntl (sock, F_GETFL); 
    flags |= O_NONBLOCK; 
    if (fcntl (sock, F_SETFL, flags) < 0) 
    { 
        print_dbg(DBG_ERR, 1, "fcntl, executing nonblock error"); 
        return -1; 
    } 
    return 0; 
} 
#endif
static int settcpnodelay(int sock) 
{ 
    int flag = 1; 
    int result = setsockopt(sock,    /* socket affected */ 
        IPPROTO_TCP,    /* set option at TCP level */ 
        TCP_NODELAY,    /* name of option */ 
        (char *) &flag,    /* the cast is historical  cruft */ 
        sizeof(int));    /* length of option value */ 
    return result; 
} 

static int setreuseaddr(int sock) 
{ 
    int flag = 1; 
    int result = setsockopt(sock,    /* socket affected */ 
        SOL_SOCKET,    /* set option at TCP level */ 
        SO_REUSEADDR,    /* name of option */ 
        (char *) &flag,    /* the cast is historical  cruft */ 
        sizeof(int));    /* length of option value */ 
    return result; 
} 

static int settcpkeepalive(int sock)
{ 
    int result = 0;
    int so_keepalive;
    struct linger so_linger;

    so_keepalive = 1;
    so_linger.l_onoff	= 1;
    so_linger.l_linger	= 3;
    setsockopt( sock, SOL_SOCKET, SO_KEEPALIVE, &so_keepalive, sizeof(so_keepalive) );
    setsockopt( sock, SOL_SOCKET, SO_LINGER,	 &so_linger,	sizeof(so_linger) );

    return result; 
} 



static int init_epoll(int eventsize) 
{ 
    int efd; 

    // init epoll 
    if ((efd = epoll_create(eventsize)) < 0) 
    { 
        print_dbg(DBG_ERR, 1, "epoll_create (1) error"); 
        return -1; 
    } 
    return efd; 
} 

static int init_acceptsock(unsigned short port) 
{ 
    struct sockaddr_in addr; 

    int sfd = socket(AF_INET, SOCK_STREAM, 0); 

    if (sfd == -1) 
    { 
        print_dbg(DBG_ERR, 1, "socket error :"); 
        net_connect_close(sfd); 
        return ERR_FAIL; 
    } 

    setreuseaddr(sfd); 

    addr.sin_family = AF_INET; 
    addr.sin_port = htons (port); 
    addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    if (bind(sfd, (struct sockaddr *) &addr, sizeof (addr)) == -1) 
    { 
        print_dbg(DBG_ERR, 1, "bind error port (%d)!!!\n", port);
        net_connect_close(sfd); 
        return ERR_FAIL; 
    }
    if (listen(sfd, NET_MAX_CLIENT) == ERR_FAIL)
    {
        print_dbg(DBG_ERR, 1, "listen error max clnt (%d) !!!\n", NET_MAX_CLIENT);
        net_connect_close(sfd);
        return ERR_FAIL;
    }
    return sfd; 
} 

static int close_client(int efd, int cfd, int readn)
{
    int i, idx = -1;

    if (g_client_que == NULL)
    {
        print_dbg(DBG_ERR, 1, "client que is null..... [%s:%s]",  __FILE__, __FUNCTION__); 

        return -1;
    }

    for (i = 0; i < NET_MAX_CLIENT; i++)
    {
        if (g_client_que[i].isalive == 1 && g_client_que[i].clntsock == cfd)
        {
            idx = i;
            print_dbg(DBG_INFO, 1, "@@@@@@@@@@@@ op_close_noti idx(%d) fd(%d) by (%d) [%s:%s]", idx, cfd, readn, __FILE__, __FUNCTION__); 
            init_client_que(&(g_client_que[i]));
            break;
        }
    }

    send_data_control_op_close_noti(cfd);
    do_del_fd(efd, cfd);
    net_connect_close(cfd); 
    print_line();
    print_dbg(DBG_INFO, 1, "@@@@@@@@@@@@ client close idx(%d) fd %d by %d [%s:%s]", idx, cfd, readn, __FILE__, __FUNCTION__); 

    return -1; 
}

static int do_use_fd(int efd, struct epoll_event ev) 
{ 
    int readn = 0; 
    int cfd = ev.data.fd; 

    static char readbuf[512] = { 0, }; 
    int i;

#ifdef USE_ET 

#if 1
    while(1)
    { 
    
        memset(readbuf, 0, sizeof(readbuf));

        if ((readn = read(cfd, readbuf, 2048)) < 0)
        {
            if (errno == EINTR)
            {
                print_dbg(DBG_INFO, 1, "$$$$$$$$$$$$$$$$$$ EINTR fd %d [%s:%s]", cfd, __FILE__, __FUNCTION__); 

                continue; /* perfectly normal; try again */
            }
            else if (errno == EAGAIN)
            {
                print_dbg(DBG_ERR, 1, "$$$$$$$$$$$$$$$$$$ EAGAIN fd %d [%s:%s]", cfd, __FILE__, __FUNCTION__); 
                break; /* perfectly normal; try again */
#if 0
                fd_set rset;
                struct timeval timeout;
                int i;

                memset( &timeout, 0, sizeof( struct timeval ));

                print_dbg(DBG_ERR, 1, "$$$$$$$$$$$$$$$$$$ EAGAIN fd %d [%s:%s]", cfd, __FILE__, __FUNCTION__); 

                FD_ZERO(&rset);
                FD_SET(cfd, &rset);
                timeout.tv_sec = 0;
                timeout.tv_usec = 100;
                
                i = select (cfd + 1, &rset, NULL, NULL, &timeout);

                if (i < 0)
                {
                    /* error; log/die/whatever and close() socket */

                    print_dbg(DBG_ERR, 1, "$$$$$$$$$$$$$$$$$$ EINTR (i < 0) fd %d [%s:%s]", cfd, __FILE__, __FUNCTION__); 

                    return close_client(efd, cfd, readn);
                }
                else if (i == 0)
                {
                    print_dbg(DBG_ERR, 1, "$$$$$$$$$$$$$$$$$$ EINTR (i == 0) fd %d [%s:%s]", cfd, __FILE__, __FUNCTION__); 

                    /* timed out without receiving any data; log/die/whatever and close() */
                    break;
                }
                else
                {
                    print_dbg(DBG_ERR, 1, "$$$$$$$$$$$$$$$$$$ EINTR (i > 0) fd %d [%s:%s]", cfd, __FILE__, __FUNCTION__); 

                    /* else, socket is now readable, so loop back up and do the read() again */
                    break;
                }
#endif
            }
            else 
            {
                /* some real error; log/die/whatever and close() socket */
                print_dbg(DBG_ERR, 1, "$$$$$$$$$$$$$$$$$$ EINTR EINTR NOT fd %d [%s:%s]", cfd, __FILE__, __FUNCTION__); 

                return close_client(efd, cfd, readn);
            }
        }
        else if (readn == 0)
        {
            /* the connection has been closed by your peer; clean-up and close() */
            return close_client(efd, cfd, readn);
        }
        else
        {
            /* you got some data; do whatever with it... */
            net_sock_t * ptrclnt = NULL;

            if (g_client_que == NULL)
            {
                print_dbg(DBG_ERR, 1, "client que is null..... fd %d [%s:%s]", cfd, __FILE__, __FUNCTION__); 

                return -1;
            }

            for (i = 0; i < NET_MAX_CLIENT; i++)
            {
                if (g_client_que[i].isalive == 1 && g_client_que[i].clntsock == cfd)
                {
                    ptrclnt = &(g_client_que[i]);
                    break;
                }
            }

            if (ptrclnt == NULL)
            {
                print_dbg(DBG_ERR, 1, "client que find error fd %d ", cfd); 
                return -1;
            }

			if ( get_print_debug_shm() == 1 )
			{
				print_line();
				print_dbg(DBG_NONE, 1, "[%s:%d] client reads data size : %d", ptrclnt->clntip, ptrclnt->clntport, readn);
			}

            recv_packet(ptrclnt, readbuf, readn);
        }


        //////////////////////////////////////////////////////////////////////////////////////////////////////////
#else
        if ((readn = read(cfd, readbuf, 512)) <= 0)
        { 
            int idx = -1;

            if (EAGAIN == errno )
            { 
                // 비정상종료시 또는 소캣 비정상종료 [10/13/2016 redIce]
                print_dbg(DBG_ERR, 1, "@@@@@@@@@@@@ EAGAIN fd(%d) by (%d) [%s:%s]",  cfd, readn, __FILE__, __FUNCTION__);
                break;
            } 

            if (g_client_que == NULL)
            {
                print_dbg(DBG_ERR, 1, "client que is null..... [%s:%s]",  __FILE__, __FUNCTION__); 

                return -1;
            }

            for (i = 0; i < NET_MAX_CLIENT; i++)
            {
                if (g_client_que[i].isalive == 1 && g_client_que[i].clntsock == cfd)
                {
                    idx = i;
                    print_dbg(DBG_INFO, 1, "@@@@@@@@@@@@ op_close_noti idx(%d) fd(%d) by (%d) [%s:%s]", idx, cfd, readn, __FILE__, __FUNCTION__); 
                    init_client_que(&(g_client_que[i]));
                    break;
                }
            }
            send_data_control_op_close_noti(cfd);
            do_del_fd(efd, cfd);
            net_connect_close(cfd); 
            print_line();
            print_dbg(DBG_INFO, 1, "@@@@@@@@@@@@ client close idx(%d) fd %d by %d [%s:%s]", idx, cfd, readn, __FILE__, __FUNCTION__); 
            return -1; 
        }
        else
        {
            net_sock_t * ptrclnt = NULL;

            if (g_client_que == NULL)
            {
                print_dbg(DBG_ERR, 1, "client que is null..... fd %d [%s:%s]", cfd, __FILE__, __FUNCTION__); 

                return -1;
            }

            for (i = 0; i < NET_MAX_CLIENT; i++)
            {
                if (g_client_que[i].isalive == 1 && g_client_que[i].clntsock == cfd)
                {
                    ptrclnt = &(g_client_que[i]);
                    break;
                }
            }

            if (ptrclnt == NULL)
            {
                 print_dbg(DBG_ERR, 1, "client que find error fd %d ", cfd); 
                 return -1;
            }

            print_line();
            print_dbg(DBG_NONE, 1, "[%s:%d] client reads data size : %d", ptrclnt->clntip, ptrclnt->clntport, readn);

            recv_packet(ptrclnt, readbuf, readn);
        }
#endif
    } 
    //print_dbg(DBG_NONE, 1, "=====>> client read data %d, %s", sizen, buf_in); 
    //send(cfd, buf_in, strlen(buf_in), sendflags); 

#ifdef USE_ETONESHOT 
    // re-set fd to epoll 
    do_modify_fd(efd, cfd); 
#endif // USE_ETONESHOT 

#else //#ifdef USE_ET 

    // if it occured ploblems with reading, delete from epoll event pool and close socket 
    if ((readn = read(cfd, readbuf, 512)) < 0) 
    { 
        close_client(efd, cfd, readn);
    }
    else if (readn == 0)
    {
        close_client(efd, cfd, readn);
    }
    else 
    { 
        net_sock_t * ptrclnt = NULL;

        if (g_client_que == NULL)
        {
            print_dbg(DBG_ERR, 1, "client que is null..... fd %d [%s:%s]", cfd, __FILE__, __FUNCTION__); 

            return -1;
        }

        for (i = 0; i < NET_MAX_CLIENT; i++)
        {
            if (g_client_que[i].isalive == 1 && g_client_que[i].clntsock == cfd)
            {
                ptrclnt = &(g_client_que[i]);
                break;
            }
        }

        if (ptrclnt == NULL)
        {
            print_dbg(DBG_ERR, 1, "client que find error fd %d ", cfd); 
            return -1;
        }

        print_line();
        print_dbg(DBG_NONE, 1, "[%s:%d] client reads data size : %d", ptrclnt->clntip, ptrclnt->clntport, readn);

        recv_packet(ptrclnt, readbuf, readn);
    } 
#endif //#ifdef USE_ET 
    return 1; 
} 

static int do_accept(int efd, int sfd) 
{ 
    struct sockaddr_in clntaddr; 
    int cfd; 
    int len = (int)sizeof(clntaddr);
    net_sock_t * ptrque = NULL;

    print_dbg(DBG_INFO, 1, "@@@@@@@@@@@  client accepted .........[%s:%s]", __FILE__, __FUNCTION__); 

    if ((cfd = accept(sfd, (struct sockaddr *)&clntaddr, (socklen_t *)&len)) < 0) 
    { 
        print_dbg(DBG_ERR, 1, "accept error"); 
        return -1; 
    }
    print_dbg(DBG_NONE, 1, "client sock id (%d)", cfd); 


#ifdef USE_ET 
    setnonblock(cfd); 

    print_dbg(DBG_INFO, 1, "@@@@@@@@@@@ setnonblock(cfd)[%s:%s]", __FILE__, __FUNCTION__); 

#endif // USE_ET 
    settcpnodelay(cfd); 

    print_dbg(DBG_INFO, 1, "@@@@@@@@@@@ settcpnodelay(cfd)[%s:%s]", __FILE__, __FUNCTION__); 

    settcpkeepalive(cfd);

    if (do_add_fd(efd, cfd) < 0)
    {
        print_dbg(DBG_ERR, 1, "do_add_fd error"); 
    }
    print_dbg(DBG_INFO, 1, "@@@@@@@@@@@ do_add_fd[%s:%s]", __FILE__, __FUNCTION__); 

    ptrque = empty_client_que();

    print_dbg(DBG_INFO, 1, "@@@@@@@@@@@ empty_client_que(%p)[%s:%s]", ptrque, __FILE__, __FUNCTION__); 

    if (ptrque == NULL)
    {
        print_dbg(DBG_ERR, 1, "client que full error [%s]", __FUNCTION__);
        net_connect_close(cfd);
    }
    else 
    {
        ptrque->clntsock = cfd;
        inet_ntop(AF_INET, &clntaddr.sin_addr.s_addr, ptrque->clntip, sizeof(ptrque->clntip));
        ptrque->clntport = clntaddr.sin_port;
        ptrque->isalive = 1;
        print_dbg(DBG_INFO, 1, "client que (%d) [%s:%d] [%s:%s]", ptrque->clntsock, ptrque->clntip, ptrque->clntport,  __FILE__, __FUNCTION__); 
    }

    return cfd; 
} 

int do_add_fd(int efd, int cfd)
{ 
    struct epoll_event ev; 

    ev.events = EPOLLIN; 
#ifdef USE_ET 
    ev.events |= EPOLLET; 
#ifdef USE_ETONESHOT 
    ev.events |= EPOLLONESHOT; 
#endif // USE_ETONESHOT 
#endif // USE_ET 
    ev.data.fd = cfd; 
    return epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &ev); 
} 

// fixme 
int do_modify_fd(int efd, int cfd) 
{ 
    struct epoll_event ev; 

    ev.events = EPOLLIN; 
#ifdef USE_ET 
    ev.events |= EPOLLET; 
#ifdef USE_ETONESHOT 
    ev.events |= EPOLLONESHOT; 
#endif // USE_ETONESHOT 
#endif // USE_ET 
    ev.data.fd = cfd; 
    return epoll_ctl(efd, EPOLL_CTL_MOD, cfd, &ev); 
} 


int do_del_fd(int efd, int cfd) 
{ 
    struct epoll_event ev; 

    return epoll_ctl(efd, EPOLL_CTL_DEL, cfd, &ev); 
} 


// return value null client que full
static net_sock_t * empty_client_que( void )
{
    int i;

    if (g_client_que == NULL)
    {
        print_dbg(DBG_ERR, 1, "op server cempty_client_que is null ... [%s:%s] ", __FILE__, __FUNCTION__);
        return NULL;
    }

    //lock_sem( g_sem_id, g_sem_key );

    for (i = 0; i < NET_MAX_CLIENT; i++)
    {
        if (g_client_que[i].isalive == 0 && g_client_que[i].clntsock == -1)
        {
            return &(g_client_que[i]);
        }
    }

    //unlock_sem( g_sem_id , g_sem_key );

    return NULL;
}

static int init_client_que(net_sock_t * ptrdata)
{
    memset(ptrdata, 0, sizeof(net_sock_t));
    ptrdata->isalive = 0;
    ptrdata->clntsock = -1;

    return 0;
}

static int create_client_que( void )
{
    net_sock_t * ptrclnt = NULL;
    int i;

    g_client_que = A_ALLOC(net_sock_t, NET_MAX_CLIENT);
    
    //lock_sem( g_sem_id, g_sem_key );

    if (g_client_que == NULL)
    {
        print_dbg(DBG_ERR, 1, "op server client que create error ... [%s:%s] ", __FILE__, __FUNCTION__);
        return ERR_FAIL;
    }

    for (i = 0; i < NET_MAX_CLIENT; i++)
    {
        if ((ptrclnt = &(g_client_que[i])) != NULL)
        {
            init_client_que(ptrclnt);
        }
    }

    //unlock_sem( g_sem_id , g_sem_key );

    return ERR_SUCCESS;
}
static int release_client_que( void )
{
    net_sock_t * ptrclnt = NULL;
    int i;

    //lock_sem( g_sem_id, g_sem_key );

    if (g_client_que == NULL)
    {
        print_dbg(DBG_INFO, 1, "op server client que release null skip ... [%s:%s] ", __FILE__, __FUNCTION__);
        return ERR_RETURN;
    }

    for (i = 0; i < NET_MAX_CLIENT; i++)
    {
        if ((ptrclnt = &(g_client_que[i])) != NULL)
        {
            do_del_fd(g_epollfd, ptrclnt->clntsock);
            net_connect_close(ptrclnt->clntsock);
            init_client_que(ptrclnt);
        }
    }

    A_FREE(g_client_que);

    //unlock_sem( g_sem_id , g_sem_key );

    return 0;
}

static int connet_server( void )
{
    int result = -1; 

    g_server_port = get_op_listen_port_shm();

    print_line();
    print_dbg(DBG_INFO, 1, "connect_fopsvr() [%d] [%s:%d:%s]",
        g_server_port, __FILE__, __LINE__, __FUNCTION__);

    if ((g_epollfd = init_epoll(NET_MAX_EVENTS)) < 0)
    {
        print_dbg(DBG_ERR, 1, "init_epoll error");
        return ERR_FAIL;
    }

    if ((g_epollevents = (struct epoll_event *) malloc (sizeof (*g_epollevents) * NET_MAX_CLIENT)) == NULL)
    {
        print_dbg(DBG_ERR, 1, "epollevent create error");
        return ERR_FAIL;
    }

    if ((g_serverfd = init_acceptsock(g_server_port)) < 0)
    {
        print_dbg(DBG_ERR, 1, "acceptsock error");
        return ERR_FAIL;
    }

    create_client_que();
    print_dbg(DBG_INFO, 1, "@@@@@@@@@@ START SERVER PORT : %d (%d)", g_server_port, g_serverfd);

    if ((result = do_add_fd(g_epollfd, g_serverfd)) < 0)
    {
        print_dbg(DBG_ERR, 1, "epoll ctl error");
        return ERR_FAIL;
    }

    return ERR_SUCCESS;
}
static int close_server( void )
{
    print_line();
    release_client_que();
    print_dbg(DBG_INFO, 1, "@@@@@@@@@@ STOP SERVER PORT : %d", g_server_port);

    do_del_fd(g_epollfd, g_serverfd);
    
    close(g_serverfd); g_serverfd = -1;
    close(g_epollfd); g_epollfd = -1;

    A_FREE(g_epollevents);

    return 0;
}

static void* fop_pthread_idle( void * ptrdata )
{
    ( void ) ptrdata;

    int i, status;
    int max_got_events;

    g_end_fop_pthread = 0;
    g_dbchange_check  = 0;
#if 1
    while ( g_release == 0 && g_start_fop_pthread == 1 && g_epollfd != -1)
    {
        max_got_events = epoll_wait(g_epollfd, g_epollevents, NET_MAX_CLIENT, 100); 

        for (i = 0; i < max_got_events; i++) 
        { 
            if (g_serverfd > 0)
            {
                if (g_epollevents[i].data.fd == g_serverfd) 
                { 
                    do_accept(g_epollfd, g_serverfd); 
                } 
                else 
                { 
                    do_use_fd(g_epollfd, g_epollevents[i]);
                } 
            }
        }
        sys_sleep(1);
    }
#endif
    if( g_end_fop_pthread == 0 )
    {
        pthread_join( fop_pthread , ( void **)&status );
        g_start_fop_pthread 	= 0;
        g_end_fop_pthread 		= 1;
        print_dbg(DBG_INFO, 1, "####### ====== >>> pthread_join end !!!!!!!!!! [%s:%s]", __FILE__, __FUNCTION__);

        if (g_dbchange_check == 1)
        {
            print_dbg(DBG_INFO, 1, "####### ====== >>> database change process start !!");
            close_server();
            sys_sleep(100);
            connet_server();
            create_fop_thread();
            print_dbg(DBG_INFO, 1, "####### ====== >>> database change process end.. !!");
        }
    }

    return ERR_SUCCESS;
}

static int create_fop_thread( void )
{
    int ret = ERR_SUCCESS;

    print_line();
    print_dbg(DBG_NONE, 1, "create_fop_thread( %d , %d, %d)", g_release , g_start_fop_pthread, g_end_fop_pthread);

    if ( g_release == 0 && g_start_fop_pthread == 0 && g_end_fop_pthread == 1 )
    {

        g_start_fop_pthread = 1;

        if ( pthread_create( &fop_pthread, NULL, fop_pthread_idle , NULL ) < 0 )
        {
            g_start_fop_pthread = 0;
            print_dbg( DBG_ERR, 1, "Fail to Create fop server Thread" );
            ret = ERR_RETURN;
        }
        else
        {
            print_dbg( DBG_NONE, 1, "Create fop server Thread Create OK. ");
        }
    }

    return ret;
}

static net_sock_t * get_client(int sock)
{
    int i;

    if ( g_client_que == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return NULL;
    }

    for (i = 0; i < NET_MAX_CLIENT; i++)
    {
        if (g_client_que[i].isalive == 1 && g_client_que[i].clntsock != -1 && g_client_que[i].clntsock == sock)
        {
            return &(g_client_que[i]);
        }
    }

    return NULL;
}


static int send_fop_packet(void * ptrsock, void * ptrhead, void * ptrbody)
{
    net_sock_t * ptrclnt = (net_sock_t *) ptrsock;

    if ( ptrhead == NULL || ptrbody == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_FAIL;
    }

    if (ptrsock == NULL)
    {
        // broadcast send
        int i;

        if (((fms_packet_header_t*)ptrhead)->shopcode == EFT_EVT_BASEDATUPDATE_RES)
        {
            // 데이터베이스 명령응답시 타임아웃설정
            int ectlcmd = ((fclt_evt_dbdataupdate_t*)ptrbody)->ectlcmd;
            if (ectlcmd == EFCLTCTLCMD_WORKREADYEND)
            {
                print_dbg(DBG_INFO, 1, "EFT_EVT_BASEDATUPDATE_REQ CMD(%d) TIMEOUT SET ", ectlcmd);
                net_packet_rule_init(&(g_cmd_packet_que));
                g_cmd_packet_que.sendid = ((fms_packet_header_t*)ptrhead)->shopcode;
                g_cmd_packet_que.state = 1;
                net_packet_cmd_rule_make(&(g_cmd_packet_que));
            }
            else if (ectlcmd == EFCLTCTLCMD_WORKCOMPLETE)
            {
                print_dbg(DBG_INFO, 1, "EFT_EVT_BASEDATUPDATE_REQ CMD(%d)", ectlcmd);
                set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
                net_packet_rule_init(&(g_cmd_packet_que));
            }
        }

        for (i = 0; i < NET_MAX_CLIENT; i++)
        {
            if (g_client_que != NULL && g_client_que[i].isalive == 1 && g_client_que[i].clntsock != -1)
            {
                ptrclnt = &(g_client_que[i]);
                print_dbg( DBG_INFO, 1, "packet send socket(%d)[%s:%d] start..... [%s:%d:%s] ", ptrclnt->clntsock, ptrclnt->clntip, ptrclnt->clntport, __FILE__, __LINE__, __FUNCTION__ );
                if (ptrbody != NULL) {
                    send(ptrclnt->clntsock, ptrhead, sizeof(fms_packet_header_t), 0);
                    send(ptrclnt->clntsock, ptrbody, ((fms_packet_header_t *)ptrhead)->nsize, 0);
                } else {
                    send(ptrclnt->clntsock, ptrhead, sizeof(fms_packet_header_t), 0);
                }
                print_dbg( DBG_NONE, 1, "packet send end.....");
            }
        }
    }
    else
    {
        // clinet send
        if (ptrclnt->isalive == 1)
        {
            print_dbg( DBG_INFO, 1, "packet send socket(%d)[%s:%d] start..... [%s:%d:%s] ", ptrclnt->clntsock, ptrclnt->clntip, ptrclnt->clntport, __FILE__, __LINE__, __FUNCTION__ );
            if (((fms_packet_header_t*)ptrhead)->shopcode == EFT_EVT_BASEDATUPDATE_RES)
            {
                // 데이터베이스 명령응답시 타임아웃설정
                int ectlcmd = ((fclt_evt_dbdataupdate_t*)ptrbody)->ectlcmd;
                if (ectlcmd == EFCLTCTLCMD_WORKREADYEND)
                {
                    print_dbg(DBG_INFO, 1, "EFT_EVT_BASEDATUPDATE_REQ CMD(%d) TIMEOUT SET ", ectlcmd);
                    net_packet_rule_init(&(g_cmd_packet_que));
                    g_cmd_packet_que.sendid = ((fms_packet_header_t*)ptrhead)->shopcode;
                    g_cmd_packet_que.state = 1;
                    net_packet_cmd_rule_make(&(g_cmd_packet_que));
                }
                else if (ectlcmd == EFCLTCTLCMD_WORKCOMPLETE)
                {
                    print_dbg(DBG_INFO, 1, "EFT_EVT_BASEDATUPDATE_REQ CMD(%d)", ectlcmd);
                    set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
                    net_packet_rule_init(&(g_cmd_packet_que));
                }
            }
            if (ptrbody != NULL) {
                send(ptrclnt->clntsock, ptrhead, sizeof(fms_packet_header_t), 0);
                send(ptrclnt->clntsock, ptrbody, ((fms_packet_header_t *)ptrhead)->nsize, 0);
            } else {
                send(ptrclnt->clntsock, ptrhead, sizeof(fms_packet_header_t), 0);
            }
            print_dbg( DBG_NONE, 1, "packet send end.....");
        }
    }
    
    return ERR_SUCCESS;
}

static int recv_packet(net_sock_t * ptrclnt, char * data, int len)
{
    recv_data_buffer_t * ptrbuf = NULL;
    fms_packet_header_t headerdata;
    int headersize = sizeof(headerdata);
    int datasize = 0;

    if ((ptrbuf = &(ptrclnt->recvbuf)) == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_FAIL;
    }

    if ((ptrbuf->posw + len) > NET_RECV_BUFFER_MAX)
    {
        print_dbg( DBG_ERR, 1, "recv data buffer overflow [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        ptrbuf->posw = 0;
        ptrbuf->posr = 0;
        return ERR_OVERFLOW;
    }

    memcpy(ptrbuf->buf + ptrbuf->posw, data, len);
    ptrbuf->posw += len;

    // 데이터버퍼 사이즈가 패킷해더 사이즈보다 커야한다.
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
        parser_packet(ptrclnt, &headerdata, ptrbuf->buf + (ptrbuf->posr + headersize));

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


static int parser_packet(net_sock_t * ptrclnt, fms_packet_header_t* ptrhead, void * ptrbody)
{
    net_packet_t* ptrque = NULL;

    if ( ptrclnt == NULL || ptrhead == NULL || ptrbody == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

        return ERR_RETURN;
    }

    // 프로토콜 확인
    if ((ptrhead->shprotocol & 0xffff) != OP_FMS_PROT)
    {
        print_dbg(DBG_INFO, 1, "###########  shprotocol(%04X) error ....!!!", ptrhead->shprotocol);
        return ERR_RETURN;
    }
#if 0
    // OP 명령처리 상태확인 RTU 업데이트 , EQP 전원제어 (AUTO 전원제어 ndata = 9), 데이터베이스 업데이트 제어 [10/19/2016 redIce]
    if (get_op_cmdctl_state() == EUT_RTU_STATE_CONTROL)
    {
        if (is_op_cmdctl_req(ptrhead->shopcode) == 1 && ptrhead->shopcode == EFT_EQPCTL_POWER)
        {
            fms_packet_header_t      head;
            fclt_ctrl_eqponoffrslt_t body;
            print_dbg(DBG_INFO, 1, "########### op client command skip sock (%d) [%s:%d]",
                ptrclnt->clntsock, ptrclnt->clntip, ptrclnt->clntport);

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));

            print_dbg(DBG_INFO, 1, "@@@@@@@@@@@@@@ [PACKET RECV] EFT_EQPCTL_POWER [ SKIP !!! ] ( cmdctl state %d )", get_op_cmdctl_state());

            memcpy(&body.ctl_base, &(((fclt_ctrl_eqponoff_t *)(ptrbody))->ctl_base), sizeof(body.ctl_base));
            body.ctl_base.tfcltelem.nsiteid = get_my_siteid_shm();
            body.ctl_base.tfcltelem.nfcltid = get_my_rtu_fcltid_shm();
            //body.ctl_base.tfcltelem.efclt;
            body.emaxstep = 0;
            body.ecurstep = 0;
            body.ecurstatus = EFCLTCTLST_OVERLAPCMDERROR;

            make_fop_packet_head(&head, EFT_EQPCTL_RES, sizeof(body));
            send_fop_packet(ptrclnt, &head, &body);

            return ERR_RETURN;
        }

        if (is_op_cmdctl_req(ptrhead->shopcode) == 1 && ptrhead->shopcode == EFT_EVT_BASEDATUPDATE_REQ)
        {
            fms_packet_header_t     head;
            fclt_evt_dbdataupdate_t body;

            memset(&head, 0, sizeof(head));
            memset(&body, 0, sizeof(body));

            print_dbg(DBG_INFO, 1, "@@@@@@@@@@@@@@ [PACKET RECV] EFT_EVT_BASEDATUPDATE_REQ [ SKIP !!! ] ( cmdctl state %d )", get_op_cmdctl_state());

            memcpy(&body, ptrbody, ptrhead->nsize);

            body.tfcltelem.nsiteid = get_my_siteid_shm();
            body.tfcltelem.nfcltid = get_my_rtu_fcltid_shm();
            body.tfcltelem.efclt = EFCLT_RTU;
            body.ectlcmd = EFCLTCTLCMD_WORKREADYFAIL;

            make_fop_packet_head()

            ptrcomm->make_header(&head, EFT_EVT_BASEDATUPDATE_RES, sizeof(body));
            send_fop_packet(ptrclnt, &head, &body);

            return ERR_RETURN;
        }
    } // if (get_op_cmdctl_state() == EUT_RTU_STATE_CONTROL)
#endif
    // OP 명령처리 상태확인 RTU 업데이트 , EQP 전원제어 (AUTO 전원제어 ndata = 9), 데이터베이스 업데이트 제어 [10/21/2016 redIce]
    if (ptrhead->shopcode == EFT_EVT_FIRMWAREUPDATE_REQ)
    {
        fms_packet_header_t     head;
        fclt_firmwareupdate_t   body;
        int clntsock = -1;

        memset(&head, 0, sizeof(head));
        memset(&body, 0, sizeof(body));

        print_dbg(DBG_INFO, 1, "@@@@@@@@@@@@@@ [PACKET RECV] EFT_EVT_FIRMWAREUPDATE_REQ ");

        memcpy(&body, ptrbody, sizeof(body));

        make_fop_packet_head(&head, EFT_EVT_FIRMWAREUPDATE_REQ, sizeof(body));
        body.tfcltelem.nsiteid = get_my_siteid_shm();
        body.tfcltelem.nfcltid = get_my_rtu_fcltid_shm();
        body.tfcltelem.efclt = EFCLT_RTU;
        body.timet = time(NULL);
        body.update_site_id = get_my_siteid_shm();
        body.estatus = get_op_cmdctl_state();

        if (get_op_cmdctl_state() != EUT_RTU_STATE_NORMAL || body.update_site_id != get_my_siteid_shm())
        {
            print_dbg(DBG_SAVE, 1, "@@@@@@@@ op cmdctl state not normal and siteid fail client close fd %d [ NET(%d) RTU(%d) ] [%s:%s]", clntsock, body.update_site_id, get_my_siteid_shm(), __FILE__,__FUNCTION__);
            send_fop_packet(ptrclnt, &head, &body);
            clntsock = ptrclnt->clntsock;
            net_connect_close(clntsock);
        }
        else
        {
            set_op_cmdctl_state(EUT_RTU_STATE_UPDATE, 0);
            send_fop_packet(ptrclnt, &head, &body);
            // updatge client start
            update_connect_req(ptrclnt->clntip, (unsigned short)(ptrhead->shdirection));
            clntsock = ptrclnt->clntsock;
            init_client_que(ptrclnt);
            print_line();
            print_dbg(DBG_ERR, 1, "@@@@@@@@ client close fd %d [%s:%s]", clntsock, __FILE__,__FUNCTION__);
            do_del_fd(g_epollfd, clntsock);
            net_connect_close(clntsock); 
        }
        return ERR_RETURN;
    }

    ptrque = &(g_cmd_packet_que);

    // recv packet delete
    if (ptrhead->shopcode == EFT_EVT_BASEDATUPDATE_REQ)
    {
        int ectlcmd = ((fclt_evt_dbdataupdate_t*)ptrbody)->ectlcmd;
        if (ectlcmd == EFCLTCTLCMD_WORKRUN)
        {
            print_dbg(DBG_INFO, 1, "EFT_EVT_BASEDATUPDATE_REQ CMD(%d) TIMEOUT SET", ectlcmd);
            set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
            net_packet_rule_init(ptrque);
            g_cmd_packet_que.sendid = EFT_EVT_BASEDATUPDATE_RES;
            g_cmd_packet_que.state = 1;
            net_packet_cmd_rule_make(ptrque);
        }
    }

    init_netcomm(ptrclnt);

    if (net_packet_parser(ptrhead, ptrbody, &g_netcomm) != ERR_SUCCESS)
    {
        print_dbg(DBG_ERR, 1, "parser packet protocol error .... %d [%s:%s] ", ptrhead->shopcode, __FILE__,__FUNCTION__);
        return ERR_RETURN;
    }

    return ERR_SUCCESS;

}

//////////////////////////////////////////////////////////////////////////

/* control로 부터 sensor data를 받아 처리하는 함수 */
static int rtusvr_sns_data_res( unsigned short inter_msg, void * ptrdata )
{
    inter_msg_t * ptrmsg = NULL;

	if ( get_print_debug_shm() == 1 )
	{
		print_line();
		print_dbg( DBG_INFO,1,"rtusvr_sns_data_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
	}

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

    init_netcomm(get_client(ptrmsg->client_id));

    return ctl_sns_data_res(inter_msg, ptrdata, &g_netcomm);
}

/* RTU센서 모니터링 데이터 실시간 수집 */
static int rtusvr_sns_cudata_res( unsigned short inter_msg, void * ptrdata )
{
    inter_msg_t * ptrmsg = NULL;

	if ( get_print_debug_shm() == 1 )
	{
		print_line();
		print_dbg( DBG_INFO,1,"rtusvr_sns_cudata_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
	}

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

    init_netcomm(get_client(ptrmsg->client_id));

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

    init_netcomm(get_client(ptrmsg->client_id));

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

    init_netcomm(get_client(ptrmsg->client_id));

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

    init_netcomm(get_client(ptrmsg->client_id));

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

    init_netcomm(get_client(ptrmsg->client_id));

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

    init_netcomm(get_client(ptrmsg->client_id));

    return ctl_sns_completed_thrld(inter_msg, ptrdata, &g_netcomm);
}

/* 감시장비 모니터링 데이터 */
static int rtusvr_eqp_data_res( unsigned short inter_msg, void * ptrdata )
{
    inter_msg_t * ptrmsg = NULL;

	if ( get_print_debug_shm() == 1 )
	{
		print_line();
		print_dbg( DBG_INFO,1,"rtusvr_eqp_data_res [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
	}

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

    init_netcomm(get_client(ptrmsg->client_id));

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

    init_netcomm(get_client(ptrmsg->client_id));

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

    init_netcomm(get_client(ptrmsg->client_id));

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

    init_netcomm(get_client(ptrmsg->client_id));

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

    init_netcomm(get_client(ptrmsg->client_id));

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

    init_netcomm(get_client(ptrmsg->client_id));

    return ctl_eqp_ctl_thrld(inter_msg, ptrdata, &g_netcomm);
}

//임계기준 수정 완료를 OP에게 알려줌.
static int rtusvr_eqp_completed_thrld(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

	print_line();
	print_dbg( DBG_INFO,1,"ctl_eqp_completed_thrld [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

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

    init_netcomm(get_client(ptrmsg->client_id));

    return ctl_eqp_completed_thrld(inter_msg, ptrdata, &g_netcomm);
}


//시간동기화 : RTU - SVR - OP 간 시간 동기화
static int rtusvr_timedat_req(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"ctl_timedat_req [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

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

    init_netcomm(get_client(ptrmsg->client_id));

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

    init_netcomm(get_client(ptrmsg->client_id));

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
    case ALM_OP_NET_CHG_NOTI_MSG: /* Control -> transfer( OP ) */ 
        {
            // OP서버 네트워크 포트 변경이 일어난경우
            g_start_fop_pthread = 0;
            g_dbchange_check = 1;
            set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
        }
        break;
    case ALM_FORCE_PWR_END_MSG:
        {
            set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 1);
        }
        break;
    }

    return 0;
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

    init_netcomm(get_client(ptrmsg->client_id));

    return ctl_evt_network_err(inter_msg, ptrdata, &g_netcomm);
}

static int rtusvr_eqp_sts_resp(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"EQP STATUS_RESP [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

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

    init_netcomm(get_client(ptrmsg->client_id));

    return ctl_eqp_sts_resp(inter_msg, ptrdata, &g_netcomm);
}

static int rtusvr_eqp_net_sts_resp(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;
	fclt_cnnstatus_eqprslt_t * ptreqprslt = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"EQP NETWORK STATUS_RESP [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

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

	ptreqprslt = (fclt_cnnstatus_eqprslt_t * )ptrmsg->ptrbudy;

	ptreqprslt->nfopcnncount = get_accept_client_list() ;

    init_netcomm(get_client(ptrmsg->client_id));

    return ctl_eqp_net_sts_resp(inter_msg, ptrdata, &g_netcomm);
}

static int rtusvr_eqp_off_opt_resp(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"EQP OFF OPTION  RESP [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

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

    init_netcomm(get_client(ptrmsg->client_id));

    return ctl_eqp_off_opt_resp(inter_msg, ptrdata, &g_netcomm);

}

static int rtusvr_eqp_on_opt_resp(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;

    print_line();
    print_dbg( DBG_INFO,1,"EQP ON OPTION  RESP [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

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

    init_netcomm(get_client(ptrmsg->client_id));

    return ctl_eqp_on_opt_resp(inter_msg, ptrdata, &g_netcomm);
}

static int make_fop_packet_head(void * ptrhead, int cmd, int nsize)
{
    fms_packet_header_t * ptr = ( fms_packet_header_t * ) ptrhead;
    ptr->shprotocol = OP_FMS_PROT;   // (2 BYTE) op_fms_prot
    ptr->shopcode = cmd;     // (2 BYTE) ecmd_rms_eqptatus_req,, 
    ptr->nsize = nsize;        // (4 BYTE) 패킷헤더를 뺀 나머지 body 크기
    ptr->shdirection = ESYSD_RTUSVR;  // (2 BYTE) enumfms_sysdir

    return ERR_SUCCESS;
}

/*================================================================================================
외부  함수  정의 
================================================================================================*/
int init_op_server( send_data_func ptrsend_data )
{
	/* CONTROL module의 함수 포인트 등록 */
	g_ptrsend_data_control_func = ptrsend_data ;	/* CONTROL module로 설정, 제어, 알람 , 장애  데이터를 보낼 수 있는 함수 포인터 */

	/* 기타 초기화 작업 */
	internal_init();

	return ERR_SUCCESS;
}

/* op_server module loop idle 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
int idle_op_server( void * ptr )
{
	return internal_idle( ptr );
}

/* 동적 메모리, fd 등 resouce release함수 */
int release_op_server( void )
{
	/* release 작업  */
	return internal_release(); 
}

/* CONTROL Module로 부터 수집 데이터, 장애, 알람를 받는 외부 함수 */
int recv_sns_data_op_server( unsigned short inter_msg, void * ptrdata )
{
	return internal_recv_sns_data( inter_msg, ptrdata );
}
