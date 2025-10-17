/*********************************************************************
	file : radio.c
	description :
	product by tory45( tory45@empal.com) ( 2016.08.05 )
	update by ?? ( . . . )
	
	- Control에서 fms_server로 보낼 데이터를 받아 fms_server로 보내는 기능
	- fms_server에서 데이터를 받아 Control로 정보를 보내는 기능 
 ********************************************************************/
#include "com_include.h"
#include "sem.h"
#include "radio.h"
#include "control.h"
#include "pwr_mng.h"
#include "ctrl_db.h"

#define RADIO_PROTOCOL	0xE2D8

#define FRE_NAME		"FRE"
#define FRS_NAME		"FRS"
#define SRE_NAME		"SRE"
#define FDE_NAME		"FDE"

#define NOTI_OFF		0
#define NOTI_ON			1
#define NOTI_INIT		2

#define MAX_RADIO_DATA_SIZE 700	
#define MAX_TCP_RECV_SIZE   ( MAX_RADIO_DATA_SIZE )	
#define MIN_TCP_QUEUE_SIZE	10
#define MAX_TCP_QUEUE_SIZE	( MAX_TCP_RECV_SIZE * 5 )
#define MAX_SEND_BUF_SIZE	300


//#define ALIVE_ACK_DBG		1
typedef struct test_
{
	float s;
	//BYTE * ptrdata;
	//ENUMFCLT_TYPE		eFclt;
}__attribute__ ((packed)) test_t;

typedef struct radio_head_
{
	unsigned short protocol;
	unsigned short opcode;
	unsigned int data_len;
	unsigned short direction;
	BYTE * ptrdata;
}__attribute__ ((packed)) radio_head_t;

typedef struct radio_queue_
{
	int head;
	char * ptrlist_buf;
	char * ptrrecv_buf;
}__attribute__ ((packed)) radio_queue_t; 

typedef struct radio_info_
{
	fclt_info_t fclt_info;
	volatile int tcp_fd;
	time_t last_send_time;
	radio_queue_t queue;
	volatile int alive_ack;

	/*2017 0124 */
	volatile time_t d_time;
	volatile int d_cnt;
	volatile int d_flag;

}__attribute__ ((packed)) radio_info_t;

typedef struct radio_manager_
{
	BYTE cnt;
	radio_info_t list[ MAX_FCLT_CNT ];
}radio_manager_t;

/*================================================================================================
 전역 변수 
 1.static 적용 
 2.숫자형 데이터 일경우 volatile 적용 
 3.초기값 적용.
================================================================================================*/
static send_data_func  g_ptrsend_data_control_func = NULL;		/* CONTROL 모듈  센서 데이터 ,  알람  정보를 넘기는 함수 포인트 */
static radio_manager_t g_radio_manager;
static volatile int g_release				= 0;
static volatile int g_clientid				= 0;

static volatile int g_fre_fclt_code			= EFCLT_EQP01;
static volatile int g_frs_fclt_code			= ( EFCLT_EQP01 + 1) ;
static volatile int g_sre_fclt_code			= ( EFCLT_EQP01 + 2) ;
static volatile int g_fde_fclt_code			= ( EFCLT_EQP01 + 3) ;

/* thread ( FRE : 고정 감시 ) */
static volatile int g_start_fre_pthread 	= 0; 
static volatile int g_end_fre_pthread 		= 1; 
static pthread_t fre_pthread;
static void * fre_pthread_idle( void * ptrdata );

/* thread ( SRE : 준고정 감시 ) */
static volatile int g_start_sre_pthread 	= 0; 
static volatile int g_end_sre_pthread 		= 1; 
static pthread_t sre_pthread;
static void * sre_pthread_idle( void * ptrdata );

/* thread ( FDE : 고정 방탐 ) */
static volatile int g_start_fde_pthread 	= 0; 
static volatile int g_end_fde_pthread 		= 1; 
static pthread_t fde_pthread;
static void * fde_pthread_idle( void * ptrdata );

static int create_radio_thread( void );
static int send_tcp_data( int fcltid, int fd, short cmd, int len, BYTE * ptrdata );
static radio_info_t * get_radio_withcode( int fcltcode );

static int close_radio_fd( int fcltcode , char * ptrname );
static int close_all_tcp( void );

/*================================================================================================
  내부 함수 선언
  static 적용 
================================================================================================*/
static int send_data_control( UINT16 inter_msg, void * ptrdata );
static int internal_init( void );
static int internal_idle( void * ptr );
static int internal_release( void );

/* Radio Recv Function */
static int parse_fre_dat_ack( UINT16 cmd, void * ptrdata );
static int parse_sre_dat_ack( UINT16 cmd, void * ptrdata );
static int parse_fde_dat_ack( UINT16 cmd, void * ptrdata );

//static int parse_fre_dat_req( UINT16 cmd, void * ptrdata );
static int parse_fre_dat_resp( UINT16 cmd, void * ptrdata );
static int parse_sre_dat_resp( UINT16 cmd, void * ptrdata );
static int parse_fde_dat_resp( UINT16 cmd, void * ptrdata );

static int parse_fre_ctl_resp( UINT16 cmd, void * ptrdata );
static int parse_sre_ctl_resp( UINT16 cmd, void * ptrdata );
static int parse_fde_ctl_resp( UINT16 cmd, void * ptrdata );

static int parse_fre_pwr_req( UINT16 cmd, void * ptrdata );
static int parse_sre_pwr_req( UINT16 cmd, void * ptrdata );
static int parse_fde_pwr_req( UINT16 cmd, void * ptrdata );

static int parse_fre_netinfo_req( UINT16 cmd, void * ptrdata );

/* Proc 함수 [ typedef int ( * proc_func )( unsigned short , void * ); ] */ 
static int send_com_pwr_req( int fclt_code, char * ptrname, fclt_ctrl_eqponoff_t * ptronoff   );
static int send_fre_pwr_req( int fclt_code ,fclt_ctrl_eqponoff_t * ptronoff );
static int send_sre_pwr_req( int fclt_code ,fclt_ctrl_eqponoff_t * ptronoff );
static int send_fde_pwr_req( int fclt_code ,fclt_ctrl_eqponoff_t * ptronoff );

static int recv_radio_eqp_pwr_req( unsigned short inter_msg, void * ptrdata );
static int recv_fclt_netinfo_res(unsigned short inter_msg, void * ptrdata );
static int recv_allim_msg(unsigned short inter_msg, void * ptrdata );

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
	/*=============================================*/
	{ EFT_EQPCTL_POWER,     recv_radio_eqp_pwr_req},	/* 전원제어 요청 ON/OFF */
    { EFT_FCLTNETINFO_RES,  recv_fclt_netinfo_res },
	{ EFT_ALLIM_MSG	,       recv_allim_msg },

	{ 0, NULL }
};

static proc_func_t g_parse_fre_func_list[ ] = 
{
	{ EFT_EQPDAT_ACK, 			parse_fre_dat_ack },
	{ EFT_EQPDAT_RES, 			parse_fre_dat_resp },
	{ EFT_EQPCTL_RES, 			parse_fre_ctl_resp },
	{ EFT_EQPCTL_POWER, 		parse_fre_pwr_req }, /* AUTO MODE */
    { EFT_FCLTNETINFO_REQ,      parse_fre_netinfo_req },

	{ 0xFF	, 	NULL }
};

static proc_func_t g_parse_sre_func_list[ ] = 
{
	{ EFT_EQPDAT_ACK, 	parse_sre_dat_ack },
	{ EFT_EQPDAT_RES, 	parse_sre_dat_resp },
	{ EFT_EQPCTL_RES, 	parse_sre_ctl_resp },

	{ EFT_EQPCTL_POWER, parse_sre_pwr_req }, /* AUTO MODE */

	{ 0xFF	, 	NULL }
};

static proc_func_t g_parse_fde_func_list[ ] = 
{
	{ EFT_EQPDAT_ACK, 	parse_fde_dat_ack },
	{ EFT_EQPDAT_RES, 	parse_fde_dat_resp },
	{ EFT_EQPCTL_RES, 	parse_fde_ctl_resp },

	{ EFT_EQPCTL_POWER, parse_fde_pwr_req }, /* AUTO MODE */

	{ 0xFF	, 	NULL }
};

/*================================================================================================
내부 함수  정의 
매개변수가 pointer일경우 반드시 NULL 체크 적용 
================================================================================================*/
static int get_radio_manager_info( int fcltcode )
{
	int pos = -1;
	int i ;

	for( i = 0 ; i < MAX_FCLT_CNT ; i++ )
	{
		if ( fcltcode == g_radio_manager.list[ i ].fclt_info.fcltcode )
		{
			return i;
		}
	}

	return pos ;
}

static int get_radio_empty_pos( void )
{
	int pos = -1;
	int i;

	for( i = 0 ; i < MAX_FCLT_CNT ; i++ )
	{
		if ( g_radio_manager.list[ i ].fclt_info.fcltcode == 0 && g_radio_manager.list[ i ].fclt_info.fcltid == 0 )
		{
			return i;
		}
	}

	return pos;
}

static int reload_fclt_data( int fcltcode, int pos, char * ptrname )
{
	fclt_info_t fclt;
	if ( pos < 0 )
	{
		print_dbg( DBG_ERR,1, "Can not find RADIO Data [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memset( &fclt, 0, sizeof( fclt ));
	if ( get_radio_fclt_info_ctrl( fcltcode , &fclt ) == ERR_SUCCESS )
	{
		memset( &g_radio_manager.list[ pos ].fclt_info, 0 , sizeof( fclt ));
		memcpy( &g_radio_manager.list[ pos ].fclt_info, &fclt, sizeof( fclt ));
		g_radio_manager.list[ pos ].tcp_fd 		= -1;
		g_radio_manager.list[ pos ].d_flag 		= NOTI_INIT;

		print_dbg( DBG_SAVE,1,"######## Reload %s Data  FcltID:%d, FcltCode:%d",ptrname, g_radio_manager.list[ pos ].fclt_info.fcltid,
				g_radio_manager.list[ pos ].fclt_info.fcltcode );
	}

	return ERR_SUCCESS;
}

static int add_fclt_data( int fcltcode, int pos, char * ptrname )
{
	fclt_info_t fclt;
	int cnt = 0;

	if ( pos < 0 )
	{
		print_dbg( DBG_ERR,1, "Can not find RADIO Data [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memset( &fclt, 0, sizeof( fclt ));

	cnt = g_radio_manager.cnt;

	if ( get_radio_fclt_info_ctrl( fcltcode , &fclt ) == ERR_SUCCESS )
	{
		memcpy( &g_radio_manager.list[ cnt ].fclt_info, &fclt, sizeof( fclt ));
		g_radio_manager.list[ cnt ].tcp_fd 		= -1;
		g_radio_manager.list[ cnt ].d_flag		= NOTI_INIT;

		g_radio_manager.cnt++;
		print_dbg( DBG_SAVE,1,"######## ADD %s INFO FcltID:%d, FcltCode:%d, IP:%s, Port:%d, TotalCnt:%d",
								ptrname, 
								g_radio_manager.list[ cnt ].fclt_info.fcltid,
				               	g_radio_manager.list[ cnt ].fclt_info.fcltcode,
				               	g_radio_manager.list[ cnt ].fclt_info.szip,
				               	g_radio_manager.list[ cnt ].fclt_info.port,
								g_radio_manager.cnt	);
	}
	else
	{
		print_dbg( DBG_ERR,1, "Cannot find RADIO INFO[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
	}

	return ERR_SUCCESS;
}

static int del_fclt_data( int fcltcode, int pos, char * ptrname )
{
	int k,j;
	fclt_info_t fclt;
	
	if ( pos < 0 )
	{
		print_dbg( DBG_ERR,1, "Can not find RADIO Data [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	print_dbg( DBG_SAVE,1,"######### Delete FCLT Code:%d, Name: %s, IP:%s, Port:%d", 
			  fcltcode, 
			  ptrname, 
			  g_radio_manager.list[ pos ].fclt_info.szip,
			  g_radio_manager.list[ pos ].fclt_info.port );

	memset( &g_radio_manager.list[ pos ].fclt_info, 0, sizeof( fclt ));
	g_radio_manager.cnt--;
	
	print_dbg( DBG_SAVE, 1, "######## Count of FCLT COUNT :%d After Delete", g_radio_manager.cnt );

	if ( g_radio_manager.cnt < 0 )
		g_radio_manager.cnt = 0;

	for( j = pos ; j < MAX_FCLT_CNT -1 ; j++ )
	{
		k = j+1;
		/* 이동 */
		if ( g_radio_manager.list[ k ].fclt_info.fcltcode != 0 &&
				g_radio_manager.list[ k ].fclt_info.fcltid != 0 )
		{
			memcpy( &g_radio_manager.list[ j ].fclt_info, &g_radio_manager.list[ k ].fclt_info, sizeof( fclt ));
			memset( &g_radio_manager.list[ k ].fclt_info, 0, sizeof( fclt ));
			print_dbg( DBG_SAVE,1,"######### Move FCLT Code:%d, Name: %s, i:%d, k:%d", fcltcode, ptrname, j, k );
		}
		else
		{
			break;
		}
	}

	return ERR_SUCCESS;
}

static int recv_allim_msg(unsigned short inter_msg, void * ptrdata )
{
	inter_msg_t * ptrmsg =NULL;
	allim_msg_t allim_msg ;

	int pos = 0;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}
	
	memset( &allim_msg, 0, sizeof( allim_msg ));
	ptrmsg = ( inter_msg_t *)ptrdata;

	if ( ptrmsg->ptrbudy == NULL )
	{
		print_dbg( DBG_ERR,1, "is budy [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	memcpy( &allim_msg, ptrmsg->ptrbudy, sizeof( allim_msg ));
	
	/* change of network */
	if ( allim_msg.allim_msg  == ALM_FRE_NET_CHG_NOTI_MSG  )
	{
		close_radio_fd( g_fre_fclt_code , "FRE" );
		pos = get_radio_manager_info( g_fre_fclt_code);
		reload_fclt_data( g_fre_fclt_code, pos, "FRE");
	}
	else if ( allim_msg.allim_msg  == ALM_SRE_NET_CHG_NOTI_MSG )
	{
		close_radio_fd( g_sre_fclt_code , "SRE" );
		pos = get_radio_manager_info( g_sre_fclt_code);
		reload_fclt_data( g_sre_fclt_code, pos, "SRE");
	}
	else if (  allim_msg.allim_msg  == ALM_FDE_NET_CHG_NOTI_MSG )
	{
		close_radio_fd( g_fde_fclt_code , "FDE" );
		pos = get_radio_manager_info( g_fde_fclt_code);
		reload_fclt_data( g_fde_fclt_code, pos, "FDE");
	}
	/* add of network */
	else if ( allim_msg.allim_msg  == ALM_FRE_NET_ADD_NOTI_MSG  )
	{
		close_radio_fd( g_fre_fclt_code , "FRE" );
		pos = get_radio_empty_pos( );
		add_fclt_data( g_fre_fclt_code, pos, "FRE");
	}
	else if ( allim_msg.allim_msg  == ALM_SRE_NET_ADD_NOTI_MSG )
	{
		close_radio_fd( g_sre_fclt_code , "SRE" );
		pos = get_radio_empty_pos( );
		add_fclt_data( g_sre_fclt_code, pos, "SRE");
	}
	else if (  allim_msg.allim_msg  == ALM_FDE_NET_ADD_NOTI_MSG )
	{
		close_radio_fd( g_fde_fclt_code , "FDE" );
		pos = get_radio_empty_pos( );
		add_fclt_data( g_fde_fclt_code, pos, "FDE");
	}
	/* del of network */
	else if ( allim_msg.allim_msg  == ALM_FRE_NET_DEL_NOTI_MSG  )
	{
		close_radio_fd( g_fre_fclt_code , "FRE" );
		pos = get_radio_manager_info( g_fre_fclt_code);
		del_fclt_data( g_fre_fclt_code, pos, "FRE");
	}
	else if ( allim_msg.allim_msg  == ALM_SRE_NET_DEL_NOTI_MSG )
	{
		close_radio_fd( g_sre_fclt_code , "SRE" );
		pos = get_radio_manager_info( g_sre_fclt_code);
		del_fclt_data( g_sre_fclt_code, pos, "SRE");
		//printf(".................del sre....................\r\n");
	}
	else if (  allim_msg.allim_msg  == ALM_FDE_NET_DEL_NOTI_MSG )
	{
		close_radio_fd( g_fde_fclt_code , "FDE" );
		pos = get_radio_manager_info( g_fde_fclt_code);
		del_fclt_data( g_fde_fclt_code, pos, "FDE");
		//printf(".................del fde...................\r\n");
	}

	return ERR_SUCCESS;
}

static int recv_radio_eqp_pwr_req( unsigned short inter_msg, void * ptrdata )
{
	int fcltid 		= 0; 
	int fcltcode 	= 0;
	int ret			= 0;
	int automode	= 0;
	int onoff		= 0;

	inter_msg_t * ptrmsg =NULL;
	radio_info_t * ptrradio = NULL;

	char sztime[ MAX_TIME_SIZE ];
	fclt_ctrl_eqponoff_t eqp_onoff;

	ptrmsg = ( inter_msg_t *)ptrdata;
	g_clientid = ptrmsg->client_id;

	if ( ptrmsg->ptrbudy == NULL )
	{
		print_dbg( DBG_ERR,1, "is budy [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memset( &eqp_onoff, 0, sizeof( eqp_onoff ));
	memset( sztime, 0, sizeof( sztime ));
	memcpy(&eqp_onoff, ptrmsg->ptrbudy , sizeof(eqp_onoff ));

	fcltid 		= eqp_onoff.ctl_base.tfcltelem.nfcltid;
	fcltcode 	= eqp_onoff.ctl_base.tfcltelem.efclt;
	automode	= eqp_onoff.bautomode;
	onoff		= eqp_onoff.ctl_base.ectlcmd;

	ptrradio = get_radio_withcode( fcltcode );

	if ( ptrradio == NULL )
	{
		print_dbg( DBG_ERR,1, "Can not find Fclt:%d [%s:%d:%s]",fcltcode ,__FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* 전원 제어 중 */

	get_cur_time( time( NULL ), sztime, NULL );
#if 1
	print_dbg( DBG_LINE, 1, NULL );
	print_dbg( DBG_INFO, 1, "RADIO::EQP Power Control Fcltid:%d, FcltCode:%d, Auto:%d, OnOff:%d, Time:%s", 
							 fcltid, 
							 fcltcode,
							 automode,
							 onoff,
							 sztime );
#endif
	if( fcltcode == g_fre_fclt_code )
	{
		ret = send_fre_pwr_req( fcltcode ,&eqp_onoff );
	}
	else if ( fcltcode == g_sre_fclt_code )
	{
		ret = send_sre_pwr_req( fcltcode ,&eqp_onoff );
	}
	else if ( fcltcode == g_fde_fclt_code )
	{
		ret = send_fde_pwr_req( fcltcode ,&eqp_onoff );
	}
	else
	{
		ret = ERR_RETURN;
	}

	return ret;
}

static int recv_fclt_netinfo_res( unsigned short inter_msg, void * ptrdata )
{
	int ret			= ERR_RETURN;

	inter_msg_t * ptrmsg =NULL;
    fclt_evt_fcltnetworkinfo_res_t  net_resp;
	radio_info_t * ptrradio = NULL;

	BYTE szsend_buf[ MAX_SEND_BUF_SIZE ];
	BYTE * ptrsend = NULL;
	radio_head_t head;

	int send_size = 0;
	int head_size = 0;
	int data_size = 0;
	UINT16 cmd = EFT_FCLTNETINFO_RES;


	ptrmsg = ( inter_msg_t *)ptrdata;
	if ( ptrmsg == NULL || ptrmsg->ptrbudy == NULL )
	{
		print_dbg( DBG_ERR,1, "is budy [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	ptrradio = get_radio_withcode( g_fre_fclt_code );

	if ( ptrradio == NULL )
	{
		print_dbg( DBG_ERR,1, "Can not find Fclt:%d [%s:%d:%s]",g_fre_fclt_code,__FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memset( &net_resp, 0, sizeof( net_resp));
	memcpy(&net_resp, ptrmsg->ptrbudy , sizeof(net_resp));

	/* Fclt Net  */
	memset( szsend_buf, 0, sizeof( szsend_buf ));
	ptrsend = (BYTE*)&szsend_buf[ 0 ];
	memset( &head, 0, sizeof( head ));
	head_size 	= ( sizeof( head ) - PNT_SIZE); 
	data_size 	= sizeof( net_resp );
	
	/* make head */
	head.protocol	= OP_FMS_PROT;
	head.opcode		= cmd;
	head.data_len	= data_size;
	head.direction 	= ESYSD_RTUEQP;

	/* head copy */
	memcpy( ptrsend, &head, head_size );
	ptrsend += head_size;
	send_size += head_size;

	/* data copy  */
	memcpy( ptrsend, &net_resp, data_size );
	send_size += data_size;

    print_dbg( DBG_INFO,1, "######## Send FCLT NET to FRE Radio " ); 

	if( send_tcp_data( g_fre_fclt_code, ptrradio->tcp_fd,  cmd, send_size, szsend_buf ) == ERR_SUCCESS )
	{
		ptrradio->last_send_time = time( NULL );
		ret = ERR_SUCCESS;
	}

	return ret;
}

#define MAX_D_CNT 		4
#define MAX_D_INTVAL	60

static int is_con_noti_msg( radio_info_t * ptrradio, int sts, time_t cur_time )
{
	int ret = 0;
	int cur_d_cnt =0 ;
	time_t  pre_d_time = 0;
	int cur_d_flag = 0;

	if ( ptrradio == NULL )
	{
		print_dbg( DBG_ERR,1, "Is not valid radio[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* 연결이 끊어 질 경우 */
	if ( sts == ESNSRCNNST_ERROR )
	{
		ptrradio->d_cnt++;
		cur_d_cnt = ptrradio->d_cnt;
		pre_d_time = ptrradio->d_time;
		
		/* 카운터가 2 이상일 경우 2로 한다 */
		if( cur_d_cnt > MAX_D_CNT  )
			ptrradio->d_cnt = MAX_D_CNT ;

		cur_d_flag = ptrradio->d_flag ;

		/* 이전에 끊어진 시간의 차이가 1분보다 작을 경우 */
		if ( ( cur_time - pre_d_time ) <= MAX_D_INTVAL )
		{
			/* 1분이내에 연결이 끊어진 상황이 2회 발생 하고 NOTI를 보내지 않았을 경우  서버로 에러 메세지 전송 */
			if ( (cur_d_cnt >= MAX_D_CNT ) && ( cur_d_flag != NOTI_ON ))
			{
				ptrradio->d_flag = NOTI_ON;
				ret = 1;
			}
		}
		/* 마지막 끊어진 시간 설정 */
		ptrradio->d_time = cur_time;
	}
	/*연결이 되었을 경우 */
	else if ( sts == ESNSRCNNST_NORMAL )
	{
		/* 서버로 연결 에러를 전송한 경우만 서버로 복구 메시지를 전송한다 
		 단, 초기 FLAG의  NOTI_INIT 단계에서는 무조건 서버로 연결 메시지를 전송한다. */
		if ( (ptrradio->d_flag == NOTI_INIT ) || (ptrradio->d_flag == NOTI_ON ) )
		{
			ret = 1;
		}
		
		/* 연결 정보를 초기화 한다 */
		ptrradio->d_time = 0;
		ptrradio->d_cnt  = 0;
		ptrradio->d_flag = NOTI_OFF;
	}

	return ret;
}

static int check_radio_connect_sts( int fcltid, int sts )
{
	radio_info_t * ptrradio = NULL;
	int i,cnt 	= 0;
	int l_fcltid = 0;
	int l_fcltcode = 0;
	time_t cur_time;
	int noti = 0;

	cnt = g_radio_manager.cnt;
	cur_time = time ( NULL );

	for( i = 0 ; i< cnt ; i++ )
	{
		ptrradio = (radio_info_t*)&g_radio_manager.list[ i ];
		
		if ( ptrradio != NULL )
		{
			l_fcltid 	= ptrradio->fclt_info.fcltid;
			l_fcltcode 	= ptrradio->fclt_info.fcltcode; 

			if( l_fcltid == fcltid )
			{
				/*2017_0124 */
				noti = is_con_noti_msg( ptrradio, sts, cur_time );
				/* 서버 보고 조건일 경우에만 서버로 보낸다 */
				if ( noti == 1 )
				{
					set_radio_tcp_conn_sts_ctrl( fcltid , l_fcltcode , sts );
				}
				if ( sts == ESNSRCNNST_NORMAL  )
				{
					if ( get_print_debug_shm() == 1 )
						print_dbg( DBG_INFO,1, "EQP : %d Tcp status once the terminal is connected( NOTI:%d)", l_fcltcode , noti); 
				}
				else
				{
					if ( get_print_debug_shm() == 1 )
						print_dbg( DBG_INFO,1, "EQP : %d Tcp status once the terminal is disconnect( NOTI:%d)", l_fcltcode, noti );  	
				}
				return ERR_SUCCESS;
			}
		}
	}

	return ERR_SUCCESS;
}

static int get_radio_info( void )
{
	fclt_info_t fclt;
	int cnt = 0;

	memset( &g_radio_manager, 0, sizeof( g_radio_manager ));
	memset( &fclt, 0, sizeof( fclt ));
	
	cnt = g_radio_manager.cnt;
	if ( get_radio_fclt_info_ctrl( g_fre_fclt_code, &fclt ) == ERR_SUCCESS )
	{
		memcpy( &g_radio_manager.list[ cnt ].fclt_info, &fclt, sizeof( fclt ));
		g_radio_manager.list[ cnt ].tcp_fd 		= -1;
		g_radio_manager.list[ cnt ].d_flag 		= NOTI_INIT;

		g_radio_manager.cnt++;
		print_dbg( DBG_INFO,1,"FRE INFO FcltID:%d, FcltCode:%d",g_radio_manager.list[ cnt ].fclt_info.fcltid,
				               g_radio_manager.list[ cnt ].fclt_info.fcltcode );
	}

	cnt = g_radio_manager.cnt;
	if ( get_radio_fclt_info_ctrl( g_sre_fclt_code, &fclt ) == ERR_SUCCESS )
	{
		memcpy( &g_radio_manager.list[ cnt ].fclt_info, &fclt, sizeof( fclt ));
		g_radio_manager.list[ cnt ].tcp_fd 		= -1;
		g_radio_manager.list[ cnt ].d_flag 		= NOTI_INIT;

		g_radio_manager.cnt++;
		print_dbg( DBG_INFO,1,"SRE INFO FcltID:%d, FcltCode:%d",g_radio_manager.list[ cnt ].fclt_info.fcltid,
				               g_radio_manager.list[ cnt ].fclt_info.fcltcode );
	}

	cnt = g_radio_manager.cnt;
	if ( get_radio_fclt_info_ctrl( g_fde_fclt_code, &fclt ) == ERR_SUCCESS )
	{
		memcpy( &g_radio_manager.list[ cnt ].fclt_info, &fclt, sizeof( fclt ));
		g_radio_manager.list[ cnt ].tcp_fd 		= -1;
		g_radio_manager.list[ cnt ].d_flag 		= NOTI_INIT;

		g_radio_manager.cnt++;
		print_dbg( DBG_INFO,1,"FDE INFO FcltID:%d, FcltCode:%d",g_radio_manager.list[ cnt ].fclt_info.fcltid,
				               g_radio_manager.list[ cnt ].fclt_info.fcltcode );
	}

	return ERR_SUCCESS;
}

/*===============================================================
  Send ACK  
  ===============================================================*/

static int send_com_ack( int fclt_code )
{
	BYTE szsend_buf[ MAX_SEND_BUF_SIZE ];
	BYTE * ptrsend = NULL;

	radio_info_t * ptrradio = NULL;
	fclt_alivecheckack_t alive_ack;
	radio_head_t head;

	int ret = ERR_RETURN;
	int send_size = 0;
	int head_size = 0;
	int data_size = 0;
	UINT16 cmd = EFT_EQPDAT_ACK;

	ptrradio = get_radio_withcode( fclt_code );

	if ( ptrradio == NULL )
	{
		print_dbg( DBG_ERR,1, "Can not find Fclt:%d [%s:%d:%s]",fclt_code ,__FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
#if 0	
	if( ptrradio->alive_ack == 0 )
	{
		print_dbg( DBG_INFO,1, "FRE is not recv alive ack:[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
#endif

	memset( &alive_ack, 0, sizeof( alive_ack)); 
	memset( szsend_buf, 0, sizeof( szsend_buf ));

	ptrsend = (BYTE*)&szsend_buf[ 0 ];
	memset( &head, 0, sizeof( head ));

	head_size 	= ( sizeof( head ) - PNT_SIZE); 
	data_size 	= sizeof( alive_ack );


	/* make head */
	head.protocol	= OP_FMS_PROT;
	head.opcode		= cmd;
	head.data_len	= data_size;
	head.direction 	= ESYSD_RTUEQP;

	/* make data */
	alive_ack.tfcltelem.nsiteid = get_my_siteid_shm();			/* site id */
	alive_ack.tfcltelem.nfcltid	= ptrradio->fclt_info.fcltid;	/* FCTLT id */
	alive_ack.tfcltelem.efclt 	= fclt_code;				/* EQP */
	alive_ack.timet 			= time( NULL );					/* TIME */

	/* head copy */
	memcpy( ptrsend, &head, head_size );
	ptrsend += head_size;
	send_size += head_size;

	/* data copy  */
	memcpy( ptrsend, &alive_ack, data_size );
	send_size += data_size;

	if( send_tcp_data( fclt_code, ptrradio->tcp_fd,  cmd, send_size, szsend_buf ) == ERR_SUCCESS )
	{
		ptrradio->last_send_time = time( NULL );
		ret = ERR_SUCCESS;
#ifdef ALIVE_ACK_DBG
		print_dbg( DBG_SAVE, 1, "Send EQP:%d ALIVE ACK Message", fclt_code );
#else
		print_dbg( DBG_INFO, 1, "Send EQP:%d ALIVE ACK Message", fclt_code );
#endif

	}

	return ret;
}

/*===============================================================
  Send Data REQ  
  ===============================================================*/

static int send_com_eqpdata_req( int fclt_code, char * ptrname )
{
	BYTE szsend_buf[ MAX_SEND_BUF_SIZE ];
	BYTE * ptrsend = NULL;

	radio_info_t * ptrradio = NULL;
	fclt_ctl_requestrsltdata_t fclt_data_req;
	radio_head_t head;

	int ret = ERR_RETURN;
	int send_size = 0;
	int head_size = 0;
	int data_size = 0;
	UINT16 cmd = EFT_EQPDAT_REQ;
    int fcltid = 0;
    fclt_info_t frs_fclt;

    if ( fclt_code == g_frs_fclt_code )
    {
        ptrradio = get_radio_withcode( g_fre_fclt_code );
        if ( ptrradio == NULL )
        {
            print_dbg( DBG_ERR,1, "Can not find Fclt:%d [%s:%d:%s]",g_fre_fclt_code ,__FILE__, __LINE__,__FUNCTION__);
            return ERR_RETURN;
        }

        memset(&frs_fclt, 0, sizeof( frs_fclt ));
        
        /* 고정운영은  ctrol에서 직접 가져온다 */
        if ( get_radio_fclt_info_ctrl( g_frs_fclt_code, &frs_fclt ) != ERR_SUCCESS )
        {
            print_dbg( DBG_ERR,1, "Can not find Fclt:%d [%s:%d:%s]",g_frs_fclt_code ,__FILE__, __LINE__,__FUNCTION__);
            return ERR_RETURN;
        }

        fcltid = frs_fclt.fcltid;

    }
    else
    {
        ptrradio = get_radio_withcode( fclt_code );
        if ( ptrradio == NULL )
        {
            print_dbg( DBG_ERR,1, "Can not find Fclt:%d [%s:%d:%s]",fclt_code ,__FILE__, __LINE__,__FUNCTION__);
            return ERR_RETURN;
        }

        fcltid = ptrradio->fclt_info.fcltid;
    }

#if 0	
	if( ptrradio->alive_ack == 0 )
	{
		print_dbg( DBG_INFO,1, "%s is not recv alive ack:[%s:%d:%s]",ptrname, __FILE__, __LINE__,__FUNCTION__);
		return -2;
	}
#endif

	memset( &fclt_data_req, 0, sizeof( fclt_data_req )); 
	memset( szsend_buf, 0, sizeof( szsend_buf ));

	ptrsend = (BYTE*)&szsend_buf[ 0 ];
	memset( &head, 0, sizeof( head ));
	head_size 	= ( sizeof( head ) - PNT_SIZE); 
	data_size 	= sizeof( fclt_ctl_requestrsltdata_t );
	
	/* make head */
	head.protocol	= OP_FMS_PROT;
	head.opcode		= cmd;
	head.data_len	= data_size;
	head.direction 	= ESYSD_RTUEQP;

	/* make data */
	fclt_data_req.tfcltelem.nsiteid = get_my_siteid_shm();			/* site id */
	fclt_data_req.tfcltelem.nfcltid	= fcltid;	                    /* FCTLT id */
	fclt_data_req.tfcltelem.efclt 	= fclt_code;					/* FRE 또는 FRS */

	fclt_data_req.timet 			= time( NULL );					/* TIME */
	fclt_data_req.ectltype			= EFCLTCTLTYP_RSLTREQ; 			/* req monitering data */
	fclt_data_req.ectlcmd			= EFCLTCTLCMD_QUERY; 			/* req monitering data */

	/* head copy */
	memcpy( ptrsend, &head, head_size );
	ptrsend += head_size;
	send_size += head_size;

	/* data copy  */
	memcpy( ptrsend, &fclt_data_req, data_size );
	send_size += data_size;

	if( send_tcp_data( fclt_code, ptrradio->tcp_fd,  cmd, send_size, szsend_buf ) == ERR_SUCCESS )
	{
		ptrradio->last_send_time = time( NULL );
		ret = ERR_SUCCESS;
	}

	return ret;

}

static int send_fre_eqpdata_req( int fclt_code )
{
 	if ( fclt_code == g_fre_fclt_code )
        send_com_eqpdata_req( fclt_code, "FRE");
    else
 	    send_com_eqpdata_req( fclt_code, "FRS");

    return ERR_SUCCESS;
}

static int send_sre_eqpdata_req( void )
{
 	return send_com_eqpdata_req( g_sre_fclt_code , "SRE");
}

static int send_fde_eqpdata_req( void )
{
 	return send_com_eqpdata_req( g_fde_fclt_code , "FDE");
}

/*===============================================================
  Send Power ctl REQ  
  ===============================================================*/
static int send_com_pwr_req( int fclt_code, char * ptrname, fclt_ctrl_eqponoff_t * ptronoff )
{
	BYTE szsend_buf[ MAX_SEND_BUF_SIZE ];
	BYTE * ptrsend = NULL;

	radio_info_t * ptrradio = NULL;
	radio_head_t head;

	int ret = ERR_RETURN;
	int send_size = 0;
	int head_size = 0;
	int data_size = 0;
	UINT16 cmd = EFT_EQPCTL_POWER;

	ptrradio = get_radio_withcode( fclt_code );
	
	if ( ptronoff == NULL )
	{
		print_dbg( DBG_ERR,1, "Is null ptronoff [%s:%d:%s]",fclt_code ,__FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	if ( ptrradio == NULL )
	{
		//print_dbg( DBG_INFO,1, "Can not find Fclt:%d [%s:%d:%s]",g_fre_fclt_code ,__FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

#if 0
	if( ptrradio->alive_ack == 0 )
	{
		print_dbg( DBG_INFO,1, "%s is not recv alive ack:[%s:%d:%s]",ptrname, __FILE__, __LINE__,__FUNCTION__);
		return -2;
	}
#endif

	memset( szsend_buf, 0, sizeof( szsend_buf ));

	ptrsend = (BYTE*)&szsend_buf[ 0 ];
	memset( &head, 0, sizeof( head ));
	head_size 	= ( sizeof( head ) - PNT_SIZE); 
	data_size 	= sizeof( fclt_ctrl_eqponoff_t );
	
	/* make head */
	head.protocol	= OP_FMS_PROT;
	head.opcode		= cmd;
	head.data_len	= data_size;
	head.direction 	= ESYSD_RTUEQP;

	/* head copy */
	memcpy( ptrsend, &head, head_size );
	ptrsend += head_size;
	send_size += head_size;

	/* data copy  */
	memcpy( ptrsend, ptronoff, data_size );
	send_size += data_size;

	if( send_tcp_data( fclt_code, ptrradio->tcp_fd,  cmd, send_size, szsend_buf ) == ERR_SUCCESS )
	{
		ptrradio->last_send_time = time( NULL );
		ret = ERR_SUCCESS;
	}

	return ret;
}

static int send_fre_pwr_req( int fclt_code ,fclt_ctrl_eqponoff_t * ptronoff )
{
 	return send_com_pwr_req( g_fre_fclt_code , "FRE",ptronoff );
}

static int send_sre_pwr_req( int fclt_code, fclt_ctrl_eqponoff_t * ptronoff  )
{
 	return send_com_pwr_req( g_sre_fclt_code , "SRE", ptronoff );
}

static int send_fde_pwr_req( int fclt_code , fclt_ctrl_eqponoff_t * ptronoff  )
{
 	return send_com_pwr_req( g_fde_fclt_code , "FDE", ptronoff );
}

static int parse_com_dat_req( void * ptrdata, int fcltcode, radio_head_t * ptrhead, void * ptrbudy , int size )
{
	radio_head_t * ptr = NULL;
	int head_size =0;

	if ( ptrdata == NULL || ptrhead == NULL || ptrbudy == NULL )
	{
		print_dbg( DBG_ERR,1, "isnull ptrdata [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptr = ( radio_head_t *)ptrdata;

	/* point 값까지 복사해야 함 */ 
	head_size = ( sizeof( radio_head_t ) );

	if ( ptr != NULL )
	{
		memcpy( ptrhead , ptr , head_size );
	}

	if ( ptrhead->ptrdata != NULL )
	{
		memcpy( ptrbudy, ptrhead->ptrdata, size );
		//print_buf( size, ptrbudy );
	}
	
	return ERR_SUCCESS;
}

static int parse_com_dat_resp( UINT16 cmd, void * ptrdata, int fclt_code, char * ptrname )
{
	int size =0;
	radio_head_t head ;
	fclt_collectrslt_dido_t dido_data;
	int i, cnt, pos;
	radio_data_t radio_data;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "isnull ptrdata [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memset(&dido_data , 0, sizeof( dido_data ));
	memset(&radio_data, 0, sizeof( radio_data ));
	memset(&head, 0, sizeof( head));

	size = sizeof( dido_data );

	if( parse_com_dat_req( ptrdata, fclt_code, &head , &dido_data, size ) == ERR_RETURN )
	{
		print_dbg( DBG_ERR,1, "invalid %s data req  [%s:%d:%s]", ptrname, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	cnt = dido_data.aidido.nfcltsubcount;
	pos = 0;

	for( i = 0 ; i < cnt ; i++ )
	{
		if ( pos >= MAX_RADIO_SNS_CNT )
			break;
#if 0
		print_dbg( DBG_INFO,1,"FRE POS:%d, SnsrType:%d, SnsrSub:%d, IdxSub:%d, cnnSts:%d, Rslt:%d",
							  	pos, 
								dido_data.rslt[ i ].esnsrtype, 
								dido_data.rslt[ i ].esnsrsub, 
								dido_data.rslt[ i ].idxsub ,
								dido_data.rslt[ i ].ecnnstatus,
								dido_data.rslt[ i ].brslt );
#endif
		radio_data.sub_type[ pos ] 	= dido_data.rslt[ i ].esnsrsub;
		//radio_data.sub_type[ pos ] 	= ESNSRSUB_EQPFRS_RX;
		radio_data.val[ pos ] 		= dido_data.rslt[ i ].brslt;
		pos++;
	}
#if 0
	print_dbg( DBG_INFO,1, "Recv %s DATA RESP Protocol:%d, OPCode:%d, Size:%d, Direction:%d",
							ptrname, head.protocol, head.opcode, head.data_len, head.direction);

	print_dbg( DBG_INFO,1,"%s SiteID:%d, FcltID%d, eFclt:(%d), FcltCnt:%d",ptrname, 	
																		dido_data.aidido.tfcltelem.nsiteid,
																		dido_data.aidido.tfcltelem.nfcltid, 
																		dido_data.aidido.tfcltelem.efclt,
																		dido_data.aidido.nfcltsubcount);
#endif

	if ( dido_data.aidido.tfcltelem.efclt == g_fre_fclt_code )
	{
		set_fre_data_ctrl( &radio_data );

	}
	else if ( dido_data.aidido.tfcltelem.efclt == g_frs_fclt_code )
	{
		set_frs_data_ctrl( &radio_data );
	}
	else if ( dido_data.aidido.tfcltelem.efclt == g_sre_fclt_code )
	{
		set_sre_data_ctrl( &radio_data );
	}
	else if ( dido_data.aidido.tfcltelem.efclt == g_fde_fclt_code )
	{
		set_fde_data_ctrl( &radio_data );
	}
	

	send_com_ack( fclt_code );

	if ( dido_data.aidido.tfcltelem.efclt == g_fre_fclt_code  )
	{
		sys_sleep( 100 );
		send_fre_eqpdata_req( g_frs_fclt_code );
	}

	return ERR_SUCCESS;
}

static int parse_fre_dat_resp( UINT16 cmd, void * ptrdata )
{
	parse_com_dat_resp( cmd, ptrdata, g_fre_fclt_code,"FRE"  );
	return ERR_SUCCESS;
}

static int parse_sre_dat_resp( UINT16 cmd, void * ptrdata )
{
	parse_com_dat_resp( cmd, ptrdata, g_sre_fclt_code,"SRE"  );

	return ERR_SUCCESS;
}
static int parse_fde_dat_resp( UINT16 cmd, void * ptrdata )
{
	parse_com_dat_resp( cmd, ptrdata, g_fde_fclt_code,"FDE"  );

	return ERR_SUCCESS;
}

/*===============================================================
 	Parsing ACK  
  ===============================================================*/

static int parse_com_dat_ack( UINT16 cmd, void * ptrdata, int fclt_code , char * ptrname )
{
	radio_head_t * ptrhead = NULL;
	radio_info_t * ptrradio = NULL;

	fclt_alivecheckack_t alive_ack;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "isnull ptrdata [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrradio = get_radio_withcode( fclt_code );

	if( ptrradio == NULL )
	{
		print_dbg( DBG_ERR,1, "isnull ptrradio [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrhead = ( radio_head_t *)ptrdata;
	memset( &alive_ack, 0, sizeof(alive_ack));
	if ( ptrhead->ptrdata != NULL )
	{
		memcpy( &alive_ack, ptrhead->ptrdata, sizeof( alive_ack ));
	}

	print_dbg( DBG_INFO,1, "Recv %s EQP AliveACK RECV Protocol:%d, OPCode:%d, Size:%d, Direction:%d, SiteID:%d, FclitID:%d, eFclt:%d, Time:%lu",
							ptrname, ptrhead->protocol, ptrhead->opcode, ptrhead->data_len, ptrhead->direction,
							alive_ack.tfcltelem.nsiteid, alive_ack.tfcltelem.nfcltid,alive_ack.tfcltelem.efclt, (time_t)alive_ack.timet);
							
#ifdef ALIVE_ACK_DBG
		print_dbg( DBG_SAVE,1, "Recv %s EQP AliveACK RECV Protocol:%d, OPCode:%d, Size:%d, Direction:%d, SiteID:%d, FclitID:%d, eFclt:%d, Time:%lu",
							ptrname, ptrhead->protocol, ptrhead->opcode, ptrhead->data_len, ptrhead->direction,
							alive_ack.tfcltelem.nsiteid, alive_ack.tfcltelem.nfcltid,alive_ack.tfcltelem.efclt, (time_t)alive_ack.timet);
#else
	print_dbg( DBG_INFO,1, "Recv %s EQP AliveACK RECV Protocol:%d, OPCode:%d, Size:%d, Direction:%d, SiteID:%d, FclitID:%d, eFclt:%d, Time:%lu",
							ptrname, ptrhead->protocol, ptrhead->opcode, ptrhead->data_len, ptrhead->direction,
							alive_ack.tfcltelem.nsiteid, alive_ack.tfcltelem.nfcltid,alive_ack.tfcltelem.efclt, (time_t)alive_ack.timet);
#endif
	ptrradio->alive_ack = 1;
	
	send_com_ack( fclt_code );

	return ERR_SUCCESS;
}

static int parse_fre_dat_ack( UINT16 cmd, void * ptrdata )
{
	parse_com_dat_ack( cmd, ptrdata, g_fre_fclt_code, "FRE" );
	return ERR_SUCCESS;
}

static int parse_sre_dat_ack( UINT16 cmd, void * ptrdata )
{
	parse_com_dat_ack( cmd, ptrdata, g_sre_fclt_code, "SRE" );
	return ERR_SUCCESS;
}

static int parse_fde_dat_ack( UINT16 cmd, void * ptrdata )
{
	parse_com_dat_ack( cmd, ptrdata, g_fde_fclt_code, "FDE" );
	return ERR_SUCCESS;
}

/*===============================================================
 	Parsing Onoff Result Control  
  ===============================================================*/
/*===============================================================
  parse 
typedef struct radio_head_
{
	short protocol;
	short opcode;
	int data_len;
	short direction;
	BYTE * ptrdata;
}__attribute__ ((packed)) radio_head_t;

  ===============================================================*/

static int parse_com_ctl_resp( UINT16 cmd, void * ptrdata, int fclt_code, char * ptrname )
{
	int size =0;
	radio_head_t head ;
	radio_head_t temp_head;
	fclt_ctrl_eqponoffrslt_t onoff_resp;
	fclt_ctrl_eqponoffrslt_old_t onoff_resp_old;
	inter_msg_t inter_msg;
    fms_ctl_base_t ctl_base;
    fms_ctl_base_old_t ctl_base_old;

	radio_data_t radio_data;
	radio_info_t * ptrradio = NULL;
	int onoff;
	int maxstep;
	int curstep;
	int cursts;
	int ret = 0;

	tid_t tid;
    time_t cur_time = 0;
    int snsr_sub = 0;
    static volatile int send_resp_cnt  =0;
	radio_head_t * ptr = NULL;
	int head_size =0;
	int old = 0;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "isnull ptrdata [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	memset(&onoff_resp, 0, sizeof( onoff_resp));
	memset(&onoff_resp_old, 0, sizeof( onoff_resp_old ));

    memset( &ctl_base, 0, sizeof( ctl_base ));
    memset( &ctl_base_old, 0, sizeof( ctl_base_old ));

    memset(&tid, 0, sizeof( tid ));
	memset(&radio_data, 0, sizeof( radio_data ));
	memset(&head, 0, sizeof( head));
	memset(&temp_head, 0, sizeof( temp_head));
	memset(&inter_msg, 0, sizeof( inter_msg ));

	ptr = ( radio_head_t *)ptrdata;
	/* point 값까지 복사해야 함 */ 
	head_size = ( sizeof( radio_head_t ) );

	if ( ptr != NULL )
	{
		memcpy( &temp_head, ptr , head_size );
	}

	/* 구버전 프로토콜일 경우 */
	if ( temp_head.data_len == sizeof( onoff_resp_old ) )
	{
		size = sizeof( onoff_resp_old );
		//print_buf( size, ptrbudy );
		ret = parse_com_dat_req( ptrdata, fclt_code, &head , &onoff_resp_old, size );
		old = 1;
	}
	else
	{
		size = sizeof( onoff_resp );
		ret = parse_com_dat_req( ptrdata, fclt_code, &head , &onoff_resp, size );
	}

	if( ret == ERR_RETURN )
	{
		print_dbg( DBG_ERR,1, "invalid %s data req  [%s:%d:%s]", ptrname, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	if ( old == 1 )
	{
		print_dbg( DBG_SAVE,1, "######## Convert EQP RESP PACKET OLD TO NEW" );
		memcpy( &onoff_resp.ctl_base.tfcltelem , &onoff_resp_old.ctl_base.tfcltelem, sizeof( fms_elementfclt_t ));
		memcpy( &onoff_resp.ctl_base.tdbtans, &onoff_resp_old.ctl_base.tdbtans , sizeof( fms_transdbdata_t ));
		memcpy( &onoff_resp.ctl_base.suserid, &onoff_resp_old.ctl_base.suserid, MAX_USER_ID_OLD );

		onoff_resp.ctl_base.esnsrtype 	= onoff_resp_old.ctl_base.esnsrtype;
		onoff_resp.ctl_base.esnsrsub  	= onoff_resp_old.ctl_base.esnsrsub;
		onoff_resp.ctl_base.idxsub 		= onoff_resp_old.ctl_base.idxsub ;
		onoff_resp.ctl_base.ectltype	= onoff_resp_old.ctl_base.ectltype;
		onoff_resp.ctl_base.ectlcmd 	= onoff_resp_old.ctl_base.ectlcmd;
	
		onoff_resp.emaxstep				= onoff_resp_old.emaxstep;
		onoff_resp.ecurstep				= onoff_resp_old.ecurstep;
		onoff_resp.ecurstatus			= onoff_resp_old.ecurstatus;
		onoff_resp.ersult				= onoff_resp_old.ersult;
	}

	ptrradio = get_radio_withcode( fclt_code );

	if( ptrradio == NULL )
	{
		print_dbg( DBG_ERR,1, "isnull ptrradio [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	cur_time = time( NULL );

	if ( send_resp_cnt++ >= MAX_SEND_CNT )
	{
		send_resp_cnt  = 1;
	}

	/* make tid */
	tid.cur_time 	= cur_time;
	tid.siteid 		= get_my_siteid_shm();
	tid.sens_type 	= ESNSRTYP_DO;
	tid.seq 		= send_resp_cnt;

	cnvt_fclt_tid( &tid );
	//fcltcode 	= onoff_req.ctl_base.tfcltelem.efclt;
	//onoff		= onoff_req.ctl_base.ectlcmd;
   
    if ( fclt_code == g_fre_fclt_code )
        snsr_sub = ESNSRSUB_EQPFRE_POWER;
    else if ( fclt_code == g_sre_fclt_code  )
        snsr_sub = ESNSRSUB_EQPSRE_POWER ;
    else if ( fclt_code == g_fde_fclt_code )
        snsr_sub = ESNSRSUB_EQPFDE_POWER;

    ctl_base.tfcltelem.nsiteid  = get_my_siteid_shm();
    ctl_base.tfcltelem.nfcltid  = ptrradio->fclt_info.fcltid;
    ctl_base.tfcltelem.efclt    = fclt_code;

    ctl_base.esnsrtype          = ESNSRTYP_DO;
    ctl_base.esnsrsub           = snsr_sub;
    ctl_base.idxsub             = 0;

    ctl_base.tdbtans.tdatetime  = (UINT32)cur_time;
    memcpy( ctl_base.tdbtans.sdbfclt_tid, tid.szdata, MAX_DBFCLT_TID  );

    ctl_base.ectltype           = onoff_resp.ctl_base.ectltype;
    ctl_base.ectlcmd            = onoff_resp.ctl_base.ectlcmd;

    memcpy( &onoff_resp.ctl_base, &ctl_base, sizeof( ctl_base ));
	
	onoff		= onoff_resp.ctl_base.ectlcmd;
	maxstep		= onoff_resp.emaxstep;
	curstep 	= onoff_resp.ecurstep;
	cursts		= onoff_resp.ecurstatus;
    //onoff_resp.ersult   = EEQPONOFFRSLT_OK;

	print_dbg( DBG_INFO,1,"Resp  EQP->RTU Power Status FcltCode:%d, ONOFF:%d, MaxStep:%d, CurStep:%d, CurSts:%d, Result:%d",
			               fclt_code, onoff, maxstep, curstep, cursts, onoff_resp.ersult );

	/* power manager로 전송 */
	inter_msg.msg		= cmd;
	inter_msg.client_id = g_clientid;
	inter_msg.ptrbudy	= &onoff_resp;

	recv_pwr_ctrl_resp_pwr_mng( cmd , &inter_msg);

	/* 전원이 ON 후 전원 제어 상태를 해제한다. */
	if (( onoff == EFCLTCTLCMD_ON) && ( maxstep != 0 ) && ( maxstep == curstep ))
	{
		g_clientid 		= 0;
		//print_dbg( DBG_ERR,1, "Release EQP(%d) Power Ctrl MaxStep:%d, CurStep:%d", fclt_code, maxstep, curstep );
	}
    else if ((onoff == EFCLTCTLCMD_OFF ) && ( curstep == 1 ))
    {
		g_clientid 		= 0;
		//print_dbg( DBG_ERR,1, "Release EQP(%d) Power Ctrl MaxStep:%d, CurStep:%d", fclt_code, maxstep, curstep );
    }
	//send_com_ack( fclt_code );
	return ERR_SUCCESS;
}

static int parse_fre_ctl_resp( UINT16 cmd, void * ptrdata )
{
	parse_com_ctl_resp( cmd, ptrdata, g_fre_fclt_code, "FRE" );
	return ERR_SUCCESS;
}

static int parse_sre_ctl_resp( UINT16 cmd, void * ptrdata )
{
	parse_com_ctl_resp( cmd, ptrdata, g_sre_fclt_code, "SRE" );
	return ERR_SUCCESS;
}

static int parse_fde_ctl_resp( UINT16 cmd, void * ptrdata )
{
	parse_com_ctl_resp( cmd, ptrdata, g_fde_fclt_code, "FDE" );
	return ERR_SUCCESS;
}

/*===============================================================
 	Parsing Onoff AUTO MODE  Control  
  ===============================================================*/
static int parse_com_pwr_req( UINT16 cmd, void * ptrdata, int fclt_code, char * ptrname )
{
	int size =0;
	radio_head_t head ;
	fclt_ctrl_eqponoff_t onoff_req;
    fms_ctl_base_t ctl_base;

	inter_msg_t inter_msg;

	radio_data_t radio_data;
	radio_info_t * ptrradio = NULL;
	tid_t tid;
    time_t cur_time = 0;
    int snsr_sub = 0;
    static volatile int send_cnt  =0;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "isnull ptrdata [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	memset(&onoff_req, 0, sizeof( onoff_req));
    memset(&ctl_base, 0, sizeof( ctl_base ));
    memset(&tid, 0, sizeof( tid ));

	memset(&radio_data, 0, sizeof( radio_data ));
	memset(&head, 0, sizeof( head));
	memset(&inter_msg, 0, sizeof( inter_msg ));

	size = sizeof( onoff_req );

	if( parse_com_dat_req( ptrdata, fclt_code, &head , &onoff_req, size ) == ERR_RETURN )
	{
		print_dbg( DBG_ERR,1, "invalid %s data req  [%s:%d:%s]", ptrname, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
    
	ptrradio = get_radio_withcode( fclt_code );

	if( ptrradio == NULL )
	{
		print_dbg( DBG_ERR,1, "isnull ptrradio [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	cur_time = time( NULL );

	if ( send_cnt++ >= MAX_SEND_CNT )
	{
		send_cnt  = 1;
	}

	/* make tid */
	tid.cur_time 	= cur_time;
	tid.siteid 		= get_my_siteid_shm();
	tid.sens_type 	= ESNSRTYP_DO;
	tid.seq 		= send_cnt;

	cnvt_fclt_tid( &tid );
	//fcltcode 	= onoff_req.ctl_base.tfcltelem.efclt;
	//onoff		= onoff_req.ctl_base.ectlcmd;
   
    if ( fclt_code == g_fre_fclt_code )
        snsr_sub = ESNSRSUB_EQPFRE_POWER;
    else if ( fclt_code == g_sre_fclt_code  )
        snsr_sub = ESNSRSUB_EQPSRE_POWER ;
    else if ( fclt_code == g_fde_fclt_code )
        snsr_sub = ESNSRSUB_EQPFDE_POWER;

    ctl_base.tfcltelem.nsiteid  = get_my_siteid_shm();
    ctl_base.tfcltelem.nfcltid  = ptrradio->fclt_info.fcltid;
    ctl_base.tfcltelem.efclt    = fclt_code;

    ctl_base.esnsrtype          = ESNSRTYP_DO;
    ctl_base.esnsrsub           = snsr_sub;
    ctl_base.idxsub             = 0;

    ctl_base.tdbtans.tdatetime  = (UINT32)cur_time;
    memcpy( ctl_base.tdbtans.sdbfclt_tid, tid.szdata, MAX_DBFCLT_TID  );

    ctl_base.ectltype           = EFCLTCTLTYP_DOCTRL;
    ctl_base.ectlcmd            = EFCLTCTLCMD_OFF;

    memcpy( &onoff_req.ctl_base, &ctl_base, sizeof( ctl_base ));
	print_dbg( DBG_INFO,1,"Recv EQP->RTU Power REQ FcltCode:%d, ONOFF:%d, AutoMode:%d",
			               fclt_code, EFCLTCTLCMD_OFF, onoff_req.bautomode );

	/* power manager로 전송 */
	inter_msg.msg		= cmd;
	inter_msg.client_id = 0;
	inter_msg.ptrbudy	= &onoff_req;

	recv_pwr_ctrl_resp_pwr_mng( cmd , &inter_msg);

	//send_com_ack( fclt_code );
	return ERR_SUCCESS;
}
static int parse_fre_pwr_req( UINT16 cmd, void * ptrdata )
{
	parse_com_pwr_req( cmd, ptrdata, g_fre_fclt_code, "FRE");
	return ERR_SUCCESS;
}

static int parse_sre_pwr_req( UINT16 cmd, void * ptrdata )
{
	parse_com_pwr_req( cmd, ptrdata, g_sre_fclt_code, "SRE");
	return ERR_SUCCESS;

}

static int parse_fde_pwr_req( UINT16 cmd, void * ptrdata )
{
	parse_com_pwr_req( cmd, ptrdata, g_fde_fclt_code, "FDE");
	return ERR_SUCCESS;
}

static int parse_fre_cmd( UINT16 cmd, void * ptrdata )
{
	int i,cnt = 0;
	proc_func tmp_parse = NULL;
	UINT16 l_cmd = 0;

	cnt = ( sizeof( g_parse_fre_func_list ) / sizeof( proc_func_t ));

	for( i = 0 ; i < cnt ; i++ )
	{
		tmp_parse 	= g_parse_fre_func_list[ i ].ptrproc ;
		l_cmd 		= g_parse_fre_func_list[ i ].inter_msg; 

		if ( tmp_parse!= NULL && cmd == l_cmd )
		{
			tmp_parse( cmd, ptrdata );
		}
	}

	return ERR_SUCCESS;	
}

static int parse_sre_cmd( UINT16 cmd, void * ptrdata )
{
	int i,cnt = 0;
	proc_func tmp_parse = NULL;
	UINT16 l_cmd = 0;

	cnt = ( sizeof( g_parse_sre_func_list ) / sizeof( proc_func_t));

	for( i = 0 ; i < cnt ; i++ )
	{
		tmp_parse 	= g_parse_sre_func_list[ i ].ptrproc ;
		l_cmd 		= g_parse_sre_func_list[ i ].inter_msg; 

		if ( tmp_parse!= NULL && cmd == l_cmd )
		{
			tmp_parse( cmd, ptrdata );
		}
	}
	return ERR_SUCCESS;	
}

static int parse_fre_netinfo_req( UINT16 cmd, void * ptrdata )
{
	fclt_net_t net_info;

    fclt_evt_fcltnetworkinfo_req_t  net_req;
    fclt_evt_fcltnetworkinfo_res_t  net_resp;

	BYTE szsend_buf[ MAX_SEND_BUF_SIZE ];
	BYTE * ptrsend = NULL;
	radio_head_t head;
    radio_head_t * ptrhead= NULL;
	radio_info_t * ptrradio = NULL;

    int site_id, fclt_id;
	UINT16 send_cmd = EFT_FCLTNETINFO_RES;
	int head_size =0;
	int data_size =0;
	int send_size =0;

	site_id = fclt_id = 0;
	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}

	ptrradio = get_radio_withcode( g_fre_fclt_code );

	if ( ptrradio == NULL )
	{
		print_dbg( DBG_ERR,1, "Can not find Fclt:%d [%s:%d:%s]",g_fre_fclt_code,__FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memset( &net_req, 0, sizeof( net_req));
	memset( &net_resp, 0, sizeof( net_resp));
    memset( &net_info, 0, sizeof( net_info ));

	ptrhead = ( radio_head_t *)ptrdata;
	if ( ptrhead->ptrdata != NULL )
	{
		memcpy( &net_req, ptrhead->ptrdata, sizeof( net_req ));
	}

    site_id = get_my_siteid_shm();
    fclt_id = net_req.tfcltelem.nfcltid;
    //print_dbg( DBG_INFO,1, "######## Recv FCLT NET Info SiteID:%d, Fclt_ID :%d",site_id, fclt_id); 

    /* DB 조회 */
    load_fclt_net_ctrl_db(site_id, fclt_id, &net_info );
   
	/* Data 등록 */
	net_resp.tfcltelem.nsiteid 	= site_id;
	net_resp.tfcltelem.nfcltid	= fclt_id;
	net_resp.tfcltelem.efclt	= g_frs_fclt_code;

	net_resp.timet = time( NULL );
	memcpy( net_resp.sip, net_info.szip , MAX_IP_SIZE );
	net_resp.nport = net_info.port;
	
	/* Fclt Net  */
	memset( szsend_buf, 0, sizeof( szsend_buf ));
	ptrsend = (BYTE*)&szsend_buf[ 0 ];
	memset( &head, 0, sizeof( head ));
	head_size 	= ( sizeof( head ) - PNT_SIZE); 
	data_size 	= sizeof( net_resp );
	
	/* make head */
	head.protocol	= OP_FMS_PROT;
	head.opcode		= send_cmd;
	head.data_len	= data_size;
	head.direction 	= ESYSD_RTUEQP;

	/* head copy */
	memcpy( ptrsend, &head, head_size );
	ptrsend += head_size;
	send_size += head_size;

	/* data copy  */
	memcpy( ptrsend, &net_resp, data_size );
	send_size += data_size;

    print_dbg( DBG_INFO,1, "######## Send FCLT NET to FRS RADIO:: Select DB IP:%s, Port:%d, SiteID:%d, Fclt_ID :%d",
				net_info.szip, net_info.port, site_id, net_info.fcltid); 

	if( send_tcp_data( g_fre_fclt_code, ptrradio->tcp_fd,  send_cmd, send_size, szsend_buf ) == ERR_SUCCESS )
	{
		ptrradio->last_send_time = time( NULL );
	}

    return ERR_SUCCESS;

}

static int parse_fde_cmd( UINT16 cmd, void * ptrdata )
{
	int i,cnt = 0;
	proc_func tmp_parse = NULL;
	UINT16 l_cmd = 0;

	cnt = ( sizeof( g_parse_fde_func_list ) / sizeof( proc_func_t));

	for( i = 0 ; i < cnt ; i++ )
	{
		tmp_parse 	= g_parse_fde_func_list[ i ].ptrproc ;
		l_cmd 		= g_parse_fde_func_list[ i ].inter_msg; 

		if ( tmp_parse!= NULL && cmd == l_cmd )
		{
			tmp_parse( cmd, ptrdata );
		}
	}
	return ERR_SUCCESS;	
}

static radio_info_t * get_radio_withcode( int fcltcode )
{
	radio_info_t * ptrradio = NULL;
	int i,cnt 	= 0;
	int l_fcltcode = 0;

	cnt = g_radio_manager.cnt;

	for( i = 0 ; i< cnt ; i++ )
	{
		ptrradio = (radio_info_t*)&g_radio_manager.list[ i ];
		if ( ptrradio != NULL )
		{
			l_fcltcode = ptrradio->fclt_info.fcltcode;
			if( l_fcltcode == fcltcode )
			{
				return ptrradio;
			}
		}
	}

	return NULL;
}

static int valid_packet_radio( int size, radio_queue_t * ptrradio_queue, proc_func ptrparse_func, char * ptrname, radio_head_t * ptrhead   )
{
	int head =0;
	char * ptrqueue= NULL;
	short sval = 0;
	int val =0;
	int l_size = 0;
	int head_size = 0;

	head  = ptrradio_queue->head;
	head_size = ( sizeof( radio_head_t ) - PNT_SIZE) ;

	if ( head < MIN_TCP_QUEUE_SIZE )
	{
		//print_dbg( DBG_INFO,1,"Lack of SR data size( step 1 )" );
		return 0;
	}

	ptrqueue = (char*)&ptrradio_queue->ptrlist_buf[ 0 ] ;
	
	/* head :protocol */
	l_size = sizeof( short );
	memcpy( &sval, ptrqueue, l_size);
	ptrhead->protocol= sval ;
	ptrqueue += l_size;

	/* head : opcode */
	l_size = sizeof( short );
	memcpy( &sval, ptrqueue, l_size);
	ptrhead->opcode = sval;
	ptrqueue += l_size;

	/* head : size */
	l_size = sizeof( int );
	memcpy( &val, ptrqueue, l_size);
	ptrhead->data_len= val;
	ptrqueue += l_size;

	if ( ptrhead->data_len >= MAX_RADIO_DATA_SIZE )
	{
		print_dbg( DBG_ERR,1, "Overflow Body Size:%d, %s", ptrhead->data_len, ptrname );
		//ptrhead->data_len = MAX_TCP_RECV_SIZE;
		memset( ptrradio_queue->ptrlist_buf, 0, MAX_TCP_QUEUE_SIZE );
		ptrradio_queue->head = 0;
        return 0;
	}

	/* head : direction  */
	l_size = sizeof( short );
	memcpy( &sval, ptrqueue, l_size);
	ptrhead->direction = sval;
	ptrqueue += l_size;


	/* stan + spdf + data len + crc : 5byte */
	if ( head < ( ptrhead->data_len + head_size ) )
	{
		//print_dbg( DBG_INFO,1,"Lack of SR data size( step 2 )" );
		return 0;
	}
	
	if( ptrhead->ptrdata != NULL && ptrhead->data_len > 0 )
	{
		memset( ptrhead->ptrdata, 0, MAX_RADIO_DATA_SIZE );
		memcpy( ptrhead->ptrdata, ptrqueue, ptrhead->data_len );
		//print_buf(ptrhead->data_len ,ptrhead->ptrdata );
	}

	if ( ptrhead->protocol == OP_FMS_PROT )
	{
		return 1;
	}
	else
	{
		memset( ptrradio_queue->ptrlist_buf, 0, MAX_TCP_QUEUE_SIZE );
		ptrradio_queue->head = 0;
		print_dbg( DBG_ERR,1,"Invalid head protocol %s Packet data :%d", ptrname, ptrhead->protocol );
	}

	return 0;
}

static int parse_packet_radio( int size, radio_queue_t * ptrradio_queue, proc_func ptrparse_func, char * ptrname, radio_head_t * ptrhead )
{
	int head =0;
	int t_size, r_size = 0;
	char * ptrqueue= NULL;
	int head_size = 0;

	head        = ptrradio_queue->head;
	ptrqueue    = ( char *)&ptrradio_queue->ptrlist_buf[ 0 ] ;
	t_size = r_size =0;

	head_size = sizeof( radio_head_t ) - PNT_SIZE;

	/* head size*/
	t_size 	+= head_size;
	ptrqueue += head_size;
	
	/* data size */
	t_size   += ptrhead->data_len;
	ptrqueue += ptrhead->data_len ;

	if ( head < t_size )
	{
		memset( ptrradio_queue->ptrlist_buf, 0, MAX_TCP_QUEUE_SIZE );
		ptrradio_queue->head = 0;
		print_dbg( DBG_ERR, 1,"Invalid %s Packet data" , ptrname );
		return -1;
	}

	r_size = head - t_size;

	if ( r_size == 0 )
	{
		memset( ptrradio_queue->ptrlist_buf, 0, MAX_TCP_QUEUE_SIZE );
		ptrradio_queue->head = 0;
	}
	else
	{
		memmove( &ptrqueue[ 0 ], &ptrqueue[ t_size ], r_size );
		ptrradio_queue->head = r_size;
	}
	//print_dbg( DBG_INFO, 1," Cortex RESP ADDR:%04x", ptrsr_data->addr& 0x0000FFFF ); 
	
	if ( ptrparse_func != NULL )
	{
		ptrparse_func( ptrhead->opcode, ptrhead );
	}

	return r_size;
}

static int parse_radio_data( int size, radio_queue_t * ptrradio_queue, proc_func ptrparse_func, char * ptrname, radio_head_t * ptrhead   )
{
	int head = 0;
	char *ptrqueue = NULL;
	char *ptrrecv = NULL;
	int valid = 0;
	int remain_size = 0;

	if ( ptrradio_queue == NULL || ptrparse_func == NULL || ptrname == NULL || ptrhead == NULL )
	{
		print_dbg( DBG_ERR, 1, "Isnull prase data[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrqueue    = ptrradio_queue->ptrlist_buf;
	ptrrecv     = ptrradio_queue->ptrrecv_buf;

	head        = ptrradio_queue->head;

	if ( head + size >= MAX_TCP_QUEUE_SIZE )
	{
		print_dbg( DBG_ERR,1, "Overflow %s QUEUE BUF Head:%d, Size:%d",ptrname, head, size );
		memset( ptrqueue, 0, MAX_TCP_QUEUE_SIZE );
		ptrradio_queue->head = 0;

		return ERR_RETURN;
	}

	memcpy( &ptrqueue[ head ], ptrrecv , size );
	ptrradio_queue->head += size;

	valid = valid_packet_radio( size, ptrradio_queue, ptrparse_func, ptrname, ptrhead );

	while( valid )
	{
		remain_size = parse_packet_radio( size, ptrradio_queue, ptrparse_func, ptrname, ptrhead );

		if ( remain_size > 0 )
			valid = valid_packet_radio( remain_size, ptrradio_queue, ptrparse_func, ptrname, ptrhead  );
		else
			valid = 0;
	}

	return ERR_SUCCESS;
}

static int set_tcp_fd( int radio_id, int fcltcode )
{
	int s = -1, n;
	int	so_reuseaddr;
	int so_keepalive;
	struct linger so_linger;
	struct sockaddr_in serv_addr;
	radio_info_t * ptrradio = NULL;

	if ( radio_id < 0 || radio_id >= MAX_FCLT_CNT )
	{
		print_dbg( DBG_ERR, 1, "Invalid radio id:%d [%s:%d:%s]",radio_id,  __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrradio = (radio_info_t *)&g_radio_manager.list[ radio_id ];
	
	if( ptrradio == NULL )
	{
		print_dbg( DBG_ERR, 1, "Invalid radio info:%d [%s:%d:%s]",radio_id,  __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	s = socket( PF_INET, SOCK_STREAM, 0 );
	if( s < 0 )
	{
		print_dbg( DBG_ERR, 1, "Cannot Create TCP Socket:IP:%s,Port:%d", ptrradio->fclt_info.szip, ptrradio->fclt_info.port );
		return ERR_RETURN;
	}

	so_reuseaddr = 1;
	so_keepalive = 1;
	so_linger.l_onoff	= 1;
	so_linger.l_linger	= 0;
	setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr) );
	setsockopt( s, SOL_SOCKET, SO_KEEPALIVE, &so_keepalive, sizeof(so_keepalive) );
	setsockopt( s, SOL_SOCKET, SO_LINGER,	 &so_linger,	sizeof(so_linger) );

#if 0
	struct sockaddr_in my_addr;
	memset( &my_addr, 0, sizeof( my_addr ) );
	my_addr.sin_family		= AF_INET;
	my_addr.sin_addr.s_addr	= INADDR_ANY;
	my_addr.sin_port		= htons( ptrradio->fclt_info.port );
	if( (bind( s, (struct sockaddr *)&my_addr, sizeof(my_addr) )) < 0 )
	{
		close( s );
		print_dbg( DBG_ERR, 1, "Cannot Create TCP Socket BIND:IP:%s,Port:%d", ptrradio->fclt_info.szip, ptrradio->fclt_info.port );
		return ERR_RETURN;
	}
#endif

	memset( &serv_addr, 0, sizeof(serv_addr) );
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= inet_addr( ptrradio->fclt_info.szip );
	serv_addr.sin_port			= htons( ptrradio->fclt_info.port );

	/* connect timeout : 3sec */
	n = connect_with_timeout_poll( s, (struct sockaddr *)&serv_addr, sizeof( serv_addr ), TCP_TIMEOUT );
	if( n < 0 )
	{
		close(s);
		print_dbg( DBG_ERR, 1, "Cannot Connect EQP(%d) :IP(Timeout):%s,Port:%d", fcltcode, ptrradio->fclt_info.szip, ptrradio->fclt_info.port );
		sys_sleep( 2000 );
		return ERR_RETURN;
	}

	ptrradio->tcp_fd = s;
	print_dbg( DBG_SAVE, 1, "######## Success of TCP Client :IP:%s,Port:%d, fd:%d ", ptrradio->fclt_info.szip, ptrradio->fclt_info.port, ptrradio->tcp_fd );

	return ERR_SUCCESS;
}

static int set_tcp_connect( void )
{
	int i, radio_cnt = 0;
	int fd = 0;
	int fcltcode = 0;

	radio_cnt = g_radio_manager.cnt;
	
	for( i = 0 ; i< radio_cnt ;i++)
	{
		fd 			= g_radio_manager.list[ i ].tcp_fd;
		fcltcode 	= g_radio_manager.list[ i ].fclt_info.fcltcode ;

		if ( fcltcode > 0 && fd <=0 )
		{
			if ( set_tcp_fd( i, fcltcode ) < 0 )
            {
			    check_radio_connect_sts( g_radio_manager.list[ i ].fclt_info.fcltid, ESNSRCNNST_ERROR );
            }
			else
			{
				check_radio_connect_sts( g_radio_manager.list[ i ].fclt_info.fcltid, ESNSRCNNST_NORMAL );
			}
            sys_sleep( 100 );
		}
	}

	return ERR_SUCCESS;
}

static int close_all_tcp( void )
{
	int i, radio_cnt = 0;
	int fd = 0;

	radio_cnt = g_radio_manager.cnt;
	
	for( i = 0 ; i< radio_cnt ;i++)
	{
		fd = g_radio_manager.list[ i ].tcp_fd;

		if ( fd >0 )
		{
			close( fd );
            print_dbg( DBG_INFO,1,"###### FCLT TCP Close :%d", fd );
			g_radio_manager.list[ i ].tcp_fd = -1;
			check_radio_connect_sts( g_radio_manager.list[ i ].fclt_info.fcltid, ESNSRCNNST_ERROR );
		}
	}
	return ERR_SUCCESS;
}

static int close_radio_fd( int fcltcode , char * ptrname )
{
	radio_info_t * ptrradio = NULL;
	int i,cnt 	= 0;
	int l_fcltcode = 0;

	cnt = g_radio_manager.cnt;

	for( i = 0 ; i< cnt ; i++ )
	{
		ptrradio = (radio_info_t*)&g_radio_manager.list[ i ];
		if ( ptrradio != NULL )
		{
			l_fcltcode = ptrradio->fclt_info.fcltcode;
			if( l_fcltcode == fcltcode )
			{
				print_dbg(DBG_INFO,1,"Close %s Radio TCP FD:%d, FCLTCODE:%d", ptrname,ptrradio->tcp_fd , fcltcode );
				close( ptrradio->tcp_fd );
				ptrradio->tcp_fd = -1;
				ptrradio->alive_ack = 0;

				check_radio_connect_sts( ptrradio->fclt_info.fcltid, ESNSRCNNST_ERROR );
				return ERR_SUCCESS;;
			}
		}
	}

	return ERR_SUCCESS;
}

static int send_tcp_data( int fcltcode, int fd, short cmd, int len, BYTE * ptrdata )
{
	int ret = ERR_SUCCESS;
	int size = 0;
	BYTE *ptrtemp = NULL;

	if( fd < 0 || ptrdata == NULL || len <= 0 )
	{
		//print_dbg( DBG_ERR, 1, "Invalid TCP Data fcltcode:%d", fcltcode );
		return ERR_RETURN;
	}

	ptrtemp 	= ptrdata;
	size 		= len;

	ret = write( fd, ptrtemp, size );
	if( ret < 0 )
	{
		print_dbg( DBG_ERR, 1, "Can not Send TCP Data fcltcode:%d, fd:%d, size:%d, ret:%d", fcltcode , fd, len, ret);
		close_radio_fd( fcltcode , "WRITE" );
		return ERR_RETURN;
	}

	while( ret < size )
	{
		//printf("send_tcp_data... size = %d, send size = %d\n", size, ret );
		ret += write( fd, (ptrtemp + ret), size - ret );
	}

	return	 ERR_SUCCESS;
}

static int read_fre( radio_head_t * ptrhead )
{
	int size ;
	int fd;
	char * ptrrecv= NULL;
	radio_info_t * ptrradio = NULL;

	ptrradio = get_radio_withcode( g_fre_fclt_code );
	if( ptrradio == NULL )
	{
		//print_dbg( DBG_ERR, 1, "Cannot read fd fcltid:%s", FRE_NAME );
		sys_sleep( 2000 );
		return ERR_RETURN;
	}
	
	fd = ptrradio->tcp_fd;

	if ( fd > 0 )
	{
		ptrrecv = ptrradio->queue.ptrrecv_buf;
        
		if ( ptrrecv != NULL )
		{
			size =0;
			memset( ptrrecv, 0, MAX_TCP_RECV_SIZE );
			size = read( fd , ptrrecv, MAX_TCP_RECV_SIZE  );

			if ( size > 0 )
			{
                 memset( ptrhead->ptrdata, 0, MAX_RADIO_DATA_SIZE );
                 //print_buf( size, (BYTE*)ptrrecv );
                 parse_radio_data( size, &ptrradio->queue, parse_fre_cmd, FRE_NAME, ptrhead );
                 //g_last_read_time = time( NULL );
			}
			else
			{
                //release
				printf("------------------------------------tcp close--------------------------\r\n");
            }

			//print_buf(size, (BYTE*)ptrrecv );
			//print_dbg( DBG_INFO,1,"size:%d, %s", size, ptrrecv);
		}
		else
		{
			print_dbg( DBG_ERR, 1, "Not have FRE recv buffer");
		}
	}
	else
	{
		sys_sleep( 2000 );
	}

	return size ;
}

static int read_sre( radio_head_t * ptrhead )
{
	int size ;
	int fd;
	char * ptrrecv= NULL;
	radio_info_t * ptrradio = NULL;

	ptrradio = get_radio_withcode( g_sre_fclt_code );
	if( ptrradio == NULL )
	{
		//print_dbg( DBG_ERR, 1, "Cannot read fd fcltid:%s", SRE_NAME );
		sys_sleep( 2000 );
		return ERR_RETURN;
	}
	
	fd = ptrradio->tcp_fd;

	if ( fd > 0 )
	{
		ptrrecv = ptrradio->queue.ptrrecv_buf;

		if ( ptrrecv != NULL )
		{
			size =0;
			memset( ptrrecv, 0, MAX_TCP_RECV_SIZE );
			size = read( fd , ptrrecv, MAX_TCP_RECV_SIZE  );
			
			//printf( ".........................SRE RECV SIZE:%d........................\r\n", size );
			if ( size > 0 )
			{
				memset( ptrhead->ptrdata, 0, MAX_RADIO_DATA_SIZE );
				//print_buf( size, (BYTE*)ptrrecv );
				parse_radio_data( size, &ptrradio->queue, parse_sre_cmd, SRE_NAME, ptrhead );
				//g_last_read_time = time( NULL );
			}
			else
			{
				//close_radio_fd( ptrradio->fclt_info.fcltcode, SRE_NAME );
			}

			//print_buf(size, (BYTE*)ptrrecv );
			//print_dbg( DBG_INFO,1,"size:%d, %s", size, ptrrecv);
		}
		else
		{
			print_dbg( DBG_ERR, 1, "Not have SRE recv buffer");
		}
	}
	else
	{
		sys_sleep( 2000 );
	}

	return size ;
}

static int read_fde( radio_head_t * ptrhead )
{

	int size ;
	int fd;
	char * ptrrecv= NULL;
	radio_info_t * ptrradio = NULL;

	ptrradio = get_radio_withcode( g_fde_fclt_code );
	if( ptrradio == NULL )
	{
		//print_dbg( DBG_ERR, 1, "Cannot read fd fcltid:%s", FDE_NAME );
		sys_sleep( 2000 );
		return ERR_RETURN;
	}
	
	fd = ptrradio->tcp_fd;

	if ( fd > 0 )
	{
		ptrrecv = ptrradio->queue.ptrrecv_buf;

		if ( ptrrecv != NULL )
		{
			size =0;
			memset( ptrrecv, 0, MAX_TCP_RECV_SIZE );
			size = read( fd , ptrrecv, MAX_TCP_RECV_SIZE  );

			if ( size > 0 )
			{
				memset( ptrhead->ptrdata, 0, MAX_RADIO_DATA_SIZE );
				//print_buf( size, (BYTE*)ptrrecv );
				parse_radio_data( size, &ptrradio->queue, parse_fde_cmd, FDE_NAME, ptrhead );
				//g_last_read_time = time( NULL );
			}
			else
			{
				//close_radio_fd( ptrradio->fclt_info.fcltcode, FDE_NAME );
			}

			//print_buf(size, (BYTE*)ptrrecv );
			//print_dbg( DBG_INFO,1,"size:%d, %s", size, ptrrecv);
		}
		else
		{
			print_dbg( DBG_ERR, 1, "Not have FDE recv buffer");
		}
	}

	return size;
}	

static void *  fre_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;
	radio_head_t r_head;
	g_end_fre_pthread = 0;
    fd_set fre_set;
    int fd ;
    int max_fd = -1;
	radio_info_t * ptrradio = NULL;
    struct timeval tv;
    int n;
    int ret = 0;

    r_head.ptrdata = NULL;
	r_head.ptrdata = ( BYTE *)malloc( sizeof( BYTE ) * MAX_TCP_QUEUE_SIZE );

	if( r_head.ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "Can not make head buf :%s", FRE_NAME );
	}

	while( g_release == 0 && g_start_fre_pthread == 1 )
	{
        ptrradio = NULL;
        ptrradio = get_radio_withcode( g_fre_fclt_code );

        if( ptrradio != NULL && ptrradio->tcp_fd > 0 )
        {
            fd = ptrradio->tcp_fd;
            FD_ZERO(&fre_set);
            FD_SET(fd , &fre_set);
            max_fd = fd ;
            tv.tv_sec  = 1;	/* 1sec */
            tv.tv_usec = 0;

            n = select( max_fd + 1 , &fre_set, NULL, NULL , &tv );
            
            if (n == -1)/*select 함수 오류 발생 */
            { 
                print_dbg( DBG_ERR, 1, "select function error [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
                sys_sleep( 5000 );
            }
            else if (n == 0) /* time-out에 의한 리턴 */
            { 
                //print_dbg( DBG_NONE, 1,"===>> 시간이 초과 되었습니다 : sock %d", g_sock);
            }
            else /* 파일 디스크립터 변화에 의한 리턴 */
            { 
                //print_dbg( DBG_NONE, 1,"===>> fms read event : sock %d", g_sock);
                if (FD_ISSET(fd, &fre_set))
                {
                    if ( r_head.ptrdata != NULL )
                    {
                        ret = read_fre( &r_head );
                        if (ret  > 0 )
                        {
                            //FD_ZERO(&fre_set);
                            //FD_SET(fd, &fre_set);
                        }
                        else
                        {
                            FD_CLR(fd, &fre_set);
                            close_radio_fd( ptrradio->fclt_info.fcltcode, FRE_NAME );
                        }
                    }
                }
            }
        }
        else
        {
            sys_sleep( 1000 );
        }
    }

    if ( r_head.ptrdata != NULL )
    {
        free( r_head.ptrdata );
        r_head.ptrdata = NULL;
    }

	if( g_end_fre_pthread == 0 )
	{
		pthread_join( fre_pthread , ( void **)&status );
		g_start_fre_pthread 	= 0;
		g_end_fre_pthread 		= 1;
	}

	return ERR_SUCCESS;
}

static void *  sre_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;
	radio_head_t r_head;
	g_end_sre_pthread = 0;
    fd_set sre_set;
    int fd ;
    int max_fd = -1;
	radio_info_t * ptrradio = NULL;
    struct timeval tv;
    int n;
    int ret = 0;

    r_head.ptrdata = NULL;
	r_head.ptrdata = ( BYTE *)malloc( sizeof( BYTE ) * MAX_TCP_QUEUE_SIZE );

	if( r_head.ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "Can not make head buf :%s", SRE_NAME );
	}

	while( g_release == 0 && g_start_sre_pthread == 1 )
	{
        ptrradio = NULL;
        ptrradio = get_radio_withcode( g_sre_fclt_code );

        if( ptrradio != NULL && ptrradio->tcp_fd > 0 )
        {
            fd = ptrradio->tcp_fd;
            FD_ZERO(&sre_set);
            FD_SET(fd , &sre_set);
            max_fd = fd ;
            tv.tv_sec  = 1;	/* 1sec */
            tv.tv_usec = 0;

            n = select( max_fd + 1 , &sre_set, NULL, NULL , &tv );
            
            if (n == -1)/*select 함수 오류 발생 */
            { 
                print_dbg( DBG_ERR, 1, "select function error [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
                sys_sleep( 5000 );
            }
            else if (n == 0) /* time-out에 의한 리턴 */
            { 
                //print_dbg( DBG_NONE, 1,"===>> 시간이 초과 되었습니다 : sock %d", g_sock);
            }
            else /* 파일 디스크립터 변화에 의한 리턴 */
            { 
                //print_dbg( DBG_NONE, 1,"===>> fms read event : sock %d", g_sock);
                if (FD_ISSET(fd, &sre_set))
                {
                    if ( r_head.ptrdata != NULL )
                    {
                        ret = read_sre( &r_head );
                        if (ret  > 0 )
                        {
                            //FD_ZERO(&sre_set);
                            //FD_SET(fd, &sre_set);
                        }
                        else
                        {
                            FD_CLR(fd, &sre_set);
                            close_radio_fd( ptrradio->fclt_info.fcltcode, SRE_NAME );
                        }
                    }
                }
            }
        }
        else
        {
            sys_sleep( 1000 );
        }
    }

    if ( r_head.ptrdata != NULL )
    {
        free( r_head.ptrdata );
        r_head.ptrdata = NULL;
    }

	if( g_end_sre_pthread == 0 )
	{
		pthread_join( sre_pthread , ( void **)&status );
		g_start_sre_pthread 	= 0;
		g_end_sre_pthread 		= 1;
	}

	return ERR_SUCCESS;
}

static void *  fde_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;
	radio_head_t r_head;
	g_end_fde_pthread = 0;
    fd_set fde_set;
    int fd ;
    int max_fd = -1;
	radio_info_t * ptrradio = NULL;
    struct timeval tv;
    int n;
    int ret = 0;

    r_head.ptrdata = NULL;
	r_head.ptrdata = ( BYTE *)malloc( sizeof( BYTE ) * MAX_TCP_QUEUE_SIZE );

	if( r_head.ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "Can not make head buf :%s", FDE_NAME );
	}

	while( g_release == 0 && g_start_fde_pthread == 1 )
	{
        ptrradio = NULL;
        ptrradio = get_radio_withcode( g_fde_fclt_code );

        if( ptrradio != NULL && ptrradio->tcp_fd > 0 )
        {
            fd = ptrradio->tcp_fd;
            FD_ZERO(&fde_set);
            FD_SET(fd , &fde_set);
            max_fd = fd ;
            tv.tv_sec  = 1;	/* 1sec */
            tv.tv_usec = 0;

            n = select( max_fd + 1 , &fde_set, NULL, NULL , &tv );
            
            if (n == -1)/*select 함수 오류 발생 */
            { 
                print_dbg( DBG_ERR, 1, "select function error [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
                sys_sleep( 5000 );
            }
            else if (n == 0) /* time-out에 의한 리턴 */
            { 
                //print_dbg( DBG_NONE, 1,"===>> 시간이 초과 되었습니다 : sock %d", g_sock);
            }
            else /* 파일 디스크립터 변화에 의한 리턴 */
            { 
                //print_dbg( DBG_NONE, 1,"===>> fms read event : sock %d", g_sock);
                if (FD_ISSET(fd, &fde_set))
                {
                    if ( r_head.ptrdata != NULL )
                    {
                        ret = read_fde( &r_head );
                        if (ret  > 0 )
                        {
                            //FD_ZERO(&fde_set);
                            //FD_SET(fd, &fde_set);
                        }
                        else
                        {
                            FD_CLR(fd, &fde_set);
                            close_radio_fd( ptrradio->fclt_info.fcltcode,FDE_NAME );
                        }
                    }
                }
            }
        }
        else
        {
            sys_sleep( 1000 );
        }
    }
	if ( r_head.ptrdata != NULL )
	{
		free( r_head.ptrdata );
		r_head.ptrdata = NULL;
	}

	if( g_end_fde_pthread == 0 )
	{
		pthread_join( fde_pthread , ( void **)&status );
		g_start_fde_pthread 	= 0;
		g_end_fde_pthread 		= 1;
	}

	return ERR_SUCCESS;
}

static int create_radio_thread( void )
{
	int ret = ERR_SUCCESS;

	/* Fixed RMS EQP */
	if ( g_release == 0 && g_start_fre_pthread == 0 && g_end_fre_pthread == 1 )
	{
		g_start_fre_pthread = 1;

		if ( pthread_create( &fre_pthread, NULL, fre_pthread_idle , NULL ) < 0 )
		{
			g_start_fre_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create FRE Thread" );
			ret = ERR_RETURN;
		}
		else
		{
			//print_dbg( DBG_INIT, 1, "mini-SEED Thread Create OK. ");
		}
	}
	
	if( ret == ERR_RETURN ) return ERR_RETURN;

	/* Semi RMS EQP */
	if ( g_release == 0 && g_start_sre_pthread == 0 && g_end_sre_pthread == 1 )
	{
		g_start_sre_pthread = 1;

		if ( pthread_create( &sre_pthread, NULL, sre_pthread_idle , NULL ) < 0 )
		{
			g_start_sre_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create SRE Thread" );
			ret = ERR_RETURN;
		}
		else
		{
			//print_dbg( DBG_INIT, 1, "mini-SEED Thread Create OK. ");
		}
	}
	if( ret == ERR_RETURN ) return ERR_RETURN;

	/* Fixed DFS EQP */
	if ( g_release == 0 && g_start_fde_pthread == 0 && g_end_fde_pthread == 1 )
	{
		g_start_fde_pthread = 1;

		if ( pthread_create( &fde_pthread, NULL, fde_pthread_idle , NULL ) < 0 )
		{
			g_start_fde_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create FDE Thread" );
			ret = ERR_RETURN;
		}
		else
		{
			//print_dbg( DBG_INIT, 1, "mini-SEED Thread Create OK. ");
		}
	}

	return ret;
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
static int internal_recv_ctrl_data( unsigned short inter_msg, void * ptrdata )
{
	int i;
	int proc_func_cnt = 0;
	
	proc_func  ptrtemp_proc_func = NULL;
	UINT16 list_msg = 0;

	if( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
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

static int init_queue( void )
{
	int i,cnt;
	radio_info_t * ptrradio = NULL;
	int er1, er2 = 0;

	cnt = g_radio_manager.cnt;

	for( i = 0 ; i < cnt; i++ )
	{
		ptrradio = 	(radio_info_t*)&g_radio_manager.list[ i ];
		er1 = er2 = 0;

		if ( ptrradio != NULL )
		{
			ptrradio->queue.ptrlist_buf = ( char *)malloc( sizeof( BYTE ) * MAX_TCP_QUEUE_SIZE );
			ptrradio->queue.ptrrecv_buf = ( char *)malloc( sizeof( BYTE ) * MAX_TCP_RECV_SIZE );	
			
			if( ptrradio->queue.ptrlist_buf == NULL )
			{
				print_dbg( DBG_ERR, 1, "Can not Create List Buffer");	
				er1 = 1;
			}

			if( ptrradio->queue.ptrrecv_buf == NULL )
			{
				print_dbg( DBG_ERR, 1, "Can not Create Recv Buffer");	
				er2 = 1;
			}
			
			if ( er1 == 0 )
			{
				memset( ptrradio->queue.ptrlist_buf, 0, MAX_TCP_QUEUE_SIZE );
			}
			if ( er2 == 0 )
			{
				memset( ptrradio->queue.ptrrecv_buf, 0, MAX_TCP_RECV_SIZE );
			}
		}
	}

	return ERR_SUCCESS;
}

/* radio module 초기화 내부 작업 */
static int internal_init( void )
{
	int ret = ERR_SUCCESS;

	/* warring을 없애기 위한 함수 호출 */
	send_data_control( 0, NULL );

	ret = get_radio_info( );
	
	if ( ret == ERR_SUCCESS )
	{
		ret = init_queue();
	}

	if ( ret == ERR_SUCCESS )
	{
		//ret = set_tcp_connect();
	}

	if ( ret == ERR_SUCCESS )
	{
		ret = create_radio_thread();
	}

	if ( ret == ERR_SUCCESS )
	{
		print_dbg(DBG_INFO, 1, "Success of RADIO Module Initialize");
	}

	return ret;
}

/* radio module loop idle 내부 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */

static int check_fre_req_time( time_t cur_time )
{
	int ret = 0;

	/* FRE 요청 */
	ret= send_fre_eqpdata_req( g_fre_fclt_code );
	/* FRS 요청 */
	//sys_sleep( 700 );
	//ret= send_fre_eqpdata_req( g_frs_fclt_code );
	
	return ret;
}

static int check_sre_req_time( time_t cur_time )
{
	static volatile time_t pre_check_sre_time ;
	static volatile time_t sre_first = 0;
	time_t inter_time;
	int ret = 0;

	if ( pre_check_sre_time == 0 )
		pre_check_sre_time = cur_time;

	inter_time = ( cur_time - pre_check_sre_time );

	/* 60초에 한번 데이터 요청  확인 */
	if ( sre_first == 0 || inter_time >= 60 )
	{
		ret= send_sre_eqpdata_req();

		/* alive 처리를 하지 않은 경우 재 요청처리 한다. */
		if ( ret != -2 )
		{
			sre_first =1;
			pre_check_sre_time = cur_time;
		}
	}
	
	return ERR_SUCCESS;
}

static int check_fde_req_time( time_t cur_time )
{
	static volatile time_t pre_check_fde_time ;
	static volatile time_t fde_first = 0;
	time_t inter_time;
	int ret = 0;

	if ( pre_check_fde_time == 0 )
		pre_check_fde_time = cur_time;

	inter_time = ( cur_time - pre_check_fde_time );

	/* 60초에 한번 데이터 요청  확인 */
	if ( fde_first == 0 || inter_time >= 60 )
	{
		ret= send_fde_eqpdata_req();

		/* alive 처리를 하지 않은 경우 재 요청처리 한다. */
		if ( ret != -2 )
		{
			fde_first =1;
			pre_check_fde_time = cur_time;
		}
	}
	
	return ERR_SUCCESS;
}

static int check_tcp_connect( time_t cur_time )
{
	static volatile time_t pre_check_tcp_time ;
	static volatile int first = 0;
	time_t inter_time;

	if ( pre_check_tcp_time == 0 )
		pre_check_tcp_time = cur_time;

	inter_time = ( cur_time - pre_check_tcp_time );
	
	/* 5초에 한번 tcp 연결 확인 */
	if ( first == 0 || inter_time >= 3 )
	{
		first =1;
		set_tcp_connect();
		pre_check_tcp_time = cur_time;
	}
	
	return ERR_SUCCESS;
}

static int internal_idle( void * ptr )
{
	time_t cur_time;
	radio_info_t * ptrradio = NULL;

	( void )ptr;

	cur_time = time( NULL );
	
	/* 전원 제어 중일 때는 상태 요청 하지 않는다 */
	ptrradio = get_radio_withcode( g_fre_fclt_code );
    if ( ptrradio != NULL )
    {
        check_fre_req_time( cur_time );
    }

    ptrradio = get_radio_withcode( g_sre_fclt_code );
    if ( ptrradio != NULL )
    {
        check_sre_req_time( cur_time );
    }

    ptrradio = get_radio_withcode( g_fde_fclt_code );
    if ( ptrradio != NULL )
    {
        check_fde_req_time( cur_time );
    }

	return ERR_SUCCESS;
}

static int internal_idle_etc( void )
{
	time_t cur_time;

	cur_time = time( NULL );

	check_tcp_connect( cur_time );
	create_radio_thread();

	return ERR_SUCCESS;
}

/* radio module의 release 내부 작업 */
static int internal_release( void )
{
	int i, cnt ;
	radio_info_t * ptrradio = NULL;

	close_all_tcp();

	cnt = g_radio_manager.cnt;

	for( i = 0 ; i < cnt; i++ )
	{
		ptrradio = 	(radio_info_t*)&g_radio_manager.list[ i ];

		if ( ptrradio != NULL )
		{
			if ( ptrradio->queue.ptrlist_buf != NULL )
			{
				free( ptrradio->queue.ptrlist_buf );
				ptrradio->queue.ptrlist_buf = NULL;
			}

			if ( ptrradio->queue.ptrrecv_buf != NULL )
			{
				free( ptrradio->queue.ptrrecv_buf );
				ptrradio->queue.ptrrecv_buf = NULL;
			}
		}
	}

	print_dbg(DBG_INFO, 1, "Release of RADIO Module");
	return ERR_SUCCESS;
}

/*================================================================================================
외부  함수  정의 
================================================================================================*/
int init_radio( send_data_func ptrsend_data )
{
	/* CONTROL module의 함수 포인트 등록 */
	g_ptrsend_data_control_func = ptrsend_data ;	/* CONTROL module로 센서 , 장애  데이터를 보낼 수 있는 함수 포인터 */

	/* 기타 초기화 작업 */
	internal_init();

	return ERR_SUCCESS;
}

/* radio module loop idle 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
int idle_radio( void * ptr )
{
	return internal_idle( ptr );
}

int idle_radio_etc( void )
{
	return internal_idle_etc();
}
/* 동적 메모리, fd 등 resouce release함수 */
int release_radio( void )
{
	/* release 작업  */
	return internal_release(); 
}

/* CONTROL Module로 부터 제어, 설정, update 정보를 받는 외부 함수 */
int recv_ctrl_data_radio( unsigned short inter_msg, void * ptrdata )
{
	return internal_recv_ctrl_data( inter_msg, ptrdata );
}
