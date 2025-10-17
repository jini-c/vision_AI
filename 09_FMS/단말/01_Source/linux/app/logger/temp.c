/*********************************************************************
	file : temp.c
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

#include "temp.h"

#define TEMP_COM		0x00
#define TEMP_STS		0x01
#define TEMP_LCD_ON		0x02
/*================================================================================================
 전역 변수 
 1.static 적용 
 2.숫자형 데이터 일경우 volatile 적용 
 3.초기값 적용.
================================================================================================*/
static send_data_func  g_ptrsend_temp_data_control_func = NULL;		/* CONTROL 모듈  센서 데이터 ,  알람  정보를 넘기는 함수 포인트 */
static sr_queue_t g_temp_queue;
static send_sr_list_t g_send_sr_list;
static volatile int g_temp_sr_timeout		= 0;

/* thread ( temp )*/
static volatile int g_start_temp_pthread 	= 0; 
static volatile int g_end_temp_pthread 		= 1; 
static pthread_t temp_pthread;
static void * temp_pthread_idle( void * ptrdata );
static volatile int g_release				= 0;

static char * g_ptrread_sr					= NULL;
static char * g_ptrcom_sr					= NULL;
static char * g_ptrsts_sr					= NULL;

/* serial */
static int set_temp_fd( void );
static int release_temp_fd( void );
static volatile int g_temp_fd 				= -1;

/*================================================================================================
  내부 함수 선언
  static 적용 
================================================================================================*/
static int send_temp_data_control( UINT16 inter_msg, void * ptrdata );

static int internal_init( void );
static int internal_idle( void * ptr );
static int internal_release( void );
static int create_temp_thread( void );

static int release_buf( void );
static int write_temp( int len, char * ptrdata , BYTE cmd);

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

static int parse_temp_comm_packet( BYTE cmd, void * ptrdata );
static int parse_temp_status_packet( BYTE cmd, void * ptrdata );
static int parse_temp_lcd_on_packet( BYTE cmd, void * ptrdata );

static int make_temp_packet( sr_data_t * ptrsr, BYTE * ptrsend );

static int proc_temp_com_req( UINT16 msg, void * ptrdata );
static int proc_temp_status_req( UINT16 msg, void * ptrdata );
static int proc_temp_lcd_on_req( void );

static proc_func_t g_proc_temp_func_list []= 
{
	/* inter msg id   		,  			proc function point */
	/*=============================================*/

	{ 0, NULL }
};

static parse_cmd_func_t g_parse_temp_func_list[ ] = 
{
	{ TEMP_COM, 	parse_temp_comm_packet },
	{ TEMP_STS, 	parse_temp_status_packet },
	{ TEMP_LCD_ON, 	parse_temp_lcd_on_packet },
	{ 0xFF	, 	NULL }
};

static int check_temp_sr_timeout( void )
{
	int ret = ERR_SUCCESS;
	int resend_pos = -1;
	char * ptrdata = NULL;
	BYTE cmd;
	int len = 0;

	ret = chk_send_list_timeout_com_sns( &g_send_sr_list , &resend_pos, DEV_ID_TEMP );

	if ( ret == RET_TIMEOUT )
	{
		set_temp_trans_sts_ctrl( ESNSRCNNST_ERROR );
	}
	else if( ret == RET_RESEND )
	{

		if( resend_pos >=0 )
		{
			cmd 	= g_send_sr_list.send_data[ resend_pos ].cmd;
			len		= g_send_sr_list.send_data[ resend_pos ].len;
			ptrdata = g_send_sr_list.send_data[ resend_pos ].ptrdata;

			write_temp( len, ptrdata, cmd );

			print_dbg( DBG_INFO, 1, "Resend TEMP SR Data cmd:%d, len:%d", cmd, len );

		}
	}
	return ERR_SUCCESS;
}

static int check_temp_sr_restore_timeout( BYTE cmd )
{
	int ret = 0;

	ret = del_send_list_com_sns( cmd, &g_send_sr_list, DEV_ID_TEMP );
	
	/* 0: 2초이내 응답이 온 경우, 1: 응답은왔으나 2초 이내가 아닐 경우 */
	if ( ret == 0 )
	{
		set_temp_trans_sts_ctrl( ESNSRCNNST_NORMAL );
	}

	return ERR_SUCCESS;
}

static int proc_temp_com_req( UINT16 msg, void * ptrdata )
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
	sr_data.cmd 		= TEMP_COM;
	sr_data.dev_id		= 0x00;
	sr_data.dev_addr	= 0x00;
	sr_data.dev_ch		= 0x00;
	sr_data.mode		= 0x00;
	sr_data.resp_time	= 0x00;

	return make_temp_packet( &sr_data, com_send_buf );
}

static int proc_temp_status_req( UINT16 msg, void * ptrdata )
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

	sr_data.len 		= 0x00;
	sr_data.cmd 		= TEMP_STS;
	sr_data.dev_id		= DEV_ID_TEMP;
	sr_data.dev_addr	= 0x00;
	sr_data.dev_ch		= 0x00;
	sr_data.mode		= 0x00;
	sr_data.resp_time	= 0x00;

	return make_temp_packet( &sr_data, sts_send_buf );
}

static int proc_temp_lcd_on_req( void  )
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

	sr_data.len 		= 0x00;
	sr_data.cmd 		= TEMP_LCD_ON;
	sr_data.dev_id		= DEV_ID_TEMP;
	sr_data.dev_addr	= 0x00;
	sr_data.dev_ch		= 0x00;
	sr_data.mode		= 0x00;
	sr_data.resp_time	= 0x00;

	return make_temp_packet( &sr_data, sts_send_buf );
}
static int parse_temp_comm_packet( BYTE cmd, void * ptrdata )
{
	
	sr_data_t * ptrsr = NULL;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrsr = (sr_data_t *)ptrdata;

	//print_dbg(DBG_NONE, 1,"Recv TEMP COMMON Packet");

	check_temp_sr_restore_timeout(ptrsr->cmd );
	
	set_temp_ver_ctrl( (BYTE*)ptrsr->ptrdata );

	return ERR_SUCCESS;
}

static int parse_temp_status_packet( BYTE cmd, void * ptrdata )
{
	sr_data_t * ptrsr = NULL;
	int temp_val , humi_val;
	temp_data_t temp_data;
	int t_minus = 1;
	int h_minus = 1;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}


	ptrsr = (sr_data_t *)ptrdata;

	//print_dbg(DBG_NONE, 1,"Recv TEMP Status Packet");

	check_temp_sr_restore_timeout(ptrsr->cmd );
#if 0
	print_dbg(DBG_NONE,1,"len:%x, cmd:%x, dev_id:%x, dev_addr:%x, dev_ch:%x, mode:%x, req_time:%x, check_sum:%x",
			  ptrsr->len, ptrsr->cmd, ptrsr->dev_id, ptrsr->dev_addr, ptrsr->dev_ch, ptrsr->mode, ptrsr->resp_time , ptrsr->check_sum );
#endif

	memset(&temp_data, 0, sizeof( temp_data ));

	memcpy( &temp_val, ptrsr->ptrdata, sizeof( int ));
	temp_val = ntohl( temp_val );
	if ( (temp_val & 0x800000000) > 0 )
	{
		t_minus = -1;
	}

	ptrsr->ptrdata +=4;
	memcpy( &humi_val, ptrsr->ptrdata, sizeof( int ));
	humi_val = ntohl( humi_val );
	if ( (humi_val & 0x800000000) > 0 )
	{
		h_minus = -1;
	}

	temp_data.heat= ((float)( temp_val & 0x7fffffff )/100 ) ;
	temp_data.humi = ((float)( humi_val & 0x7fffffff )/100) ;

	temp_data.heat *= t_minus;
	temp_data.humi *= h_minus;

	set_temp_data_ctrl( &temp_data );

	return ERR_SUCCESS;
}

static int parse_temp_lcd_on_packet( BYTE cmd, void * ptrdata )
{
	sr_data_t * ptrsr = NULL;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrsr = (sr_data_t *)ptrdata;

	//print_dbg(DBG_NONE, 1,"Recv TEMP LCD ON Packet");

	check_temp_sr_restore_timeout(ptrsr->cmd );
#if 0
	print_dbg(DBG_NONE,1,"len:%x, cmd:%x, dev_id:%x, dev_addr:%x, dev_ch:%x, mode:%x, req_time:%x, check_sum:%x",
			  ptrsr->len, ptrsr->cmd, ptrsr->dev_id, ptrsr->dev_addr, ptrsr->dev_ch, ptrsr->mode, ptrsr->resp_time , ptrsr->check_sum );
#endif

	return ERR_SUCCESS;
}

static int make_temp_packet( sr_data_t * ptrsr, BYTE * ptrsend  )
{
	int t_size = 0;
	
	if ( ptrsr == NULL || ptrsend == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	t_size = make_sr_packet_com_sns( ptrsr, ptrsend);

	if ( t_size > 0 )
	{
		write_temp( t_size, (char*)ptrsend , ptrsr->cmd ); 
	}

	return ERR_RETURN;
}

/*================================================================================================
내부 함수  정의 
매개변수가 pointer일경우 반드시 NULL 체크 적용 
================================================================================================*/
static int parse_temp_cmd( BYTE cmd, void * ptrdata )
{
	int i,cnt = 0;
	parse tmp_parse = NULL;
	BYTE l_cmd = 0;

	cnt = ( sizeof( g_parse_temp_func_list ) / sizeof( parse_cmd_func_t ));

	for( i = 0 ; i < cnt ; i++ )
	{
		tmp_parse 	= g_parse_temp_func_list[ i ].parse_func ;
		l_cmd 		= g_parse_temp_func_list[ i ].cmd; 

		if ( tmp_parse!= NULL && cmd == l_cmd )
		{
			tmp_parse( cmd, ptrdata );
		}
	}

	return ERR_SUCCESS;	
}

static int write_temp( int len, char * ptrdata, BYTE cmd )
{
	int ret = ERR_SUCCESS;

	if( g_temp_fd < 0 )
	{
		print_dbg( DBG_ERR,1, "Can not Write TEMP Serial, fd:%d", g_temp_fd );
		return ERR_RETURN;
	}

	if ( ptrdata == NULL || len ==  0 )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ret = write_serial_com_sns( g_temp_fd, ptrdata, len );
	//print_buf( len, (BYTE*)ptrdata );
	if ( ret <= 0 )
	{
		print_dbg( DBG_ERR,1, "Can not Write TEMP Serial, fd:%d, len:%d", g_temp_fd, len );
	}
	else
	{
		/* send list buf에 등록 */
		add_send_list_com_sns( cmd, &g_send_sr_list, ret, ptrdata , DEV_ID_TEMP );
	}
	return ret;
}

static int read_temp( void )
{

	int size ;
	char * ptrrecv= NULL;
	sr_data_t sr_data;

	if ( g_temp_fd > 0 )
	{
		ptrrecv = g_temp_queue.ptrrecv_buf;

		if ( ptrrecv != NULL )
		{
			size =0;
			memset( ptrrecv, 0, MAX_SR_RECV_SIZE );
			size = read( g_temp_fd , ptrrecv, MAX_SR_RECV_SIZE  );

			if ( size > 0 )
			{
				memset( &sr_data, 0, sizeof( sr_data ));
				if ( g_ptrread_sr != NULL )
				{
					sr_data.ptrdata 	= g_ptrread_sr;
					memset( sr_data.ptrdata , 0,  MAX_SR_DEF_DATA_SIZE );
				}

				//print_buf( size, (BYTE*)ptrrecv );
				parse_serial_data_com_sns(g_temp_fd,  size, &g_temp_queue, parse_temp_cmd, TEMP_NAME, &sr_data );
				//g_last_read_time = time( NULL );
			}
			//print_buf(size, (BYTE*)ptrrecv );
			//print_dbg( DBG_INFO,1,"size:%d, %s", size, ptrrecv);
		}
		else
		{
			print_dbg( DBG_ERR, 1, "Not have temp recv buffer");
		}
	}

	return ERR_SUCCESS;
}

static int create_temp_thread( void )
{
	int ret = ERR_SUCCESS;

	if ( g_release == 0 && g_start_temp_pthread == 0 && g_end_temp_pthread == 1 )
	{
		g_start_temp_pthread = 1;

		if ( pthread_create( &temp_pthread, NULL, temp_pthread_idle , NULL ) < 0 )
		{
			g_start_temp_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create TEMP Thread" );
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
static int release_temp_thread( void )
{
	int status = 0;
	
	if ( g_end_temp_pthread == 0 )
	{
		pthread_join( temp_pthread , ( void **)&status );
		g_start_temp_pthread 	= 0;
		g_end_temp_pthread 		= 1;
	}

	return ERR_SUCCESS;
}
#endif
static void *  temp_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;

	g_end_temp_pthread = 0;

	while( g_release == 0 && g_start_temp_pthread == 1 )
	{
		read_temp();
	}

	if( g_end_temp_pthread == 0 )
	{
		pthread_join( temp_pthread , ( void **)&status );
		g_start_temp_pthread 	= 0;
		g_end_temp_pthread 		= 1;
	}

	return ERR_SUCCESS;
}

static int set_temp_fd( void )
{
	int fd = -1;

	if ( g_temp_fd >= 0 )
		return ERR_SUCCESS;

	fd = set_serial_fd_com_sns( TEMP_BAUD, TEMP_TTY, TEMP_NAME );

	if ( fd >= 0 )
	{
		g_temp_fd = fd;
	}
	else
	{
		set_temp_trans_sts_ctrl( ESNSRCNNST_ERROR );
		return ERR_RETURN;
	}

	return ERR_SUCCESS;
}

static int release_temp_fd( void )
{
	if ( g_temp_fd >= 0 )
	{
		close ( g_temp_fd );
		g_temp_fd = -1;
	}

	return ERR_SUCCESS;
}


/* CONTROL module에  보낼 데이터(제어, 설정, 알람)가  있을 경우 호출 */
static int send_temp_data_control( UINT16 inter_msg, void * ptrdata  )
{
	if ( g_ptrsend_temp_data_control_func != NULL )
	{
		return g_ptrsend_temp_data_control_func( inter_msg, ptrdata );
	}

	return ERR_RETURN;
}

/* CONTROL Module로 부터 수집 데이터, 알람, 장애  정보를 받는 내부 함수로
   등록된 proc function list 중에서 control에서  넘겨 받은 inter_msg와 동일한 msg가 존재할 경우
   해당  함수를 호출하도록 한다. 
 */
static int internal_temp_recv_ctrl_data( unsigned short inter_msg, void * ptrdata )
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
	proc_func_cnt= ( sizeof( g_proc_temp_func_list ) / sizeof( proc_func_t ));

	/* 등록된 proc function에서 inter_msg가 동일한 function 찾기 */
	for( i = 0 ; i < proc_func_cnt ; i++ )
	{    
		list_msg 			= g_proc_temp_func_list[ i ].inter_msg; 	/* function list에 등록된 msg id */
		ptrtemp_proc_func 	= g_proc_temp_func_list[ i ].ptrproc;	/* function list에 등록된 function point */

		if ( ptrtemp_proc_func != NULL && list_msg == inter_msg )
		{    
			ptrtemp_proc_func( inter_msg, ptrdata );
			break;
		}    
	} 

	return ERR_SUCCESS;
}

/* temp module 초기화 내부 작업 */
static int internal_init( void )
{
	int i,ret = ERR_SUCCESS;

	/* warring을 없애기 위한 함수 호출 */
	send_temp_data_control( 0, NULL );

	ret = set_temp_fd();
	
	if ( ret == ERR_SUCCESS )
	{
		ret = create_temp_thread( );
	}

	memset( &g_temp_queue, 0, sizeof( g_temp_queue ));
	memset( &g_send_sr_list, 0,sizeof( g_send_sr_list ));

	/* buf 생성 */
	g_temp_queue.ptrlist_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_QUEUE_SIZE );
	g_temp_queue.ptrrecv_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_RECV_SIZE );	
	g_temp_queue.ptrsend_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_SEND_SIZE );	

	g_ptrread_sr			= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );
	g_ptrcom_sr				= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );
	g_ptrsts_sr				= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );	

	if( g_temp_queue.ptrlist_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create TEMP SR List Buffer");
		ret = ERR_RETURN;
	}

	if( g_temp_queue.ptrrecv_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create TEMP SR Recv Buffer");
		ret = ERR_RETURN;
	}

	if( g_temp_queue.ptrsend_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create TEMP SR Send Buffer");
		ret = ERR_RETURN;
	}

	if ( ret == ERR_SUCCESS )
	{
		print_dbg(DBG_INFO, 1, "Success of TEMP Module Initialize");
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
			print_dbg(DBG_ERR, 1, "Can not Create TEMP SR Send List Buffer");
			ret = ERR_RETURN;
		}
		else
		{
			memset( g_send_sr_list.send_data[ i ].ptrdata, 0, MAX_SR_DEF_DATA_SIZE );
		}
	}

	sys_sleep( 2000 );
	proc_temp_com_req( 0, NULL );

	return ret;
}

static int idle_temp_func( void )
{
	time_t cur_time;
	static time_t pre_time = 0;

	cur_time = time( NULL );
	if( pre_time == 0 ) 
		pre_time = cur_time;

	if ( cur_time - pre_time > TEST_INTVAL )
	{

		pre_time = cur_time;
		//proc_temp_com_req( 0, NULL );
		//sys_sleep( 2000 );
		//sys_sleep( 2000 );
		proc_temp_status_req( 0 , NULL );
		//print_dbg( DBG_INFO,1, "############### SEND INFO CMD TEMP");
	}
	
	check_temp_sr_timeout( );

	return ERR_SUCCESS;
}

/* temp module loop idle 내부 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
static int internal_idle( void * ptr )
{
	( void )ptr;

	if( g_release == 0 )
	{
		create_temp_thread();
		set_temp_fd( );

		idle_temp_func();
	}
	return ERR_SUCCESS;
}

static int release_buf ( void )
{
	int i = 0;

	if ( g_temp_queue.ptrlist_buf != NULL )
	{
		free( g_temp_queue.ptrlist_buf );
		g_temp_queue.ptrlist_buf  = NULL;
	}

	if ( g_temp_queue.ptrrecv_buf != NULL )
	{
		free( g_temp_queue.ptrrecv_buf );
		g_temp_queue.ptrrecv_buf  = NULL;
	}

	if ( g_temp_queue.ptrsend_buf != NULL )
	{
		free( g_temp_queue.ptrsend_buf );
		g_temp_queue.ptrsend_buf  = NULL;
	}

	if( g_ptrread_sr != NULL )
	{ 
		free( g_ptrread_sr );
		g_ptrread_sr  = NULL ;
	}
	if( g_ptrcom_sr != NULL )
	{ 
		free( g_ptrcom_sr );
		g_ptrcom_sr  = NULL ;
	}
	if( g_ptrsts_sr != NULL )
	{ 
		free( g_ptrsts_sr );
		g_ptrsts_sr  = NULL ;
	}
	
	for( i = 0 ; i < MAX_SR_SEND_CNT ;i++ )
	{
		if ( g_send_sr_list.send_data[ i ].ptrdata != NULL )
		{
			free( g_send_sr_list.send_data[ i ].ptrdata );
			g_send_sr_list.send_data[ i ].ptrdata = NULL;
		}
	}

	return ERR_SUCCESS;
}

/* temp module의 release 내부 작업 */
static int internal_release( void )
{
	g_release = 1;
	
	release_temp_fd();
	//release_temp_thread();

	print_dbg(DBG_INFO, 1, "Release of TEMP Module");
	return ERR_SUCCESS;
}

/*================================================================================================
외부  함수  정의 
================================================================================================*/
int init_temp( send_data_func ptrsend_data )
{
	/* CONTROL module의 함수 포인트 등록 */
	g_ptrsend_temp_data_control_func = ptrsend_data ;	/* CONTROL module로 센서 , 장애  데이터를 보낼 수 있는 함수 포인터 */

	/* 기타 초기화 작업 */
	internal_init();

	return ERR_SUCCESS;
}

/* temp module loop idle 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
int idle_temp( void * ptr )
{
	return internal_idle( ptr );
}

/* 동적 메모리, fd 등 resouce release함수 */
int release_temp( void )
{
	/* release 작업  */
	return internal_release(); 
}

/* CONTROL Module로 부터 제어, 설정, update 정보를 받는 외부 함수 */
int recv_ctrl_data_temp( unsigned short inter_msg, void * ptrdata )
{
	return internal_temp_recv_ctrl_data( inter_msg, ptrdata );
}

int recv_lcd_on_temp( void )
{
	return proc_temp_lcd_on_req( );
}

