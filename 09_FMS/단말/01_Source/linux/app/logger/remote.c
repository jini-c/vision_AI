/*********************************************************************
	file : remote.c
	description :
	product by tory45( tory45@empal.com) ( 2016.08.05 )
	update by ?? ( . . . )
	
	- Control에서 fms_server로 보낼 데이터를 받아 fms_server로 보내는 기능
	- fms_server에서 데이터를 받아 Control로 정보를 보내는 기능 
 ********************************************************************/
#include "com_include.h"
#include "sem.h"
#include "com_sns.h"
#include "control.h"

#include "remote.h"

#define REMOTE_COM		0x00
#define REMOTE_CTRL		0x05
#define REMOTE_STS		0x06
/*================================================================================================
 전역 변수 
 1.static 적용 
 2.숫자형 데이터 일경우 volatile 적용 
 3.초기값 적용.
================================================================================================*/
static volatile int g_remote_sem_key 						= REMOTE_SEM_KEY;
static volatile int g_remote_sem_id 						= 0;

static send_data_func  g_ptrsend_remote_data_control_func = NULL;		/* CONTROL 모듈  센서 데이터 ,  알람  정보를 넘기는 함수 포인트 */
static sr_queue_t g_remote_queue;
static BYTE g_bit_val[ MAX_BIT_CNT ];
static send_sr_list_t g_send_sr_list;
static volatile int g_remote_sr_timeout		= 0;
static volatile int g_clientid				= 0;

/* thread ( remote )*/
static volatile int g_start_remote_pthread 	= 0; 
static volatile int g_end_remote_pthread 		= 1; 
static pthread_t remote_pthread;
static void * remote_pthread_idle( void * ptrdata );
static volatile int g_release				= 0;

static char * g_ptrread_sr					= NULL;
static char * g_ptrcom_sr					= NULL;
static char * g_ptrsts_sr					= NULL;
static char * g_ptrctrl_sr					= NULL;

/* serial */
static int set_remote_fd( void );
static int release_remote_fd( void );
static volatile int g_remote_fd 				= -1;

/*================================================================================================
  내부 함수 선언
  static 적용 
================================================================================================*/
static int send_remote_data_control( UINT16 inter_msg, void * ptrdata );

static int internal_init( void );
static int internal_idle( void * ptr );
static int internal_release( void );
static int create_remote_thread( void );

static int release_buf( void );
static int write_remote( int len, char * ptrdata , BYTE cmd);

/* Proc 함수 [ typedef int ( * proc_func )( unsigned short , void * ); ] */ 
//static int rtusvr_sns_data_res( unsigned short inter_msg, void * ptrdata );
/*================================================================================================
  Proc Function Point 구조체
 	Messsage ID 	: 내부 Message ID
	Proc Function 	: Message ID 처리 함수 
	Control에서 받은 데이터를 처리할 함수를 아래의 형식으로 등록하시면 됩니다. 
	typedef int ( * proc_func )( unsigned short , void * );
	이해가되시면 주석은 삭제하셔도 됩니다. 
================================================================================================*/

static int parse_remote_comm_packet( BYTE cmd, void * ptrdata );
static int parse_remote_ctrl_packet( BYTE cmd, void * ptrdata );
static int parse_remote_sts_packet( BYTE cmd, void * ptrdata );

static int make_remote_packet( sr_data_t * ptrsr, BYTE *ptrsend );

static int proc_remote_com_req( UINT16 msg, void * ptrdata );
static int proc_remote_status_req( UINT16 msg, void * ptrdata );
static int proc_remote_ctrl_req( UINT16 msg, void * ptrdata );

static proc_func_t g_proc_remote_func_list []= 
{
	/* inter msg id   		,  			proc function point */
	/*=============================================*/
	{ EFT_SNSRCTL_POWER, proc_remote_ctrl_req },

	{ 0, NULL }
};

static parse_cmd_func_t g_parse_remote_func_list[ ] = 
{
	{ REMOTE_COM, 	parse_remote_comm_packet },
	{ REMOTE_CTRL, 	parse_remote_ctrl_packet },
	{ REMOTE_STS, 	parse_remote_sts_packet },
	{ 0xFF	, 	NULL }
};

static int check_remote_sr_timeout( void )
{
	int ret = ERR_SUCCESS;
	int resend_pos = -1;
	char * ptrdata = NULL;
	BYTE cmd;
	int len = 0;
	result_t result;
	BYTE result_data[ 2 ];


	lock_sem( g_remote_sem_id, g_remote_sem_key );
	ret = chk_send_list_timeout_com_sns( &g_send_sr_list , &resend_pos, DEV_ID_REMOTE );
	unlock_sem( g_remote_sem_id, g_remote_sem_key );

	if ( ret == RET_TIMEOUT )
	{
		memset(& result_data, 0, sizeof(result_data ));
		memset(& result, 0, sizeof(result ));

		set_remote_trans_sts_ctrl( ESNSRCNNST_ERROR );

		result.clientid 	= g_clientid;
		result.sts			= ESNSRCNNST_ERROR;

		set_remote_onoff_result_ctrl(&result, result_data );

	}
	else if( ret == RET_RESEND )
	{

		if( resend_pos >=0 )
		{
			cmd 	= g_send_sr_list.send_data[ resend_pos ].cmd;
			len		= g_send_sr_list.send_data[ resend_pos ].len;
			ptrdata = g_send_sr_list.send_data[ resend_pos ].ptrdata;

			write_remote( len, ptrdata, cmd );

			print_dbg( DBG_INFO, 1, "Resend REMOTE SR Data cmd:%d, len:%d", cmd, len );

		}
	}
	return ERR_SUCCESS;
}

static int check_remote_sr_restore_timeout( BYTE cmd )
{
	int ret = 0;

	lock_sem( g_remote_sem_id, g_remote_sem_key );
	ret = del_send_list_com_sns( cmd, &g_send_sr_list, DEV_ID_REMOTE );
	unlock_sem( g_remote_sem_id, g_remote_sem_key );
		
	/* 0: 2초이내 응답이 온 경우, 1: 응답은왔으나 2초 이내가 아닐 경우 */
	if ( ret == 0 )
	{
		set_remote_trans_sts_ctrl( ESNSRCNNST_NORMAL );
	}

	return ERR_SUCCESS;
}

static int proc_remote_com_req( UINT16 msg, void * ptrdata )
{
	sr_data_t sr_data;
	static BYTE com_send_buf[ MAX_SR_DEF_DATA_SIZE ];
	
	memset( &sr_data, 0, sizeof( sr_data ));
	memset(&com_send_buf, 0, sizeof( com_send_buf ));

	if ( g_ptrcom_sr != NULL )
	{
		sr_data.ptrdata 	= g_ptrcom_sr;
		memset( sr_data.ptrdata , 0,  MAX_SR_DEF_DATA_SIZE );
	}

	sr_data.len 		= 0x00;
	sr_data.cmd 		= REMOTE_COM;
	sr_data.dev_id		= 0x00;
	sr_data.dev_addr	= 0x00;
	sr_data.dev_ch		= 0x00;
	sr_data.mode		= 0x00;
	sr_data.resp_time	= 0x00;

	return make_remote_packet( &sr_data, com_send_buf );
}

static int proc_remote_status_req( UINT16 msg, void * ptrdata )
{
	sr_data_t sr_data;
	static BYTE sts_send_buf[ MAX_SR_DEF_DATA_SIZE ];
	
	memset( &sr_data, 0, sizeof( sr_data ));
	memset(&sts_send_buf, 0, sizeof( sts_send_buf ));

	if ( g_ptrsts_sr != NULL )
	{
		sr_data.ptrdata 	= g_ptrsts_sr;
		memset( sr_data.ptrdata , 0,  MAX_SR_DEF_DATA_SIZE );
	}

	/* data size가 0일 경우 ctrl이 아니라 status 요청으로 처리 됨 */
	sr_data.len 		= 0x00;
	sr_data.cmd 		= REMOTE_STS;
	sr_data.dev_id		= DEV_ID_REMOTE;
	sr_data.dev_addr	= 0x00;
	sr_data.dev_ch		= 0x00;
	sr_data.mode		= 0x00;
	sr_data.resp_time	= 0x00;

	return make_remote_packet( &sr_data, sts_send_buf );
}

static int proc_remote_ctrl_req( UINT16 msg, void * ptrdata )
{
	sr_data_t sr_data;
	static BYTE ctrl_send_buf[ MAX_SR_DEF_DATA_SIZE ];
	remote_data_t remote_data;

	memset( &sr_data, 0, sizeof( sr_data ));
	memset(&ctrl_send_buf, 0, sizeof( ctrl_send_buf ));
	memset(&remote_data, 0, sizeof(remote_data ));

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}

	if ( g_ptrctrl_sr != NULL )
	{
		sr_data.ptrdata 	= g_ptrctrl_sr;
		memset( sr_data.ptrdata , 0,  MAX_SR_DEF_DATA_SIZE );
	}

	memcpy(&remote_data, ptrdata, sizeof( remote_data ));

	sr_data.len 		= 0x01;
	sr_data.cmd 		= REMOTE_CTRL;
	sr_data.dev_id		= DEV_ID_REMOTE;
	sr_data.dev_addr	= 0x00;
	sr_data.dev_ch		= 0x00;
	sr_data.mode		= 0x00;
	sr_data.resp_time	= 0x00;
	sr_data.ptrdata[ 0 ] = remote_data.remote_val;

	//print_dbg( DBG_INFO,1,"RECV Remote ONOFF  :%x", sr_data.ptrdata[0] );
	return make_remote_packet( &sr_data, ctrl_send_buf );
}

static int parse_remote_comm_packet( BYTE cmd, void * ptrdata )
{
	
	sr_data_t * ptrsr = NULL;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrsr = (sr_data_t *)ptrdata;

	//print_dbg(DBG_NONE, 1,"Recv REMOTE COMMON Packet");

	check_remote_sr_restore_timeout(ptrsr->cmd );

#if 0
	print_dbg(DBG_NONE,1,"len:%x, cmd:%x, dev_id:%x, dev_addr:%x, dev_ch:%x, mode:%x, req_time:%x, check_sum:%x",
			  ptrsr->len, ptrsr->cmd, ptrsr->dev_id, ptrsr->dev_addr, ptrsr->dev_ch, ptrsr->mode, ptrsr->resp_time , ptrsr->check_sum );
#endif
	
	set_remote_ver_ctrl( (BYTE *)ptrsr->ptrdata );

	return ERR_SUCCESS;
}

static int parse_remote_sts_packet( BYTE cmd, void * ptrdata )
{
	sr_data_t * ptrsr = NULL;
	BYTE val = 0;
	remote_data_t remote_data;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrsr = (sr_data_t *)ptrdata;

	//print_dbg(DBG_NONE, 1,"Recv REMOTE Status Packet");

	check_remote_sr_restore_timeout(ptrsr->cmd );
#if 0
	print_dbg(DBG_NONE,1,"len:%x, cmd:%x, dev_id:%x, dev_addr:%x, dev_ch:%x, mode:%x, req_time:%x, check_sum:%x",
			  ptrsr->len, ptrsr->cmd, ptrsr->dev_id, ptrsr->dev_addr, ptrsr->dev_ch, ptrsr->mode, ptrsr->resp_time , ptrsr->check_sum );
#endif
	memset(&remote_data, 0, sizeof( remote_data ));
	val = ptrsr->ptrdata[ 0 ];
	remote_data.remote_val = val;

	set_remote_data_ctrl(&remote_data );

	return ERR_SUCCESS;
}

static int parse_remote_ctrl_packet( BYTE cmd, void * ptrdata )
{
	sr_data_t * ptrsr = NULL;
	result_t result;
	BYTE result_data[2 ];

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrsr = (sr_data_t *)ptrdata;

	//print_dbg(DBG_NONE, 1,"Recv REMOTE Ctrl Packet");

	memset(result_data, 0,sizeof( result_data));
	memset(&result, 0,sizeof( result ));


	check_remote_sr_restore_timeout(ptrsr->cmd );
#if 0
	print_dbg(DBG_NONE,1,"len:%x, cmd:%x, dev_id:%x, dev_addr:%x, dev_ch:%x, mode:%x, req_time:%x, check_sum:%x",
			  ptrsr->len, ptrsr->cmd, ptrsr->dev_id, ptrsr->dev_addr, ptrsr->dev_ch, ptrsr->mode, ptrsr->resp_time , ptrsr->check_sum );
#endif

	result_data[ 0 ] 	= ptrsr->ptrdata[ 0 ];
	result.clientid 	= g_clientid;
	result.sts			= ESNSRCNNST_NORMAL;

	set_remote_onoff_result_ctrl(&result, result_data );

	return ERR_SUCCESS;
}

static int make_remote_packet( sr_data_t * ptrsr, BYTE * ptrsend )
{
	int t_size = 0;
	
	if ( ptrsr == NULL || ptrsend == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	t_size = make_sr_packet_com_sns( ptrsr, ptrsend );

	if ( t_size > 0 )
	{
		write_remote( t_size, (char*)ptrsend , ptrsr->cmd ); 
	}

	return ERR_RETURN;
}

/*================================================================================================
내부 함수  정의 
매개변수가 pointer일경우 반드시 NULL 체크 적용 
================================================================================================*/
static int parse_remote_cmd( BYTE cmd, void * ptrdata )
{
	int i,cnt = 0;
	parse tmp_parse = NULL;
	BYTE l_cmd = 0;

	cnt = ( sizeof( g_parse_remote_func_list ) / sizeof( parse_cmd_func_t ));

	for( i = 0 ; i < cnt ; i++ )
	{
		tmp_parse 	= g_parse_remote_func_list[ i ].parse_func ;
		l_cmd 		= g_parse_remote_func_list[ i ].cmd; 

		if ( tmp_parse!= NULL && cmd == l_cmd )
		{
			tmp_parse( cmd, ptrdata );
		}
	}

	return ERR_SUCCESS;	
}

static int write_remote( int len, char * ptrdata, BYTE cmd )
{
	int ret = ERR_SUCCESS;

	if( g_remote_fd < 0 )
	{
		print_dbg( DBG_ERR,1, "Can not Write REMOTE Serial, fd:%d", g_remote_fd );
		return ERR_RETURN;
	}
	
	if ( ptrdata == NULL || len ==  0 )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* lock */
	lock_sem( g_remote_sem_id, g_remote_sem_key );
	ret = write_serial_com_sns( g_remote_fd, ptrdata, len );
	unlock_sem( g_remote_sem_id, g_remote_sem_key );
	/* unlock */
	if ( ret <= 0 )
	{
		print_dbg( DBG_ERR,1, "Can not Write REMOTE Serial, fd:%d, len:%d", g_remote_fd, len );
	}
	else
	{
		/* send list buf에 등록 */
		lock_sem( g_remote_sem_id, g_remote_sem_key );
		add_send_list_com_sns( cmd, &g_send_sr_list, ret, ptrdata , DEV_ID_REMOTE );
		unlock_sem( g_remote_sem_id, g_remote_sem_key );

	}
	return ret;
}

static int read_remote( void )
{

	int size ;
	char * ptrrecv= NULL;
	sr_data_t sr_data;

	if ( g_remote_fd > 0 )
	{
		ptrrecv = g_remote_queue.ptrrecv_buf;

		if ( ptrrecv != NULL )
		{
			size =0;
			memset( ptrrecv, 0, MAX_SR_RECV_SIZE );
			size = read( g_remote_fd , ptrrecv, MAX_SR_RECV_SIZE  );

			if ( size > 0 )
			{
				memset( &sr_data, 0, sizeof( sr_data ));
				if ( g_ptrread_sr != NULL )
				{
					sr_data.ptrdata 	= g_ptrread_sr;
					memset( sr_data.ptrdata , 0,  MAX_SR_DEF_DATA_SIZE );
				}

				//print_buf( size, (BYTE*)ptrrecv );
				parse_serial_data_com_sns( g_remote_fd, size, &g_remote_queue, parse_remote_cmd, REMOTE_NAME, &sr_data );
				//g_last_read_time = time( NULL );
			}
			//print_buf(size, (BYTE*)ptrrecv );
			//print_dbg( DBG_INFO,1,"size:%d, %s", size, ptrrecv);
		}
		else
		{
			print_dbg( DBG_ERR, 1, "Not have remote recv buffer");
		}
	}

	return ERR_SUCCESS;
}

static int create_remote_thread( void )
{
	int ret = ERR_SUCCESS;

	if ( g_release == 0 && g_start_remote_pthread == 0 && g_end_remote_pthread == 1 )
	{
		g_start_remote_pthread = 1;

		if ( pthread_create( &remote_pthread, NULL, remote_pthread_idle , NULL ) < 0 )
		{
			g_start_remote_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create REMOTE Thread" );
			ret = ERR_RETURN;
		}
		else
		{
			//print_dbg( DBG_INIT, 1, "mini-SEED Thread Create OK. ");
		}
	}

	return ret;
}

#if 0
static int release_remote_thread( void )
{
	int status = 0;
	
	if ( g_end_remote_pthread == 0 )
	{
		pthread_join( remote_pthread , ( void **)&status );
		g_start_remote_pthread 	= 0;
		g_end_remote_pthread 		= 1;
	}

	return ERR_SUCCESS;
}
#endif

static void *  remote_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;

	g_end_remote_pthread = 0;

	while( g_release == 0 && g_start_remote_pthread == 1 )
	{
		read_remote();
	}

	if( g_end_remote_pthread == 0 )
	{
		pthread_join( remote_pthread , ( void **)&status );
		g_start_remote_pthread 	= 0;
		g_end_remote_pthread 		= 1;
	}

	return ERR_SUCCESS;
}

static int set_remote_fd( void )
{
	int fd = -1;

	if ( g_remote_fd >= 0 )
		return ERR_SUCCESS;

	fd = set_serial_fd_com_sns( REMOTE_BAUD, REMOTE_TTY, REMOTE_NAME );

	if ( fd >= 0 )
	{
		g_remote_fd = fd;
	}
	else
	{
		set_remote_trans_sts_ctrl( ESNSRCNNST_ERROR );
		return ERR_RETURN;
	}

	return ERR_SUCCESS;
}

static int release_remote_fd( void )
{
	if ( g_remote_fd >= 0 )
	{
		close ( g_remote_fd );
		g_remote_fd = -1;
	}

	return ERR_SUCCESS;
}


/* CONTROL module에  보낼 데이터(제어, 설정, 알람)가  있을 경우 호출 */
static int send_remote_data_control( UINT16 inter_msg, void * ptrdata  )
{
	if ( g_ptrsend_remote_data_control_func != NULL )
	{
		return g_ptrsend_remote_data_control_func( inter_msg, ptrdata );
	}

	return ERR_RETURN;
}

/* CONTROL Module로 부터 수집 데이터, 알람, 장애  정보를 받는 내부 함수로
   등록된 proc function list 중에서 control에서  넘겨 받은 inter_msg와 동일한 msg가 존재할 경우
   해당  함수를 호출하도록 한다. 
 */
static int internal_remote_recv_ctrl_data( unsigned short inter_msg, void * ptrdata )
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
	proc_func_cnt= ( sizeof( g_proc_remote_func_list ) / sizeof( proc_func_t ));

	/* 등록된 proc function에서 inter_msg가 동일한 function 찾기 */
	for( i = 0 ; i < proc_func_cnt ; i++ )
	{    
		list_msg 			= g_proc_remote_func_list[ i ].inter_msg; 	/* function list에 등록된 msg id */
		ptrtemp_proc_func 	= g_proc_remote_func_list[ i ].ptrproc;	/* function list에 등록된 function point */

		if ( ptrtemp_proc_func != NULL && list_msg == inter_msg )
		{    
			ptrtemp_proc_func( inter_msg, ptrdata );
			break;
		}    
	} 

	return ERR_SUCCESS;
}

/* remote module 초기화 내부 작업 */
static int internal_init( void )
{
	int i,ret = ERR_SUCCESS;

	/* warring을 없애기 위한 함수 호출 */
	send_remote_data_control( 0, NULL );

	ret = set_remote_fd();
	
	if ( ret == ERR_SUCCESS )
	{
		ret = create_remote_thread( );
	}

	memset( &g_remote_queue, 0, sizeof( g_remote_queue ));
	memset( &g_send_sr_list, 0,sizeof( g_send_sr_list ));

	/* buf 생성 */
	g_remote_queue.ptrlist_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_QUEUE_SIZE );
	g_remote_queue.ptrrecv_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_RECV_SIZE );	
	g_remote_queue.ptrsend_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_SEND_SIZE );	

	g_ptrread_sr			= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );
	g_ptrcom_sr				= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );
	g_ptrsts_sr				= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );	
	g_ptrctrl_sr			= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );	


	if( g_remote_queue.ptrlist_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create REMOTE SR List Buffer");
		ret = ERR_RETURN;
	}

	if( g_remote_queue.ptrrecv_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create REMOTE SR Recv Buffer");
		ret = ERR_RETURN;
	}

	if( g_remote_queue.ptrsend_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create REMOTE SR Send Buffer");
		ret = ERR_RETURN;
	}

	if ( ret == ERR_SUCCESS )
	{
		print_dbg(DBG_INFO, 1, "Success of REMOTE Module Initialize");
	}
	else
	{
		release_buf();
	}


	/* send list buf 생성 */
	for( i = 0 ; i < MAX_SR_SEND_CNT ; i++ )
	{
		g_send_sr_list.send_data[ i ].ptrdata= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE);
		if ( g_send_sr_list.send_data[ i ].ptrdata== NULL )
		{
			print_dbg(DBG_ERR, 1, "Can not Create REMOTE SR Send List Buffer");
			ret = ERR_RETURN;
		}
		else
		{
			memset( g_send_sr_list.send_data[ i ].ptrdata, 0, MAX_SR_DEF_DATA_SIZE );
		}
	}

	g_remote_sem_id = create_sem( g_remote_sem_key );
	print_dbg( DBG_INFO, 1, " REMOTE SEMA : %d", g_remote_sem_id );

	sys_sleep( 2000 );
	proc_remote_com_req( 0, NULL );

	g_bit_val [ 0 ] = BIT_0;
	g_bit_val [ 1 ] = BIT_1;
	g_bit_val [ 2 ] = BIT_2;
	g_bit_val [ 3 ] = BIT_3;
	g_bit_val [ 4 ] = BIT_4;
	g_bit_val [ 5 ] = BIT_5;
	g_bit_val [ 6 ] = BIT_6;
	g_bit_val [ 7 ] = BIT_7;

	return ret;
}

static int idle_remote_func( void )
{
	time_t cur_time;
	static time_t pre_time = 0;
	
	cur_time = time( NULL );
	
	if( pre_time == 0 ) 
		pre_time = cur_time;

	if ( cur_time - pre_time > TEST_INTVAL )
	{
		pre_time = cur_time;
		//proc_remote_com_req( 0, NULL );
		//proc_remote_ctrl_req( 0 , NULL );
		//sys_sleep( 10000 );
		proc_remote_status_req( 0 , NULL );
		//print_dbg( DBG_INFO,1, "SEND INFO CMD");
	}
	
	check_remote_sr_timeout( );

	return ERR_SUCCESS;
}

/* remote module loop idle 내부 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
static int internal_idle( void * ptr )
{
	( void )ptr;

	if( g_release == 0 )
	{
		create_remote_thread();
		set_remote_fd( );

		idle_remote_func();
	}
	return ERR_SUCCESS;
}

static int release_buf ( void )
{
	int i = 0;

	if ( g_remote_queue.ptrlist_buf != NULL )
	{
		free( g_remote_queue.ptrlist_buf );
		g_remote_queue.ptrlist_buf  = NULL;
	}

	if ( g_remote_queue.ptrrecv_buf != NULL )
	{
		free( g_remote_queue.ptrrecv_buf );
		g_remote_queue.ptrrecv_buf  = NULL;
	}

	if ( g_remote_queue.ptrsend_buf != NULL )
	{
		free( g_remote_queue.ptrsend_buf );
		g_remote_queue.ptrsend_buf  = NULL;
	}

	if( g_ptrread_sr != NULL )
	{ 
		free( g_ptrread_sr );
		g_ptrread_sr  = NULL ;
	}
	if( g_ptrcom_sr != NULL )
	{ 
		free( g_ptrcom_sr  );
		g_ptrcom_sr  = NULL ;
	}
	if( g_ptrsts_sr != NULL )
	{ 
		free( g_ptrsts_sr  );
		g_ptrsts_sr  = NULL ;
	}
	if( g_ptrctrl_sr != NULL )
	{ 
		free( g_ptrctrl_sr  );
		g_ptrctrl_sr  = NULL ;
	}

	
	for( i = 0 ; i < MAX_SR_SEND_CNT ;i++ )
	{
		if ( g_send_sr_list.send_data[ i ].ptrdata != NULL )
		{
			free( g_send_sr_list.send_data[ i ].ptrdata );
			g_send_sr_list.send_data[ i ].ptrdata = NULL;
		}
	}
	if ( g_remote_sem_id > 0 )
	{
		destroy_sem( g_remote_sem_id, g_remote_sem_key );
		g_remote_sem_id = -1;
	}

	return ERR_SUCCESS;
}

/* remote module의 release 내부 작업 */
static int internal_release( void )
{
	g_release = 1;
	
	release_remote_fd();
	//release_remote_thread();

	print_dbg(DBG_INFO, 1, "Release of REMOTE Module");
	return ERR_SUCCESS;
}

/*================================================================================================
외부  함수  정의 
================================================================================================*/
int init_remote( send_data_func ptrsend_data )
{
	/* CONTROL module의 함수 포인트 등록 */
	g_ptrsend_remote_data_control_func = ptrsend_data ;	/* CONTROL module로 센서 , 장애  데이터를 보낼 수 있는 함수 포인터 */

	/* 기타 초기화 작업 */
	internal_init();

	return ERR_SUCCESS;
}

/* remote module loop idle 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
int idle_remote( void * ptr )
{
	return internal_idle( ptr );
}

/* 동적 메모리, fd 등 resouce release함수 */
int release_remote( void )
{
	/* release 작업  */
	return internal_release(); 
}

/* CONTROL Module로 부터 제어, 설정, update 정보를 받는 외부 함수 */
int recv_ctrl_data_remote( unsigned short inter_msg, void * ptrdata )
{
	return internal_remote_recv_ctrl_data( inter_msg, ptrdata );
}
