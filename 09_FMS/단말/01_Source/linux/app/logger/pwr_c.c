/*********************************************************************
	file : pwr_c.c
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

#include "pwr_c.h"

#define PWR_C_COM		0x00
#define PWR_C_STS		0x01
#define PWR_C_CTRL		0x02
/*================================================================================================
 전역 변수 
 1.static 적용 
 2.숫자형 데이터 일경우 volatile 적용 
 3.초기값 적용.
================================================================================================*/
static volatile int g_pwr_c_sem_key 						= PWR_C_SEM_KEY;
static volatile int g_pwr_c_sem_id 						= 0;

static send_data_func  g_ptrsend_pwr_c_data_control_func = NULL;		/* CONTROL 모듈  센서 데이터 ,  알람  정보를 넘기는 함수 포인트 */
static sr_queue_t g_pwr_c_queue;
static BYTE g_bit_val[ MAX_BIT_CNT ];
static send_sr_list_t g_send_sr_list;

static volatile int g_clientid 				= 0;

/* thread ( pwr_c )*/
static volatile int g_start_pwr_c_pthread 	= 0; 
static volatile int g_end_pwr_c_pthread 		= 1; 
static pthread_t pwr_c_pthread;
static void * pwr_c_pthread_idle( void * ptrdata );
static volatile int g_release				= 0;

static  char * g_ptrread_sr					= NULL;
static  char * g_ptrcom_sr					= NULL;
static  char* g_ptrsts_sr					= NULL;
static  char* g_ptrctrl_sr					= NULL;

/* serial */
static int set_pwr_c_fd( void );
static int release_pwr_c_fd( void );
static volatile int g_pwr_c_fd 				= -1;

/*================================================================================================
  내부 함수 선언
  static 적용 
================================================================================================*/
static int send_pwr_c_data_control( UINT16 inter_msg, void * ptrdata );

static int internal_init( void );
static int internal_idle( void * ptr );
static int internal_release( void );
static int create_pwr_c_thread( void );

static int release_buf( void );
static int write_pwr_c( int len, char * ptrdata , BYTE cmd);

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

static int parse_pwr_c_comm_packet( BYTE cmd, void * ptrdata );
static int parse_pwr_c_status_packet( BYTE cmd, void * ptrdata );
static int parse_pwr_c_ctrl_packet( BYTE cmd, void * ptrdata );

static int make_pwr_c_packet( sr_data_t * ptrsr, BYTE * ptrsend );

static int proc_pwr_c_com_req( UINT16 msg, void * ptrdata );
static int proc_pwr_c_status_req( UINT16 msg, void * ptrdata );
static int proc_pwr_c_ctrl_req( UINT16 msg, void * ptrdata );

static proc_func_t g_proc_pwr_c_func_list []= 
{
	/* inter msg id   		,  			proc function point */
	/*=============================================*/
	{ EFT_SNSRCTL_POWER, proc_pwr_c_ctrl_req },
	{ 0, NULL }
};

static parse_cmd_func_t g_parse_pwr_c_func_list[ ] = 
{
	{ PWR_C_COM, 	parse_pwr_c_comm_packet },
	{ PWR_C_STS, 	parse_pwr_c_status_packet },
	{ PWR_C_CTRL, 	parse_pwr_c_ctrl_packet },
	{ 0xFF	, 	NULL }
};

static int check_pwr_c_sr_timeout( void )
{
	int ret = ERR_SUCCESS;
	int resend_pos = -1;
	char * ptrdata = NULL;
	BYTE cmd;
	int len = 0;

	lock_sem( g_pwr_c_sem_id, g_pwr_c_sem_key );
	ret = chk_send_list_timeout_com_sns( &g_send_sr_list , &resend_pos, DEV_ID_PWR_C );
	unlock_sem( g_pwr_c_sem_id, g_pwr_c_sem_key );

	if ( ret == RET_TIMEOUT )
	{
		set_pwr_c_trans_sts_ctrl( ESNSRCNNST_ERROR );

	}
	else if( ret == RET_RESEND )
	{

		if( resend_pos >=0 )
		{
			cmd 	= g_send_sr_list.send_data[ resend_pos ].cmd;
			len		= g_send_sr_list.send_data[ resend_pos ].len;
			ptrdata = g_send_sr_list.send_data[ resend_pos ].ptrdata;

			write_pwr_c( len, ptrdata, cmd );

			print_dbg( DBG_INFO, 1, "Resend PWR_C SR Data cmd:%d, len:%d", cmd, len );

		}
	}
	return ERR_SUCCESS;
}

static int check_pwr_c_sr_restore_timeout( BYTE cmd )
{
	int ret = 0;

	lock_sem( g_pwr_c_sem_id, g_pwr_c_sem_key );
	ret = del_send_list_com_sns( cmd, &g_send_sr_list, DEV_ID_PWR_C );
	unlock_sem( g_pwr_c_sem_id, g_pwr_c_sem_key );
		
	/* 0: 2초이내 응답이 온 경우, 1: 응답은왔으나 2초 이내가 아닐 경우 */
	if ( ret == 0 )
	{
		set_pwr_c_trans_sts_ctrl( ESNSRCNNST_NORMAL );
	}

	return ERR_SUCCESS;
}

static int proc_pwr_c_com_req( UINT16 msg, void * ptrdata )
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
	sr_data.cmd 		= PWR_C_COM;
	sr_data.dev_id		= 0x00;
	sr_data.dev_addr	= 0x00;
	sr_data.dev_ch		= 0x00;
	sr_data.mode		= 0x00;
	sr_data.resp_time	= 0x00;

	return make_pwr_c_packet( &sr_data, com_send_buf );
}

static int proc_pwr_c_status_req( UINT16 msg, void * ptrdata )
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
	sr_data.cmd 		= PWR_C_STS;
	sr_data.dev_id		= DEV_ID_PWR_C;
	sr_data.dev_addr	= 0x00;
	sr_data.dev_ch		= 0x00;
	sr_data.mode		= 0x00;
	sr_data.resp_time	= 0x00;

	return make_pwr_c_packet( &sr_data, sts_send_buf );
}

static int proc_pwr_c_ctrl_req( UINT16 msg, void * ptrdata )
{
	sr_data_t sr_data;
	static BYTE ctrl_send_buf[ MAX_SR_DEF_DATA_SIZE ];
	pwr_c_data_t pwr_c_data; 
	BYTE val =0;
	int i;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}
	
	memset( &sr_data, 0, sizeof( sr_data ));
	memset( &ctrl_send_buf, 0, sizeof( ctrl_send_buf ));
	memset( &pwr_c_data, 0, sizeof( pwr_c_data ));

	if ( g_ptrctrl_sr != NULL )
	{
		sr_data.ptrdata 	= g_ptrctrl_sr;
		memset( sr_data.ptrdata , 0,  MAX_SR_DEF_DATA_SIZE );
	}

	memcpy(&pwr_c_data, ptrdata, sizeof( pwr_c_data ));

	sr_data.len 		= 0x01;
	sr_data.cmd 		= PWR_C_CTRL;
	sr_data.dev_id		= DEV_ID_PWR_C;
	sr_data.dev_addr	= 0x00;
	sr_data.dev_ch		= 0x00;
	sr_data.mode		= 0x00;
	sr_data.resp_time	= 0x00;

	/* PWR VAL */
	val = 0;
	for( i = ( MAX_PWR_C_PORT_CNT -1 ) ; i >0 ; i-- )
	{
		val |= ( pwr_c_data.val[ i ] << i );
	}
	val |= ( pwr_c_data.val[ 0 ] );

	sr_data.ptrdata[0] = val; 
	
	return make_pwr_c_packet( &sr_data, ctrl_send_buf );
}

static int parse_pwr_c_comm_packet( BYTE cmd, void * ptrdata )
{
	
	sr_data_t * ptrsr = NULL;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrsr = (sr_data_t *)ptrdata;

	//print_dbg(DBG_NONE, 1,"Recv PWR_C COMMON Packet");

	check_pwr_c_sr_restore_timeout(ptrsr->cmd );

#if 0
	print_dbg(DBG_NONE,1,"len:%x, cmd:%x, dev_id:%x, dev_addr:%x, dev_ch:%x, mode:%x, req_time:%x, check_sum:%x",
			  ptrsr->len, ptrsr->cmd, ptrsr->dev_id, ptrsr->dev_addr, ptrsr->dev_ch, ptrsr->mode, ptrsr->resp_time , ptrsr->check_sum );
#endif

	set_pwr_c_ver_ctrl( (BYTE *)ptrsr->ptrdata);

	return ERR_SUCCESS;
}

static int parse_pwr_c_status_packet( BYTE cmd, void * ptrdata )
{
	sr_data_t * ptrsr = NULL;
	UINT32 uval1, uval2, uval3, uval4, uval5, uval6;
	BYTE pwr_data = 0;
	pwr_c_data_t pwr_c_data;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	uval1 = uval2 = uval3 = uval4 = uval5 = uval6 =0;
	memset( &pwr_c_data, 0, sizeof( pwr_c_data ));

	ptrsr = (sr_data_t *)ptrdata;

	//print_dbg(DBG_NONE, 1,"Recv PWR_C Status Packet");

	check_pwr_c_sr_restore_timeout(ptrsr->cmd );
#if 0
	print_dbg(DBG_NONE,1,"len:%x, cmd:%x, dev_id:%x, dev_addr:%x, dev_ch:%x, mode:%x, req_time:%x, check_sum:%x",
			  ptrsr->len, ptrsr->cmd, ptrsr->dev_id, ptrsr->dev_addr, ptrsr->dev_ch, ptrsr->mode, ptrsr->resp_time , ptrsr->check_sum );
#endif
	memcpy( &uval1, ptrsr->ptrdata, sizeof(int));
	ptrsr->ptrdata +=4;

	memcpy( &uval2, ptrsr->ptrdata ,sizeof(int));
	ptrsr->ptrdata +=4;
	memcpy( &uval3, ptrsr->ptrdata, sizeof(int));
	ptrsr->ptrdata +=4;
	memcpy( &uval4, ptrsr->ptrdata, sizeof(int));
	ptrsr->ptrdata +=4;
	memcpy( &uval5, ptrsr->ptrdata, sizeof(int));
	ptrsr->ptrdata +=4;
	memcpy( &uval6, ptrsr->ptrdata, sizeof(int));
	ptrsr->ptrdata +=4;

	pwr_data = *ptrsr->ptrdata;

	pwr_c_data.uval[ 0 ] = ntohl( uval1);
	pwr_c_data.uval[ 1 ] = ntohl( uval2);
	pwr_c_data.uval[ 2 ] = ntohl( uval3);
	pwr_c_data.uval[ 3 ] = ntohl( uval4);
	pwr_c_data.uval[ 4 ] = ntohl( uval5);
	pwr_c_data.uval[ 5 ] = ntohl( uval6);

	pwr_c_data.val[ 0 ] = ((pwr_data & 0x01)> 0 ? 1: 0);
	pwr_c_data.val[ 1 ] = ((pwr_data & 0x02)> 0 ? 1: 0);
	pwr_c_data.val[ 2 ] = ((pwr_data & 0x04)> 0 ? 1: 0);
	pwr_c_data.val[ 3 ] = ((pwr_data & 0x08)> 0 ? 1: 0);
	pwr_c_data.val[ 4 ] = ((pwr_data & 0x10)> 0 ? 1: 0);
	pwr_c_data.val[ 5 ] = ((pwr_data & 0x20)> 0 ? 1: 0 );

	set_pwr_c_data_ctrl( &pwr_c_data );

	return ERR_SUCCESS;
}

static int parse_pwr_c_ctrl_packet( BYTE cmd, void * ptrdata )
{
	sr_data_t * ptrsr = NULL;
	BYTE result_val[ 2 ];
	result_t result;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	memset(&result, 0, sizeof( result));
	memset(&result_val, 0, sizeof( result_val ));

	ptrsr = (sr_data_t *)ptrdata;

	//print_dbg(DBG_NONE, 1,"Recv PWR_C Contorl Packet");

	check_pwr_c_sr_restore_timeout(ptrsr->cmd );

#if 0
	print_dbg(DBG_NONE,1,"len:%x, cmd:%x, dev_id:%x, dev_addr:%x, dev_ch:%x, mode:%x, req_time:%x, check_sum:%x",
			  ptrsr->len, ptrsr->cmd, ptrsr->dev_id, ptrsr->dev_addr, ptrsr->dev_ch, ptrsr->mode, ptrsr->resp_time , ptrsr->check_sum );
#endif
	
	result.clientid = g_clientid;
	result.sts 		= ESNSRCNNST_NORMAL;
	
	result_val[0] = *ptrsr->ptrdata & 0xFF;

	set_pwr_c_onoff_result_ctrl( &result, result_val );

	return ERR_SUCCESS;
}

static int make_pwr_c_packet( sr_data_t * ptrsr, BYTE * ptrsend )
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
		write_pwr_c( t_size, (char*)ptrsend , ptrsr->cmd ); 
	}

	return ERR_RETURN;
}

/*================================================================================================
내부 함수  정의 
매개변수가 pointer일경우 반드시 NULL 체크 적용 
================================================================================================*/
static int parse_pwr_c_cmd( BYTE cmd, void * ptrdata )
{
	int i,cnt = 0;
	parse tmp_parse = NULL;
	BYTE l_cmd = 0;

	cnt = ( sizeof( g_parse_pwr_c_func_list ) / sizeof( parse_cmd_func_t ));

	for( i = 0 ; i < cnt ; i++ )
	{
		tmp_parse 	= g_parse_pwr_c_func_list[ i ].parse_func ;
		l_cmd 		= g_parse_pwr_c_func_list[ i ].cmd; 

		if ( tmp_parse!= NULL && cmd == l_cmd )
		{
			tmp_parse( cmd, ptrdata );
		}
	}

	return ERR_SUCCESS;	
}

static int write_pwr_c( int len, char * ptrdata, BYTE cmd )
{
	int ret = ERR_SUCCESS;

	if( g_pwr_c_fd < 0 )
	{
		print_dbg( DBG_ERR,1, "Can not Write PWR_C Serial, fd:%d", g_pwr_c_fd );
		return ERR_RETURN;
	}
	if ( ptrdata == NULL || len ==  0 )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* lock */
	lock_sem( g_pwr_c_sem_id, g_pwr_c_sem_key );
	ret = write_serial_com_sns( g_pwr_c_fd, ptrdata, len );
	unlock_sem( g_pwr_c_sem_id, g_pwr_c_sem_key );
	/* unlock */
	if ( ret <= 0 )
	{
		print_dbg( DBG_ERR,1, "Can not Write PWR_C Serial, fd:%d, len:%d", g_pwr_c_fd, len );
	}
	else
	{
		/* send list buf에 등록 */
		lock_sem( g_pwr_c_sem_id, g_pwr_c_sem_key );
		add_send_list_com_sns( cmd, &g_send_sr_list, ret, ptrdata , DEV_ID_PWR_C );
		unlock_sem( g_pwr_c_sem_id, g_pwr_c_sem_key );

	}
	return ret;
}

static int read_pwr_c( void )
{

	int size ;
	char * ptrrecv= NULL;
	sr_data_t sr_data;

	if ( g_pwr_c_fd > 0 )
	{
		ptrrecv = g_pwr_c_queue.ptrrecv_buf;

		if ( ptrrecv != NULL )
		{
			size =0;
			memset( ptrrecv, 0, MAX_SR_RECV_SIZE );
			size = read( g_pwr_c_fd , ptrrecv, MAX_SR_RECV_SIZE  );

			if ( size > 0 )
			{
				memset( &sr_data, 0, sizeof( sr_data ));
				if ( g_ptrread_sr != NULL )
				{
					sr_data.ptrdata 	= g_ptrread_sr;
					memset( sr_data.ptrdata , 0,  MAX_SR_DEF_DATA_SIZE );
				}

				//print_buf( size, (BYTE*)ptrrecv );
				parse_serial_data_com_sns( g_pwr_c_fd, size, &g_pwr_c_queue, parse_pwr_c_cmd, PWR_C_NAME, &sr_data );
				//g_last_read_time = time( NULL );
			}
			//print_buf(size, (BYTE*)ptrrecv );
			//print_dbg( DBG_INFO,1,"size:%d, %s", size, ptrrecv);
		}
		else
		{
			print_dbg( DBG_ERR, 1, "Not have pwr_c recv buffer");
		}
	}

	return ERR_SUCCESS;
}

static int create_pwr_c_thread( void )
{
	int ret = ERR_SUCCESS;

	if ( g_release == 0 && g_start_pwr_c_pthread == 0 && g_end_pwr_c_pthread == 1 )
	{
		g_start_pwr_c_pthread = 1;

		if ( pthread_create( &pwr_c_pthread, NULL, pwr_c_pthread_idle , NULL ) < 0 )
		{
			g_start_pwr_c_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create PWR_C Thread" );
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
static int release_pwr_c_thread( void )
{
	int status = 0;
	
	if ( g_end_pwr_c_pthread == 0 )
	{
		pthread_join( pwr_c_pthread , ( void **)&status );
		g_start_pwr_c_pthread 	= 0;
		g_end_pwr_c_pthread 		= 1;
	}

	return ERR_SUCCESS;
}
#endif

static void *  pwr_c_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;

	g_end_pwr_c_pthread = 0;

	while( g_release == 0 && g_start_pwr_c_pthread == 1 )
	{
		read_pwr_c();
	}

	if( g_end_pwr_c_pthread == 0 )
	{
		pthread_join( pwr_c_pthread , ( void **)&status );
		g_start_pwr_c_pthread 	= 0;
		g_end_pwr_c_pthread 		= 1;
	}

	return ERR_SUCCESS;
}

static int set_pwr_c_fd( void )
{
	int fd = -1;

	if ( g_pwr_c_fd >= 0 )
		return ERR_SUCCESS;

	fd = set_serial_fd_com_sns( PWR_C_BAUD, PWR_C_TTY, PWR_C_NAME );

	if ( fd >= 0 )
	{
		g_pwr_c_fd = fd;
	}
	else
	{
		set_pwr_c_trans_sts_ctrl( ESNSRCNNST_ERROR );
		return ERR_RETURN;
	}

	return ERR_SUCCESS;
}

static int release_pwr_c_fd( void )
{
	if ( g_pwr_c_fd >= 0 )
	{
		close ( g_pwr_c_fd );
		g_pwr_c_fd = -1;
	}

	return ERR_SUCCESS;
}

/* CONTROL module에  보낼 데이터(제어, 설정, 알람)가  있을 경우 호출 */
static int send_pwr_c_data_control( UINT16 inter_msg, void * ptrdata  )
{
	if ( g_ptrsend_pwr_c_data_control_func != NULL )
	{
		return g_ptrsend_pwr_c_data_control_func( inter_msg, ptrdata );
	}

	return ERR_RETURN;
}

/* CONTROL Module로 부터 수집 데이터, 알람, 장애  정보를 받는 내부 함수로
   등록된 proc function list 중에서 control에서  넘겨 받은 inter_msg와 동일한 msg가 존재할 경우
   해당  함수를 호출하도록 한다. 
 */
static int internal_pwr_c_recv_ctrl_data( unsigned short inter_msg, void * ptrdata )
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
	proc_func_cnt= ( sizeof( g_proc_pwr_c_func_list ) / sizeof( proc_func_t ));

	/* 등록된 proc function에서 inter_msg가 동일한 function 찾기 */
	for( i = 0 ; i < proc_func_cnt ; i++ )
	{    
		list_msg 			= g_proc_pwr_c_func_list[ i ].inter_msg; 	/* function list에 등록된 msg id */
		ptrtemp_proc_func 	= g_proc_pwr_c_func_list[ i ].ptrproc;	/* function list에 등록된 function point */

		if ( ptrtemp_proc_func != NULL && list_msg == inter_msg )
		{    
			ptrtemp_proc_func( inter_msg, ptrdata );
			break;
		}    
	} 

	return ERR_SUCCESS;
}

/* pwr_c module 초기화 내부 작업 */
static int internal_init( void )
{
	int i,ret = ERR_SUCCESS;

	/* warring을 없애기 위한 함수 호출 */
	send_pwr_c_data_control( 0, NULL );

	ret = set_pwr_c_fd();
	
	if ( ret == ERR_SUCCESS )
	{
		ret = create_pwr_c_thread( );
	}

	memset( &g_pwr_c_queue, 0, sizeof( g_pwr_c_queue ));
	memset( &g_send_sr_list, 0,sizeof( g_send_sr_list ));

	/* buf 생성 */
	g_pwr_c_queue.ptrlist_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_QUEUE_SIZE );
	g_pwr_c_queue.ptrrecv_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_RECV_SIZE );	
	g_pwr_c_queue.ptrsend_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_SEND_SIZE );	

	g_ptrread_sr			= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );
	g_ptrcom_sr				= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );
	g_ptrsts_sr				= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );	
	g_ptrctrl_sr			= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );	

	if( g_pwr_c_queue.ptrlist_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create PWR_C SR List Buffer");
		ret = ERR_RETURN;
	}

	if( g_pwr_c_queue.ptrrecv_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create PWR_C SR Recv Buffer");
		ret = ERR_RETURN;
	}

	if( g_pwr_c_queue.ptrsend_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create PWR_C SR Send Buffer");
		ret = ERR_RETURN;
	}

	if ( ret == ERR_SUCCESS )
	{
		print_dbg(DBG_INFO, 1, "Success of PWR_C Module Initialize");
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
			print_dbg(DBG_ERR, 1, "Can not Create PWR_C SR Send List Buffer");
			ret = ERR_RETURN;
		}
		else
		{
			memset( g_send_sr_list.send_data[ i ].ptrdata, 0, MAX_SR_DEF_DATA_SIZE );
		}
	}

	g_pwr_c_sem_id = create_sem( g_pwr_c_sem_key );
	print_dbg( DBG_INFO, 1, " PWR_C SEMA : %d", g_pwr_c_sem_id );

	sys_sleep( 2000 );
	proc_pwr_c_com_req( 0, NULL );

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

static int idle_pwr_c_func( void )
{
	time_t cur_time;
	static time_t pre_time = 0;
	
	cur_time = time( NULL );
	
	if( pre_time == 0 ) 
		pre_time = cur_time;

	if ( cur_time - pre_time > TEST_INTVAL )
	{
		//print_dbg( DBG_LINE,1,NULL );

		pre_time = cur_time;
		//proc_pwr_c_com_req( 0, NULL );
		//sys_sleep( 2000 );
		proc_pwr_c_status_req( 0 , NULL );
		//print_dbg( DBG_INFO,1, "SEND INFO CMD");
	}
	
	check_pwr_c_sr_timeout( );

	return ERR_SUCCESS;
}

/* pwr_c module loop idle 내부 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
static int internal_idle( void * ptr )
{
	( void )ptr;

	if( g_release == 0 )
	{
		create_pwr_c_thread();
		set_pwr_c_fd( );

		idle_pwr_c_func();
	}
	return ERR_SUCCESS;
}

static int release_buf ( void )
{
	int i = 0;

	if ( g_pwr_c_queue.ptrlist_buf != NULL )
	{
		free( g_pwr_c_queue.ptrlist_buf );
		g_pwr_c_queue.ptrlist_buf  = NULL;
	}

	if ( g_pwr_c_queue.ptrrecv_buf != NULL )
	{
		free( g_pwr_c_queue.ptrrecv_buf );
		g_pwr_c_queue.ptrrecv_buf  = NULL;
	}

	if ( g_pwr_c_queue.ptrsend_buf != NULL )
	{
		free( g_pwr_c_queue.ptrsend_buf );
		g_pwr_c_queue.ptrsend_buf  = NULL;
	}

	if( g_ptrread_sr != NULL )
	{ 
		free( g_ptrread_sr  );
		g_ptrread_sr  = NULL ;
	}
	if( g_ptrcom_sr != NULL )
	{ 
		free( g_ptrcom_sr  );
		g_ptrcom_sr  = NULL ;
	}
	if( g_ptrsts_sr != NULL )
	{ 
		free( g_ptrsts_sr );
		g_ptrsts_sr  = NULL ;
	}
	if( g_ptrctrl_sr != NULL )
	{ 
		free( g_ptrctrl_sr );
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
	if ( g_pwr_c_sem_id > 0 )
	{
		destroy_sem( g_pwr_c_sem_id, g_pwr_c_sem_key );
		g_pwr_c_sem_id = -1;
	}

	return ERR_SUCCESS;
}

/* pwr_c module의 release 내부 작업 */
static int internal_release( void )
{
	g_release = 1;
	
	release_pwr_c_fd();
	//release_pwr_c_thread();

	print_dbg(DBG_INFO, 1, "Release of PWR_C Module");
	return ERR_SUCCESS;
}

/*================================================================================================
외부  함수  정의 
================================================================================================*/
int init_pwr_c( send_data_func ptrsend_data )
{
	/* CONTROL module의 함수 포인트 등록 */
	g_ptrsend_pwr_c_data_control_func = ptrsend_data ;	/* CONTROL module로 센서 , 장애  데이터를 보낼 수 있는 함수 포인터 */

	/* 기타 초기화 작업 */
	internal_init();

	return ERR_SUCCESS;
}

/* pwr_c module loop idle 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
int idle_pwr_c( void * ptr )
{
	return internal_idle( ptr );
}

/* 동적 메모리, fd 등 resouce release함수 */
int release_pwr_c( void )
{
	/* release 작업  */
	return internal_release(); 
}

/* CONTROL Module로 부터 제어, 설정, update 정보를 받는 외부 함수 */
int recv_ctrl_data_pwr_c( unsigned short inter_msg, void * ptrdata )
{
	return internal_pwr_c_recv_ctrl_data( inter_msg, ptrdata );
}
