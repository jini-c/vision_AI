#include "com_include.h"
#include "net.h"
#include "ctrl_db.h"

#include <libwebsockets.h>

#define KGRN "\033[0;32;32m"
#define KCYN "\033[0;36m"
#define KRED "\033[0;32;31m"
#define KYEL "\033[1;33m"
#define KMAG "\033[0;35m"
#define KBLU "\033[0;32;34m"
#define KCYN_L "\033[1;36m"
#define RESET "\033[0m"

#define MAX_C_SOCKET_CNT		10
#define MAX_WEB_RECV_BUF_SIZE   500
#define MAX_WEB_SEND_BUF_SIZE   1024 
#define MAX_WEB_HEAD_SIZE 		sizeof( fms_packet_header_t )
#define MAX_SEND_SIZE			( MAX_WEB_SEND_BUF_SIZE - MAX_WEB_HEAD_SIZE )

#define MAX_WEB_VAL_CNT 		5


typedef struct c_sock_
{
	struct libwebsocket * ptrsock;
}c_sock_t;

typedef struct c_sock_mng_
{
	int cnt;
	c_sock_t list_sock[ MAX_C_SOCKET_CNT ];
}c_sock_mng_t;

struct per_session_data {
	int fd;
};

static volatile int g_websock_init	= 0;
static volatile int g_websock_done = 0;
static struct libwebsocket_context *g_context = NULL;

static c_sock_mng_t g_sock_mng;
static send_data_func g_ptrsend_websock_control_func = NULL;
static int send_websock_control( UINT16 inter_msg, void * ptrdata  );

static int send_webclient( BYTE * ptrdata, int size );
//static int websocket_write_bin(struct libwebsocket *wsi_in, char *str, int str_size_in);
//static int websocket_write_back(struct libwebsocket *wsi_in, char *str, int str_size_in);
static int websocket_write_bin(struct libwebsocket *wsi_in, BYTE *str, int str_size_in) ;

/* control -> websock */
static int recv_ctrl_sns_data_res( UINT16 inter_msg, void * ptrdata );
static int recv_ctrl_sns_data_evt_fault( UINT16 inter_msg, void * ptrdata );
static int recv_ctrl_sns_data_ctl_res( UINT16 inter_msg, void * ptrdata );
static int recv_ctrl_web_noti( UINT16 inter_msg, void * ptrdata );

/* web -> websock -> control */
static int parse_web_packet( struct libwebsocket *wsi_in, BYTE * ptrdata, int size );
/*=============================================================*/

static proc_func_t g_proc_func_list []= 
{
	/* inter msg id   		,  			proc function point */
	/*=============================================*/
    { EFT_SNSRCUDAT_RES,		recv_ctrl_sns_data_res},		//RTU센서 모니터링 데이터 수집
    //{ EFT_SNSRDAT_RES, 			recv_ctrl_sns_data_res},	//RTU센서 모니터링 데이터 수집
    { EFT_SNSREVT_FAULT,		recv_ctrl_sns_data_evt_fault}, 	//장애(FAULT)이벤트
    { EFT_SNSRCTL_RES,			recv_ctrl_sns_data_ctl_res},   	//전원제어에 대한 결과
    { EFT_ALLIM_WEB_NOTI,		recv_ctrl_web_noti},   			//WEB_NOTI

	{ 0, NULL }
};

#if 0
static void send_test_data( void )
{
	static BYTE send_buf[ MAX_WEB_SEND_BUF_SIZE ];
	BYTE * ptr = NULL;
	static int cnt = 0;
	int i;
	int size = 0;

	memset( send_buf, 0, sizeof( send_buf ));
	ptr = (BYTE*)&send_buf[ 0 ];
	size = ( MAX_WEB_SEND_BUF_SIZE - LWS_SEND_BUFFER_PRE_PADDING  - LWS_SEND_BUFFER_POST_PADDING );

	for( i = 0 ; i < size ; i++ )
	{
		*ptr++ = 0xFF;
	}

	send_buf[ size - 1 ] = cnt ;
	send_webclient( send_buf, size );

	if ( cnt++ > 200 ) cnt = 0;
}
#endif

static int send_add_web_client_to_ctrl( void )
{
	static inter_msg_t send_msg;
	static allim_msg_t allim_msg;

	memset( &send_msg, 0, sizeof( send_msg ));
	memset( &allim_msg, 0, sizeof( allim_msg ));

	allim_msg.allim_msg = WEB_RTU_ADD_CLIENT;

	/* -100 : WEB Client */
	send_msg.client_id 	= 0;
	send_msg.msg 		= EFT_ALLIM_WEB_REQ;
	send_msg.ptrbudy 	= &allim_msg ;
	send_websock_control( EFT_ALLIM_WEB_REQ, &send_msg );

	return ERR_SUCCESS;
}

static int send_sys_info( void )
{
	static BYTE send_buf[ MAX_WEB_SEND_BUF_SIZE ];
	int size = 0;

	int send_size =0;
	fms_packet_header_t head;
	BYTE * ptrsend = NULL;
	web_req_t web_noti;
	sys_info_t sys_info;

	size = sizeof( web_noti );
	memset( &head, 0, sizeof( head ));
	memset(&web_noti, 0, size );
	memset(&sys_info, 0, sizeof( sys_info ));
	memset( send_buf,0, MAX_WEB_SEND_BUF_SIZE );
	ptrsend = (BYTE *)&send_buf[ 0 ];

	if( get_shm_sys_info( &sys_info ) != ERR_SUCCESS )
	{
        print_dbg(DBG_ERR, 1, "err of sys info [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
	}
	
	web_noti.site_id = get_my_siteid_shm();
	web_noti.web_msg = 1;

	web_noti.n_val1 = sys_info.use_cpu * 100;
	web_noti.n_val2 = sys_info.use_mem * 100;
	web_noti.n_val3 = sys_info.use_hard * 100;
	web_noti.n_val4 = get_fms_server_net_sts() ;
	
	//printf("wwwwwwwwwwww cpu :%d, mem:%d, hard:%d.................\r\n",web_noti.n_val1,web_noti.n_val2,web_noti.n_val3);

	head.shprotocol 	= OP_FMS_PROT;
	head.shopcode		= EFT_ALLIM_WEB_NOTI;
	head.nsize			= size ;
	head.shdirection 	= ESYSD_RTUFOP;

	/* WEB SOCKET PADDING 위치에서부터 데이터를 복사한다 */
	//ptrsend += LWS_SEND_BUFFER_PRE_PADDING;
	memcpy( ptrsend, &head, MAX_WEB_HEAD_SIZE );
	ptrsend += MAX_WEB_HEAD_SIZE;
	memcpy( ptrsend, &web_noti, size  );
	send_size = size + MAX_WEB_HEAD_SIZE;

	send_webclient( send_buf, send_size );
	
	return ERR_SUCCESS;
}

/*================================================================================================
내부 함수  정의 
매개변수가 pointer일경우 반드시 null 체크 적용 
================================================================================================*/
/* CONTROL module에  보낼 데이터(제어, 설정, 알람)가  있을 경우 호출 */
static int send_websock_control( UINT16 inter_msg, void * ptrdata  )
{
	if ( g_ptrsend_websock_control_func != NULL )
	{
		return g_ptrsend_websock_control_func( inter_msg, ptrdata );
	}

	return ERR_RETURN;
}

static int get_snsrtype(void * ptrdata, int pos)
{
    int esnsrtype = 0;

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrdata [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }

	//print_buf( 100, ptrdata );
	/* inter_msg_t : clientid :4byte + msg : 2byte */
    memcpy(&esnsrtype, ptrdata + pos , sizeof(int ));
    return esnsrtype;
}

static int parse_head( fms_packet_header_t * ptrhead, BYTE * ptrrecv, int size )
{
	int recv_size =0;

	if( ptrhead == NULL || ptrrecv == NULL || size == 0 )
	{
		print_dbg(DBG_ERR, 1, "is valid data [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
		return ERR_RETURN;
	}
	
	memcpy( ptrhead, ptrrecv, sizeof( fms_packet_header_t ));

	/* PROTOCOL이 맞지 않을 경우 */	
	if ( (ptrhead->shprotocol ) != OP_FMS_PROT )
	{
		print_dbg(DBG_ERR, 1, "Is not FMS PROT:%02x [%s:%d:%s]", ptrhead->shprotocol , __FILE__, __LINE__, __FUNCTION__);
		return ERR_RETURN;
	}

	recv_size = (size - sizeof( fms_packet_header_t ) );
	
	/* 보낸 사이즈와 payload 사이즈가 다들 경우 */
	if ( ptrhead->nsize !=  recv_size )
	{
		print_dbg(DBG_ERR, 1, "Is not Same Size :recv_size:%d, payload_size:%d [%s:%d:%s]", recv_size, ptrhead->nsize, __FILE__, __LINE__, __FUNCTION__);
		return ERR_RETURN;
	}
	
	/* EFT_SNSRCTL_POWER */
	/* EFT_SNSRCTL_THRLD */
	/* EFT_EVT_BASEDATUPDATE_REQ */
	/* EFT_EVT_FIRMWAREUPDATE_REQ */
	/* EFT_ALLIM_WEB_REQ */
	
	return ERR_SUCCESS;
}

static int copy_firmware( char * ptrfile )
{
	char szpath[ MAX_PATH_SIZE ];
	FILE * fp = NULL;

	if ( ptrfile == NULL || strlen( ptrfile )  <= 0 )
	{
		print_dbg( DBG_ERR,1, "is null firmware file name[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
		return ERR_RETURN;
	}
	
	memset( szpath, 0, sizeof( szpath ));
	sprintf(szpath , "%s%s" ,APP_TEMP_PATH ,ptrfile );

	/* 파일 존재 확인 */
	fp = fopen( szpath , "ab+");
	if ( fp == NULL )
	{
		print_dbg( DBG_ERR,1, "Cannot open file :%s[%s:%d:%s]", szpath, __FILE__, __LINE__, __FUNCTION__ );
		return ERR_RETURN;
	}
	
	fclose( fp );

	/* 파일 복사 */
	memset( szpath, 0, sizeof( szpath ));
	sprintf(szpath , "cp -f %s%s %s%s && chmod +x %s%s"
			, APP_TEMP_PATH ,ptrfile 
			, APP_PATH_1,ptrfile 
			, APP_PATH_1, ptrfile );

	system_cmd(szpath );
	return ERR_SUCCESS;
}

static int set_web_req( fms_packet_header_t * ptrhead, BYTE * ptrrecv )
{
	int msg =0;
	web_req_t web_req;
	net_info_t net;
	char szcmd[ MAX_PATH_SIZE ];

	memset( &web_req , 0, sizeof( web_req ));
	memcpy(&web_req, &ptrrecv[ MAX_WEB_HEAD_SIZE ], ptrhead->nsize );

	print_buf( ptrhead->nsize, &ptrrecv[ MAX_WEB_HEAD_SIZE ] );

	msg = web_req.web_msg;

	print_dbg( DBG_SAVE, 1, "Recv Web SITE_ID:%d, Msg:%d", web_req.site_id ,  web_req.web_msg );

	/* RTU Reset */
	if ( msg == WEB_RTU_RESET_MSG )
	{
		print_dbg( DBG_SAVE, 1, "Recv RTU Reset from WEB");
		sys_sleep( 500 );
		system_cmd("reboot");
	}
	/* RTU IP Change */
	else if ( msg == WEB_RTU_IP_CHG_MSG )
	{
		print_dbg( DBG_SAVE, 1, "Recv RTU CHG IP from WEB [IP:%s, SUB:%s, GATE:%s]", 
				                 web_req.ch_val1,
								 web_req.ch_val2,
								 web_req.ch_val3 );
		memset( &net, 0, sizeof( net));

		/* ch_val1 : ip, ch_val2 : subnet, ch_val3 : gateway */
		if ( strlen( web_req.ch_val1 ) > 0 && strlen( web_req.ch_val2 ) > 0 && strlen( web_req.ch_val3 ) > 0  )
		{
			memcpy( net.ip 		, web_req.ch_val1, strlen( web_req.ch_val1));
			memcpy( net.gateway , web_req.ch_val3, strlen( web_req.ch_val3));
			memcpy( net.netmask , web_req.ch_val2, strlen( web_req.ch_val2));

			if( save_rtu_net_shm( &net  ) == ERR_SUCCESS )
			{
				sys_sleep( 1000 );
				system_cmd("reboot");
			}
			else
			{
				print_dbg(DBG_ERR, 1, "Can not Change RTU IP ADDR[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
				return ERR_RETURN;
			}
		}
		else
		{
			print_dbg(DBG_ERR, 1, "Is null IP , SUB, GATE DATA[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__ );
			return ERR_RETURN;
		}
	}
	/* RTU SITE ID Change */
	else if ( msg == WEB_RTU_SITE_CHG_MSG )
	{
		print_dbg( DBG_SAVE, 1, "Recv RTU CHG SITE ID from WEB [SITE ID :%d]", web_req.n_val1 );

		if ( update_rtu_site_id_ctrl_db( web_req.n_val1 ) == ERR_SUCCESS )
		{

			if ( set_my_siteid_shm( web_req.n_val1 ) == ERR_SUCCESS )
			{
				sys_sleep( 1000 );
				//system_cmd("reboot");
				killall( APP_PNAME, 9);

			}
		}
	}
	else if ( msg == WEB_RTU_FIRMWARE_CHG_MSG )
	{
		if ( strlen( web_req.ch_val1 ) > 0 )
		{
			print_dbg( DBG_SAVE, 1, "Recv RTU CHG Firmware from WEB [FILENAME:%s]", web_req.ch_val1);

			if( copy_firmware( web_req.ch_val1 ) != ERR_SUCCESS )
			{
				print_dbg( DBG_ERR, 1, "Cannot Update Firmware[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__ );
			}
			else
			{
				print_dbg( DBG_SAVE, 1, "Update Firmware[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__ );
				sys_sleep( 2000 );
				//system_cmd("reboot");
				killall( APP_PNAME, 9);

			}
		}
		else
		{
			print_dbg(DBG_ERR, 1, "Is null IP , FRIMWARE FILE[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__ );
			return ERR_RETURN;
		}

	}
	else if ( msg == WEB_RTU_NTP_CHG_MSG )
	{
		if ( strlen( web_req.ch_val1 ) > 0 )
		{
			print_dbg( DBG_SAVE, 1, "Recv RTU CHG NTP ADDR from WEB [FILENAME:%s]", web_req.ch_val1);

			if( set_ntp_addr_shm( web_req.ch_val1 ) != ERR_SUCCESS )
			{
				print_dbg( DBG_ERR, 1, "Cannot Update NTP ADDR [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__ );
			}
			else
			{
					memset( szcmd, 0, sizeof( szcmd ));
					sprintf( szcmd, "ntpdate -u %s", web_req.ch_val1);
					system_cmd( szcmd );
					print_dbg( DBG_LINE, 1, NULL );
					print_dbg( DBG_SAVE, 1, "Set of NTP Time :%s", szcmd );
					sys_sleep( 1000 );
					//system_cmd("reboot");
					killall( APP_PNAME, 9);
			}
		}
		else
		{
			print_dbg(DBG_ERR, 1, "Is null NTP FILE[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__ );
			return ERR_RETURN;
		}

	}
	else if ( msg == WEB_RTU_MODE_CHG_MSG )
	{
		print_dbg( DBG_SAVE, 1, "Recv RTU RUN MODE ID from WEB [RUN MODE :%d]", web_req.n_val1 );
		if ( set_rtu_run_mode_shm( web_req.n_val1 ) == ERR_SUCCESS )
		{
			sys_sleep( 1000 );
			//system_cmd("reboot");
			killall( APP_PNAME, 9);
		}
	}
	else
	{
		print_dbg( DBG_SAVE, 1, "Invalid Web Msg :%d[%s:%d:%s]",msg, __FILE__, __LINE__, __FUNCTION__ );
	}

	return ERR_SUCCESS;
}

static int copy_and_send_ctrl( fms_packet_header_t * ptrhead, BYTE * ptrrecv, int size )
{
	inter_msg_t send_msg;
	BYTE * ptrbudy = NULL;
	snsr_info_t snsrinfo;
    fclt_ctrl_snsronoff_t   snsr_onoff;

	memset(&snsrinfo, 0, sizeof(snsrinfo));
	memset( &send_msg, 0, sizeof( send_msg ));
	memset( &snsr_onoff, 0, sizeof( snsr_onoff ));

	ptrbudy = &ptrrecv[ MAX_WEB_HEAD_SIZE ]; 
	
	print_buf( ptrhead->nsize, ptrbudy );

#if 0
	int snsr_id = 0;
	/* ONOFF 제어에 대해서는 로그를 남긴다 */
	if ( ptrhead->shopcode == EFT_SNSRCTL_POWER  )
	{
		memcpy( &snsr_onoff, ptrbudy , sizeof( snsr_onoff ));
        if (get_db_col_info_shm(
            snsr_onoff.ctl_base.esnsrtype, snsr_onoff.ctl_base.esnsrsub, snsr_onoff.ctl_base.idxsub,
            &snsrinfo) != ERR_RETURN )
        {
            snsr_id = snsrinfo.snsr_id;
            insert_db_sns_ctl_hist_onoff(&snsr_onoff, snsr_id);
        }
	}
#endif

	/* -100 : WEB Client */
	send_msg.client_id 	= WEB_CLIENTID;
	send_msg.msg 		= ptrhead->shopcode;
	send_msg.ptrbudy 	= ptrbudy ;
	send_websock_control( ptrhead->shopcode, &send_msg );

	return ERR_SUCCESS;
}

static int parse_web_packet( struct libwebsocket *wsi_in, BYTE * ptrdata, int size )
{
	static BYTE recv_buf[ MAX_WEB_RECV_BUF_SIZE ];
	static volatile int parsed = 0;
	fms_packet_header_t head;

	if ( parsed == 1 )
	{
		print_dbg(DBG_SAVE, 1, "Duplicate control.....[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
		return ERR_RETURN;
	}

	parsed = 1;
	memset( recv_buf, 0, sizeof( recv_buf ));
	memset( &head, 0, sizeof( head ));

	if ( ptrdata == NULL || size <=0 )
	{
		print_dbg(DBG_ERR, 1, "is valid data size:%d, [%s:%d:%s]",size, __FILE__, __LINE__, __FUNCTION__);
		parsed = 0;
		return ERR_RETURN;
	}
	
	if ( size >= MAX_WEB_RECV_BUF_SIZE )
	{
        print_dbg(DBG_ERR, 1, "Over of Recv buff size :%d [%s:%d:%s]",size, __FILE__, __LINE__, __FUNCTION__);
		parsed = 0;
		return ERR_RETURN;
	}
	
	memcpy( recv_buf, ptrdata, size );

	if ( parse_head( &head, recv_buf, size ) != ERR_SUCCESS )
	{
        print_dbg(DBG_ERR, 1, "is valid head[%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
		parsed = 0;
		return ERR_RETURN;
	}
	
	print_dbg( DBG_INFO, 1, "Recv Web: Protocol:%02x, Msg:%d, Size:%d",head.shprotocol, head.shopcode, head.nsize );

	/* WEB REQ일 경우 CONTROL에 넘기지 않는다 */
	if ( head.shopcode == EFT_ALLIM_WEB_REQ )
	{
		set_web_req( &head, recv_buf );
	}
	else
	{
		copy_and_send_ctrl( &head, recv_buf, size );
	}
	
	parsed = 0;

	return ERR_SUCCESS;
}

static int make_head( UINT16 inter_msg, void * ptrdata, BYTE * ptrbuf, int size, int *ptrsend_size )
{
    inter_msg_t * ptrmsg = NULL;
	int send_size =0;
	fms_packet_header_t head;
	BYTE * ptrsend = NULL;

	if ( ptrbuf == NULL || ptrdata == NULL || size == 0 )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	if ( size >=  MAX_SEND_SIZE )
	{
		print_dbg( DBG_ERR, 1, "Over of Web Send buff size:%d[%s:%d:%s]", size, __FILE__, __LINE__,__FUNCTION__ );
		return ERR_RETURN;
	}
	
	memset( &head, 0, sizeof( head ));
	memset( ptrbuf,0, MAX_WEB_SEND_BUF_SIZE );
	ptrsend = ptrbuf;

	ptrmsg = ( inter_msg_t *)ptrdata;
	
	if ( ptrmsg->ptrbudy == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrbudy is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	head.shprotocol 	= OP_FMS_PROT;
	head.shopcode		= inter_msg;
	head.nsize			= size;
	head.shdirection 	= ESYSD_RTUFOP;

	/* WEB SOCKET PADDING 위치에서부터 데이터를 복사한다 */
	//ptrsend += LWS_SEND_BUFFER_PRE_PADDING;
	memcpy( ptrsend, &head, MAX_WEB_HEAD_SIZE );
	ptrsend += MAX_WEB_HEAD_SIZE;
	memcpy( ptrsend, ptrmsg->ptrbudy, size  );
	send_size = size + MAX_WEB_HEAD_SIZE;
	*ptrsend_size = send_size;

	return ERR_SUCCESS;
}

static int recv_ctrl_sns_data_res( UINT16 inter_msg, void * ptrdata )
{
	int size =0;
	int send_size = 0;
	static BYTE sns_buf[ MAX_WEB_SEND_BUF_SIZE ];
	int snsr_type = -1;
	inter_msg_t * ptrmsg =NULL;
	BYTE * ptrbudy = NULL;

	/*system 정보를 먼저 보낸다 */
	send_sys_info();

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrmsg 		= ( inter_msg_t *)ptrdata;
	ptrbudy 	= ptrmsg->ptrbudy;
	snsr_type 	= get_snsrtype( ptrbudy , sizeof( fms_collectrslt_aidido_t ) );

	//printf("...........snsr_type:%d..........................\r\n", snsr_type );	
	if ( snsr_type == ESNSRTYP_AI )
	{
		size = sizeof(fclt_collectrslt_ai_t);
	}
	else if ( snsr_type == ESNSRTYP_DI || snsr_type == ESNSRTYP_DO )
	{
		size = sizeof(fclt_collectrslt_dido_t);
	}
	else
	{
		print_dbg( DBG_ERR, 1, "invalid snsr_type:%d [%s:%d:%s]", snsr_type, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	if ( make_head( inter_msg, ptrdata, sns_buf, size, &send_size ) == ERR_SUCCESS )
	{
		send_webclient( sns_buf, send_size );
	}

	return ERR_SUCCESS;
}

static int recv_ctrl_sns_data_evt_fault( UINT16 inter_msg, void * ptrdata )
{
	int size =0;
	int send_size = 0;
	static BYTE sns_buf[ MAX_WEB_SEND_BUF_SIZE ];
	int snsr_type = -1;
	inter_msg_t * ptrmsg =NULL;
	BYTE * ptrbudy = NULL;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrmsg 		= ( inter_msg_t *)ptrdata;
	ptrbudy 	= ptrmsg->ptrbudy;
	snsr_type 	= get_snsrtype( ptrbudy , sizeof( fms_evt_faultaidi_t ) );
	
	if ( snsr_type == ESNSRTYP_AI )
	{
		size = sizeof(fclt_evt_faultai_t );
	}
	else if ( snsr_type == ESNSRTYP_DI || snsr_type == ESNSRTYP_DO )
	{
		size = sizeof(fclt_evt_faultdi_t );
	}
	else
	{
		print_dbg( DBG_SAVE, 1, "invalid snsr_type [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	if ( make_head( inter_msg, ptrdata, sns_buf, size, &send_size ) == ERR_SUCCESS )
	{
		send_webclient( sns_buf, send_size );
	}

	return ERR_SUCCESS;
}

static int recv_ctrl_sns_data_ctl_res( UINT16 inter_msg, void * ptrdata )
{
	int size =0;
	int send_size = 0;
	static BYTE sns_buf[ MAX_WEB_SEND_BUF_SIZE ];
	int snsr_type = -1;
	inter_msg_t * ptrmsg =NULL;
	BYTE * ptrbudy = NULL;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrmsg = ( inter_msg_t *)ptrdata;
	ptrbudy = ptrmsg->ptrbudy;
	snsr_type = get_snsrtype( ptrbudy , sizeof( fms_collectrslt_aidido_t ) );
	
	if ( snsr_type == ESNSRTYP_AI )
	{
		size = sizeof(fclt_collectrslt_ai_t);
	}
	else if ( snsr_type == ESNSRTYP_DI || snsr_type == ESNSRTYP_DO )
	{
		size = sizeof(fclt_collectrslt_dido_t);
	}
	else
	{
		print_dbg( DBG_ERR, 1, "invalid snsr_type [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	if ( make_head( inter_msg, ptrdata, sns_buf, size, &send_size ) == ERR_SUCCESS )
	{
		send_webclient( sns_buf, send_size );
	}

	return ERR_SUCCESS;
}

static int recv_ctrl_web_noti( UINT16 inter_msg, void * ptrdata )
{
	int size =0;
	int send_size = 0;
	static BYTE sns_buf[ MAX_WEB_SEND_BUF_SIZE ];
	inter_msg_t * ptrmsg =NULL;
	BYTE * ptrbudy = NULL;
	web_req_t web_noti;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memset( &web_noti, 0, sizeof( web_noti ));

	ptrmsg 	= ( inter_msg_t *)ptrdata;
	ptrbudy = ptrmsg->ptrbudy;
	size   	=  sizeof( web_req_t );

	if ( make_head( inter_msg, ptrdata, sns_buf, size, &send_size ) == ERR_SUCCESS )
	{
		memcpy( &web_noti, ptrbudy , size );
		print_dbg( DBG_INFO, 1, "---------------- Recv Web Noti from Ctrol MsgID:%d", web_noti.web_msg );
		send_webclient( sns_buf, send_size );
	}

	return ERR_SUCCESS;
}

static int put_client_sock( struct libwebsocket * wsi_in )
{
	int i, cnt;
	
	cnt = g_sock_mng.cnt;
	//printf("add %p.......\r\n", wsi_in );
	if ( cnt < MAX_C_SOCKET_CNT ) 
	{
		for( i = 0 ; i < MAX_C_SOCKET_CNT ; i++ )
		{
			if( g_sock_mng.list_sock[ i ].ptrsock == NULL )
			{
				g_sock_mng.list_sock[ i ].ptrsock= wsi_in;
				g_sock_mng.cnt++;
				//printf("Add Socket.......\r\n");
				send_sys_info();
				send_add_web_client_to_ctrl();
				break;
			}
		}
	}
	//printf("Client Socket Cnt:%d...\r\n", g_sock_mng.cnt );	
	return ERR_SUCCESS;
}

static int del_client_sock( struct libwebsocket * wsi_in )
{
	int i;
	//cnt = g_sock_mng.cnt;
	//printf("del %p.......\r\n", wsi_in );
	for( i = 0 ; i < MAX_C_SOCKET_CNT ; i++ )
	{
		if( g_sock_mng.list_sock[ i ].ptrsock == wsi_in )
		{
			g_sock_mng.list_sock[ i ].ptrsock= NULL;
			g_sock_mng.cnt--;
			//printf("Del Socket.......\r\n");
			break;
		}
	}

	//printf("Client Socket Cnt:%d...\r\n", g_sock_mng.cnt );	
	return ERR_SUCCESS;
}

static int send_webclient( BYTE * ptrdata, int size )
{
	int i;
	struct libwebsocket * ptr = NULL;

	for( i = 0 ; i < MAX_C_SOCKET_CNT ; i++ )
	{
		ptr = g_sock_mng.list_sock[ i ].ptrsock;
		if( ptr != NULL && ptrdata != NULL )
		{
			websocket_write_bin( ptr, ptrdata, size);
			//printf("send data:%p, %s...\r\n", ptr, ptrdata);	
		}
	}

	return ERR_SUCCESS;
}

/* *
 * websocket_write_back: write the string data to the destination wsi.
 */
#if 0
static int websocket_write_back(struct libwebsocket *wsi_in, char *str, int str_size_in) 
{
	if (str == NULL || wsi_in == NULL)
		return -1;

	int n;
	int len;
	BYTE *out = NULL;

	if (str_size_in < 1) 
		len = strlen(str);
	else
		len = str_size_in;

	out = (BYTE *)malloc(sizeof(char)*(LWS_SEND_BUFFER_PRE_PADDING + len + LWS_SEND_BUFFER_POST_PADDING));
	//* setup the buffer*/
	memcpy (out + LWS_SEND_BUFFER_PRE_PADDING, str, len );
	//* write out*/
	n = libwebsocket_write(wsi_in, out + LWS_SEND_BUFFER_PRE_PADDING, len, LWS_WRITE_TEXT);

	//printf(KBLU"[websocket_write_back] %s\n"RESET, str);
	//* free the buffer*/
	free(out);

	return n;
}
#endif
static int websocket_write_bin(struct libwebsocket *wsi_in, BYTE *ptrdata, int str_size_in) 
{
	int n = 0;
	int len;
	int size = 0;

	len = str_size_in;
	size = (LWS_SEND_BUFFER_PRE_PADDING + len + LWS_SEND_BUFFER_POST_PADDING);

	if ( ptrdata == NULL || wsi_in == NULL)
	{
		print_dbg( DBG_ERR, 1, "Isvalid Data [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
		return ERR_RETURN;
	}
	if ( size > MAX_WEB_SEND_BUF_SIZE )
	{
		print_dbg( DBG_ERR, 1, "Oversize of Send size :%d [%s:%d:%s]", size, __FILE__, __LINE__, __FUNCTION__);
		return ERR_RETURN;
	}

	if ( wsi_in != NULL )
	{
#ifdef USE_WEBSOCK 
		n = libwebsocket_write( wsi_in, ptrdata, len, LWS_WRITE_BINARY );
#endif
	}

	return n;
}

static int ws_service_callback(struct libwebsocket_context *context,
		struct libwebsocket *wsi,
		enum libwebsocket_callback_reasons reason, void *user,
		void *in, size_t len)
{
	
	switch (reason) {

		case LWS_CALLBACK_ESTABLISHED:
			printf(KYEL"[Main Service] Connection established\n"RESET);
			put_client_sock( wsi );
			break;

			//* If receive a data from client*/
		case LWS_CALLBACK_RECEIVE:
			printf(KCYN_L"[Main Service] Server recvived:%s\n"RESET,(char *)in);

			//* echo back to client*/
			//websocket_write_back(wsi ,(char *)in, -1);

			parse_web_packet( wsi, (BYTE *)in, len );

			break;
		case LWS_CALLBACK_CLOSED:
			printf(KYEL"[Main Service] Client close.\n"RESET);
			del_client_sock( wsi );
			break;

		default:
			break;
	}

	return ERR_SUCCESS;
}

static int internal_recv_ctrl_data( unsigned short inter_msg, void * ptrdata )
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

static int create_websock( void )
{

#ifdef USE_WEBSOCK 
	int port = 6006;
	const char *interface = NULL;
	struct lws_context_creation_info info;
	struct libwebsocket_protocols protocol;
	// Not using ssl
	const char *cert_path = NULL;
	const char *key_path = NULL;
	int opts = 0;
	
	port = get_web_port_shm();

	memset( &g_sock_mng, 0, sizeof( g_sock_mng ));
	//* register the signal SIGINT handler */
	//* setup websocket protocol */
	protocol.name = "my-echo-protocol";
	protocol.callback = ws_service_callback;
	protocol.per_session_data_size=sizeof(struct per_session_data);
	protocol.rx_buffer_size = 0;

	//* setup websocket context info*/
	memset(&info, 0, sizeof( info));
	info.port = port;
	info.iface = interface;
	info.protocols = &protocol;
	info.extensions = libwebsocket_get_internal_extensions();
	info.ssl_cert_filepath = cert_path;
	info.ssl_private_key_filepath = key_path;
	info.gid = -1;
	info.uid = -1;
	info.options = opts;

	//* create libwebsocket context. */
	g_context = libwebsocket_create_context(&info);
	if (g_context == NULL) {
		printf(KRED"[Main] Websocket context create error.\n"RESET);
		return -1;
	}

	printf(KGRN"[Main] Websocket context create success.\n"RESET);
	//* websocket service */
	while ( !g_websock_done ) {
		libwebsocket_service(g_context, 500 );
	}

	usleep(10);
	if ( g_context != NULL )
	{
		libwebsocket_context_destroy(g_context);
	}
#else
	/* 사용하지 않음 경고창 없애기 목적 */
	struct libwebsocket_protocols protocol;
	protocol.callback = ws_service_callback;
	(void )&protocol;

#endif
	g_context = NULL;
	return ERR_SUCCESS;
}

static int internal_init( void ) 
{
	int ret = ERR_SUCCESS;

	memset( &g_sock_mng, 0, sizeof( g_sock_mng ));

	if ( create_websock() != ERR_SUCCESS )
	{
		ret = ERR_RETURN;
	}
	return ret;
}

static int internal_idle( void * ptr )
{
	( void ) ptr;

	//send_test_data();
	return ERR_SUCCESS;
}

static int internal_release( void )
{
	g_websock_done = 1;
	print_dbg( DBG_INFO,1, "Release of WebSocket");
	return ERR_SUCCESS;
}

int init_websock( send_data_func ptrsend_data ) 
{
	g_ptrsend_websock_control_func	= ptrsend_data;
	return internal_init();
}

int idle_websock( void * ptr )
{
	return internal_idle( ptr );
}

int release_websock( void )
{
	return internal_release();
}

/*================================================================================================
  CONTROL Module에서 센서 데이터를 보내는  함수.(다른 파일에서 호출하지 마세요)
================================================================================================*/
int recv_ctrl_sns_data_websock( unsigned short inter_msg, void * ptrdata )
{
	return internal_recv_ctrl_data( inter_msg, ptrdata );
}

