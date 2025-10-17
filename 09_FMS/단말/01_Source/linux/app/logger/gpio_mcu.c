/*********************************************************************
	file : mcu.c
	description :
	product by tory45( tory45@empal.com) ( 2016.08.05 )
	update by ?? ( . . . )
	
	- Control에서 fms_server로 보낼 데이터를 받아 fms_server로 보내는 기능
	- fms_server에서 데이터를 받아 Control로 정보를 보내는 기능 
 ********************************************************************/
#include "com_include.h"
#include "sem.h"
#include "com_sns.h"

#include "gpio_mcu.h"
#include "control.h"

#define MCU_COM		0x00
#define MCU_STS		0x01
#define MCU_CTRL	0x02

/*================================================================================================
 전역 변수 
 1.static 적용 
 2.숫자형 데이터 일경우 volatile 적용 
 3.초기값 적용.
================================================================================================*/
static volatile int g_mcu_sem_key 						= MCU_SEM_KEY;
static volatile int g_mcu_sem_id 						= 0;


static send_data_func  g_ptrsend_mcu_data_control_func = NULL;		/* CONTROL 모듈  센서 데이터 ,  알람  정보를 넘기는 함수 포인트 */
static sr_queue_t g_mcu_queue;
static BYTE g_bit_val[ MAX_BIT_CNT ];
static send_sr_list_t g_send_sr_list;

static volatile int g_clientid 				= 0;

/* thread ( mcu )*/
static volatile int g_start_mcu_pthread 	= 0; 
static volatile int g_end_mcu_pthread 		= 1; 
static pthread_t mcu_pthread;
static void * mcu_pthread_idle( void * ptrdata );
static volatile int g_release				= 0;

static char* g_ptrread_sr					= NULL;
static char * g_ptrcom_sr					= NULL;
static char * g_ptrsts_sr					= NULL;
static char * g_ptrctrl_sr					= NULL;

/* serial */
static int set_mcu_fd( void );
static int release_mcu_fd( void );
static volatile int g_mcu_fd 				= -1;

/*================================================================================================
  내부 함수 선언
  static 적용 
================================================================================================*/
static int send_mcu_data_control( UINT16 inter_msg, void * ptrdata );

static int internal_init( void );
static int internal_idle( void * ptr );
static int internal_release( void );
static int create_mcu_thread( void );

static int release_buf( void );
static int write_mcu( int len, char * ptrdata , BYTE cmd);

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

static int parse_mcu_comm_packet( BYTE cmd, void * ptrdata );
static int parse_mcu_status_packet( BYTE cmd, void * ptrdata );
static int parse_mcu_ctrl_packet( BYTE cmd, void * ptrdata );

static int make_mcu_packet( sr_data_t * ptrsr, BYTE * ptrsend  );

static int proc_mcu_com_req( UINT16 msg, void * ptrdata );
static int proc_mcu_status_req( UINT16 msg, void * ptrdata );
static int proc_mcu_ctrl_req( UINT16 msg, void * ptrdata );


static proc_func_t g_proc_mcu_func_list []= 
{
	/* inter msg id   		,  			proc function point */
	/*=============================================*/
	{ EFT_SNSRCTL_POWER, proc_mcu_ctrl_req },
	{ 0, NULL }
};

static parse_cmd_func_t g_parse_mcu_func_list[ ] = 
{
	{ MCU_COM, 	parse_mcu_comm_packet },
	{ MCU_STS, 	parse_mcu_status_packet },
	{ MCU_CTRL, parse_mcu_ctrl_packet },
	{ 0xFF	, 	NULL }
};

static int check_mcu_sr_timeout( void )
{
	int ret = ERR_SUCCESS;
	int resend_pos = -1;
	char * ptrdata = NULL;
	BYTE cmd;
	int len = 0;
	result_t result;
	BYTE result_val[ 4 ];

	lock_sem( g_mcu_sem_id, g_mcu_sem_key );
	ret = chk_send_list_timeout_com_sns( &g_send_sr_list , &resend_pos, DEV_ID_MCU );
	unlock_sem( g_mcu_sem_id, g_mcu_sem_key );

	if ( ret == RET_TIMEOUT )
	{
		memset( &result, 0, sizeof( result ));
		memset( result_val, 0, sizeof( result_val ));

		set_gpio_trans_sts_ctrl( ESNSRCNNST_ERROR );
		result.clientid = g_clientid;
		result.sts		= ESNSRCNNST_ERROR;

		set_gpio_do_onoff_result_ctrl( &result, result_val );
	}
	else if( ret == RET_RESEND )
	{

		if( resend_pos >=0 )
		{
			cmd 	= g_send_sr_list.send_data[ resend_pos ].cmd;
			len		= g_send_sr_list.send_data[ resend_pos ].len;
			ptrdata = g_send_sr_list.send_data[ resend_pos ].ptrdata;

			write_mcu( len, ptrdata, cmd );

			print_dbg( DBG_INFO, 1, "Resend MCU SR Data cmd:%d, len:%d", cmd, len );

		}
	}
	return ERR_SUCCESS;
}

static int check_mcu_sr_restore_timeout( BYTE cmd )
{
	int ret = 0;

	lock_sem( g_mcu_sem_id, g_mcu_sem_key );
	ret = del_send_list_com_sns( cmd, &g_send_sr_list, DEV_ID_MCU );
	unlock_sem( g_mcu_sem_id, g_mcu_sem_key );
		
	/* 0: 2초이내 응답이 온 경우, 1: 응답은왔으나 2초 이내가 아닐 경우 */
	if ( ret == 0 )
	{
		set_gpio_trans_sts_ctrl( ESNSRCNNST_NORMAL );
	}

	return ERR_SUCCESS;
}

static int proc_mcu_com_req( UINT16 msg, void * ptrdata )
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
	sr_data.cmd 		= MCU_COM;
	sr_data.dev_id		= 0x00;
	sr_data.dev_addr	= 0x00;
	sr_data.dev_ch		= 0x00;
	sr_data.mode		= 0x00;
	sr_data.resp_time	= 0x00;

	return make_mcu_packet( &sr_data , com_send_buf );
}

static int proc_mcu_status_req( UINT16 msg, void * ptrdata )
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
	sr_data.cmd 		= MCU_STS;
	sr_data.dev_id		= DEV_ID_MCU;
	sr_data.dev_addr	= 0x00;
	sr_data.dev_ch		= 0x00;
	sr_data.mode		= 0x00;
	sr_data.resp_time	= 0x00;

	return make_mcu_packet( &sr_data, sts_send_buf );
}


static int proc_mcu_ctrl_req( UINT16 msg, void * ptrdata )
{
	sr_data_t sr_data;
	static BYTE ctrl_send_buf[ MAX_SR_DEF_DATA_SIZE ];
	mcu_data_t mcu_data;
	BYTE val;
	int i;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}

	memset(&mcu_data, 0, sizeof( mcu_data ));
	memset( &sr_data, 0, sizeof( sr_data ));
	memset(&ctrl_send_buf, 0, sizeof( ctrl_send_buf ));

	if ( g_ptrctrl_sr != NULL )
	{
		sr_data.ptrdata 	= g_ptrctrl_sr;
		memset( sr_data.ptrdata , 0,  MAX_SR_DEF_DATA_SIZE );
	}

	memcpy(&mcu_data, ptrdata, sizeof( mcu_data ));

	sr_data.len 		= 0x02;
	sr_data.cmd 		= MCU_CTRL;
	sr_data.dev_id		= DEV_ID_MCU;
	sr_data.dev_addr	= 0x00;
	sr_data.dev_ch		= 0x00;
	sr_data.mode		= 0x00;
	sr_data.resp_time	= 0x00;

	/* DO VAL */
	val = 0;
	for( i = ( MAX_GPIO_CNT -1 ) ; i >0 ; i-- )
	{
		val |= ( mcu_data.do_[ i ] << i );
	}
	val |= ( mcu_data.do_[ 0 ] );

	sr_data.ptrdata[0] = val; 
	
	/* DOE VAL */
	val = 0;
	for( i = ( MAX_GPIO_CNT -1 ) ; i >0 ; i-- )
	{
		val |= ( mcu_data.doe[ i ] << i );
	}
	val |= ( mcu_data.doe[ 0 ] );
	sr_data.ptrdata[1] = val;

	//print_dbg( DBG_INFO,1,"RECV MCU DO_DOE :%x, %x", sr_data.ptrdata[0],sr_data.ptrdata[1] );
	//print_dbg( DBG_NONE, 1, ".................VAL....:%x" , val );
	return make_mcu_packet( &sr_data, ctrl_send_buf );
}

static int parse_mcu_comm_packet( BYTE cmd, void * ptrdata )
{
	
	sr_data_t * ptrsr = NULL;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrsr = (sr_data_t *)ptrdata;

	//print_dbg(DBG_NONE, 1,"Recv MCU COMMON Packet");

	check_mcu_sr_restore_timeout(ptrsr->cmd );

	set_gpio_ver_ctrl( (BYTE*)ptrsr->ptrdata );

	return ERR_SUCCESS;
}

/* 값을 저장하여 모니터링 또는 실시간 데이터로 전송한다.. */
static int parse_mcu_status_packet( BYTE cmd, void * ptrdata )
{
	sr_data_t * ptrsr = NULL;
	mcu_data_t mcu_data;
	BYTE val;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrsr = (sr_data_t *)ptrdata;

	//print_dbg(DBG_NONE, 1,"Recv MCU Status Packet");

	check_mcu_sr_restore_timeout(ptrsr->cmd );
#if 0
	print_dbg(DBG_NONE,1,"len:%x, cmd:%x, dev_id:%x, dev_addr:%x, dev_ch:%x, mode:%x, req_time:%x, check_sum:%x",
			  ptrsr->len, ptrsr->cmd, ptrsr->dev_id, ptrsr->dev_addr, ptrsr->dev_ch, ptrsr->mode, ptrsr->resp_time , ptrsr->check_sum );
#endif
	memset( &mcu_data, 0, sizeof( mcu_data ));

	val = *ptrsr->ptrdata++;
	mcu_data.di[ 0 ] = ((val & 0x01 )> 0 ? 1: 0 );
	mcu_data.di[ 1 ] = ((val & 0x02 )> 0 ? 1: 0 );
	mcu_data.di[ 2 ] = ((val & 0x04 )> 0 ? 1: 0 );
	mcu_data.di[ 3 ] = ((val & 0x08 )> 0 ? 1: 0 );

	mcu_data.di[ 4 ] = ((val & 0x10 )> 0 ? 1: 0 );
	/*반전 출입문 : 0 열림, 1: 닫임 */
#if 0
	if (mcu_data.di[ 4 ] == 0 )
	{
		mcu_data.di[ 4 ] = 1;
	}
	else
	{
		mcu_data.di[ 4 ] = 0;
	}
#endif
	mcu_data.di[ 5 ] = ((val & 0x20 )> 0 ? 1: 0 );
	mcu_data.di[ 6 ] = ((val & 0x40 )> 0 ? 1: 0 );
	mcu_data.di[ 7 ] = ((val & 0x80 )> 0 ? 1: 0 );

	val = *ptrsr->ptrdata++;
	mcu_data.do_[ 0 ] = ((val & 0x01 )> 0 ? 1: 0 );
	mcu_data.do_[ 1 ] = ((val & 0x02 )> 0 ? 1: 0 );
	mcu_data.do_[ 2 ] = ((val & 0x04 )> 0 ? 1: 0 );
	mcu_data.do_[ 3 ] = ((val & 0x08 )> 0 ? 1: 0 );
	mcu_data.do_[ 4 ] = ((val & 0x10 )> 0 ? 1: 0 );
	mcu_data.do_[ 5 ] = ((val & 0x20 )> 0 ? 1: 0 );
	mcu_data.do_[ 6 ] = ((val & 0x40 )> 0 ? 1: 0 );
	mcu_data.do_[ 7 ] = ((val & 0x80 )> 0 ? 1: 0 );

	val = *ptrsr->ptrdata++;
	mcu_data.doe[ 0 ] = ((val & 0x01 )> 0 ? 1: 0 );
	mcu_data.doe[ 1 ] = ((val & 0x02 )> 0 ? 1: 0 );
	mcu_data.doe[ 2 ] = ((val & 0x04 )> 0 ? 1: 0 );
	mcu_data.doe[ 3 ] = ((val & 0x08 )> 0 ? 1: 0 );
	mcu_data.doe[ 4 ] = ((val & 0x10 )> 0 ? 1: 0 );
	mcu_data.doe[ 5 ] = ((val & 0x20 )> 0 ? 1: 0 );
	mcu_data.doe[ 6 ] = ((val & 0x40 )> 0 ? 1: 0 );
	mcu_data.doe[ 7 ] = ((val & 0x80 )> 0 ? 1: 0 );
#if 0
	static int flag = 0;
	
	if ( flag == 0 )
	{
		flag = 1;
	}
	else
	{
		flag = 0;
	}
	mcu_data.di[ 0 ] = flag; 
	mcu_data.di[ 1 ] = flag;
	mcu_data.di[ 2 ] = flag;
	mcu_data.di[ 3 ] = flag;
	mcu_data.di[ 4 ] = flag;
	mcu_data.di[ 5 ] = flag;
	mcu_data.di[ 6 ] = flag;
	mcu_data.di[ 7 ] = flag;
#endif
	set_gpio_data_ctrl( &mcu_data );

	return ERR_SUCCESS;
}

static int parse_mcu_ctrl_packet( BYTE cmd, void * ptrdata )
{
	sr_data_t * ptrsr = NULL;
	BYTE result_val[ 4 ];

	result_t result;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	memset(&result, 0, sizeof( result));
	memset(&result_val, 0, sizeof( result_val ));

	ptrsr = (sr_data_t *)ptrdata;

	//print_dbg(DBG_NONE, 1,"Recv MCU Contorl Packet");

	check_mcu_sr_restore_timeout(ptrsr->cmd );
#if 0
	print_dbg(DBG_NONE,1,"len:%x, cmd:%x, dev_id:%x, dev_addr:%x, dev_ch:%x, mode:%x, req_time:%x, check_sum:%x",
			  ptrsr->len, ptrsr->cmd, ptrsr->dev_id, ptrsr->dev_addr, ptrsr->dev_ch, ptrsr->mode, ptrsr->resp_time , ptrsr->check_sum );
#endif
	result_val[ 0 ] = *ptrsr->ptrdata++;
	result_val[ 1 ] = *ptrsr->ptrdata++;
	result_val[ 2 ] = *ptrsr->ptrdata++;
	result_val[ 3 ] = *ptrsr->ptrdata++;
	
	result.clientid = g_clientid;
	result.sts		= ESNSRCNNST_NORMAL;

	set_gpio_do_onoff_result_ctrl( &result, result_val );

	return ERR_SUCCESS;
}

static int make_mcu_packet( sr_data_t * ptrsr, BYTE * ptrsend )
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
		write_mcu( t_size, (char*)ptrsend , ptrsr->cmd ); 
	}

	return ERR_RETURN;
}

/*================================================================================================
내부 함수  정의 
매개변수가 pointer일경우 반드시 NULL 체크 적용 
================================================================================================*/
static int parse_mcu_cmd( BYTE cmd, void * ptrdata )
{
	int i,cnt = 0;
	parse tmp_parse = NULL;
	BYTE l_cmd = 0;

	cnt = ( sizeof( g_parse_mcu_func_list ) / sizeof( parse_cmd_func_t ));

	for( i = 0 ; i < cnt ; i++ )
	{
		tmp_parse 	= g_parse_mcu_func_list[ i ].parse_func ;
		l_cmd 		= g_parse_mcu_func_list[ i ].cmd; 

		if ( tmp_parse!= NULL && cmd == l_cmd )
		{
			tmp_parse( cmd, ptrdata );
		}
	}

	return ERR_SUCCESS;	
}

static int write_mcu( int len, char * ptrdata, BYTE cmd )
{
	int ret = ERR_SUCCESS;

	if( g_mcu_fd < 0 )
	{
		print_dbg( DBG_ERR,1, "Can not Write MCU Serial, fd:%d", g_mcu_fd );
		return ERR_RETURN;
	}
	
	if ( ptrdata == NULL || len ==  0 )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* lock */
	lock_sem( g_mcu_sem_id, g_mcu_sem_key );
	ret = write_serial_com_sns( g_mcu_fd, ptrdata, len );
	unlock_sem( g_mcu_sem_id, g_mcu_sem_key );
	/* unlock */
	if ( ret <= 0 )
	{
		print_dbg( DBG_ERR,1, "Can not Write MCU Serial, fd:%d, len:%d", g_mcu_fd, len );
	}
	else
	{
		/* send list buf에 등록 */
		lock_sem( g_mcu_sem_id, g_mcu_sem_key );
		add_send_list_com_sns( cmd, &g_send_sr_list, ret, ptrdata , DEV_ID_MCU );
		unlock_sem( g_mcu_sem_id, g_mcu_sem_key );

	}
	return ret;
}

static int read_mcu( void )
{

	int size ;
	char * ptrrecv= NULL;
	sr_data_t sr_data;

	if ( g_mcu_fd > 0 )
	{
		ptrrecv = g_mcu_queue.ptrrecv_buf;

		if ( ptrrecv != NULL )
		{
			size =0;
			memset( ptrrecv, 0, MAX_SR_RECV_SIZE );
			size = read( g_mcu_fd , ptrrecv, MAX_SR_RECV_SIZE  );

			if ( size > 0 )
			{
				memset( &sr_data, 0, sizeof( sr_data ));

				if ( g_ptrread_sr != NULL )
				{
					sr_data.ptrdata 	= g_ptrread_sr;
					memset( sr_data.ptrdata , 0,  MAX_SR_DEF_DATA_SIZE );
				}

				//print_buf( size, (BYTE*)ptrrecv );
				parse_serial_data_com_sns( g_mcu_fd, size, &g_mcu_queue, parse_mcu_cmd, MCU_NAME, &sr_data );
				//g_last_read_time = time( NULL );
			}
			//print_buf(size, (BYTE*)ptrrecv );
			//print_dbg( DBG_INFO,1,"size:%d, %s", size, ptrrecv);
		}
		else
		{
			print_dbg( DBG_ERR, 1, "Not have mcu recv buffer");
		}
	}

	return ERR_SUCCESS;
}

static int create_mcu_thread( void )
{
	int ret = ERR_SUCCESS;

	if ( g_release == 0 && g_start_mcu_pthread == 0 && g_end_mcu_pthread == 1 )
	{
		g_start_mcu_pthread = 1;

		if ( pthread_create( &mcu_pthread, NULL, mcu_pthread_idle , NULL ) < 0 )
		{
			g_start_mcu_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create MCU Thread" );
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
static int release_mcu_thread( void )
{
	int status = 0;
	
	if ( g_end_mcu_pthread == 0 )
	{
		pthread_join( mcu_pthread , ( void **)&status );
		g_start_mcu_pthread 	= 0;
		g_end_mcu_pthread 		= 1;
	}

	return ERR_SUCCESS;
}
#endif

static void *  mcu_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;

	g_end_mcu_pthread = 0;

	while( g_release == 0 && g_start_mcu_pthread == 1 )
	{
		read_mcu();
	}

	if( g_end_mcu_pthread == 0 )
	{
		pthread_join( mcu_pthread , ( void **)&status );
		g_start_mcu_pthread 	= 0;
		g_end_mcu_pthread 		= 1;
	}

	return ERR_SUCCESS;
}

static int set_mcu_fd( void )
{
	int fd = -1;

	if ( g_mcu_fd >= 0 )
		return ERR_SUCCESS;

	fd = set_serial_fd_com_sns( MCU_BAUD, MCU_TTY, MCU_NAME );

	if ( fd >= 0 )
	{
		g_mcu_fd = fd;
	}
	else
	{
		set_gpio_trans_sts_ctrl( ESNSRCNNST_ERROR );
		return ERR_RETURN;
	}

	return ERR_SUCCESS;
}

static int release_mcu_fd( void )
{
	if ( g_mcu_fd >= 0 )
	{
		close ( g_mcu_fd );
		g_mcu_fd = -1;
	}

	return ERR_SUCCESS;
}

/* CONTROL module에  보낼 데이터(제어, 설정, 알람)가  있을 경우 호출 */
static int send_mcu_data_control( UINT16 inter_msg, void * ptrdata  )
{
	if ( g_ptrsend_mcu_data_control_func != NULL )
	{
		return g_ptrsend_mcu_data_control_func( inter_msg, ptrdata );
	}

	return ERR_RETURN;
}

/* CONTROL Module로 부터 수집 데이터, 알람, 장애  정보를 받는 내부 함수로
   등록된 proc function list 중에서 control에서  넘겨 받은 inter_msg와 동일한 msg가 존재할 경우
   해당  함수를 호출하도록 한다. 
 */
static int internal_mcu_recv_ctrl_data( unsigned short inter_msg, void * ptrdata )
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
	proc_func_cnt= ( sizeof( g_proc_mcu_func_list ) / sizeof( proc_func_t ));

	/* 등록된 proc function에서 inter_msg가 동일한 function 찾기 */
	for( i = 0 ; i < proc_func_cnt ; i++ )
	{    
		list_msg 			= g_proc_mcu_func_list[ i ].inter_msg; 	/* function list에 등록된 msg id */
		ptrtemp_proc_func 	= g_proc_mcu_func_list[ i ].ptrproc;	/* function list에 등록된 function point */

		if ( ptrtemp_proc_func != NULL && list_msg == inter_msg )
		{    
			ptrtemp_proc_func( inter_msg, ptrdata );
			break;
		}    
	} 

	return ERR_SUCCESS;
}

/* mcu module 초기화 내부 작업 */
static int internal_init( void )
{
	int i,ret = ERR_SUCCESS;

	/* warring을 없애기 위한 함수 호출 */
	send_mcu_data_control( 0, NULL );

	ret = set_mcu_fd();
	
	if ( ret == ERR_SUCCESS )
	{
		ret = create_mcu_thread( );
	}

	memset( &g_mcu_queue, 0, sizeof( g_mcu_queue ));
	memset( &g_send_sr_list, 0,sizeof( g_send_sr_list ));

	/* buf 생성 */
	g_mcu_queue.ptrlist_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_QUEUE_SIZE );
	g_mcu_queue.ptrrecv_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_RECV_SIZE );	
	g_mcu_queue.ptrsend_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_SEND_SIZE );	

	g_ptrread_sr			= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );
	g_ptrcom_sr				= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );
	g_ptrsts_sr				= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );	
	g_ptrctrl_sr			= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE );	


	if( g_mcu_queue.ptrlist_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create MCU SR List Buffer");
		ret = ERR_RETURN;
	}

	if( g_mcu_queue.ptrrecv_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create MCU SR Recv Buffer");
		ret = ERR_RETURN;
	}

	if( g_mcu_queue.ptrsend_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create MCU SR Send Buffer");
		ret = ERR_RETURN;
	}

	if ( ret == ERR_SUCCESS )
	{
		print_dbg(DBG_INFO, 1, "Success of MCU Module Initialize");
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
			print_dbg(DBG_ERR, 1, "Can not Create MCU SR Send List Buffer");
			ret = ERR_RETURN;
		}
		else
		{
			memset( g_send_sr_list.send_data[ i ].ptrdata, 0, MAX_SR_DEF_DATA_SIZE );
		}
	}

	g_mcu_sem_id = create_sem( g_mcu_sem_key );
	print_dbg( DBG_INFO, 1, " MCU SEMA : %d", g_mcu_sem_id );

	sys_sleep( 2000 );
	proc_mcu_com_req( 0, NULL );

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

static int idle_mcu_func( void )
{
	time_t cur_time;
	static time_t pre_time = 0;
	
	cur_time = time( NULL );
	
	if( pre_time == 0 ) 
		pre_time = cur_time;

	if ( cur_time - pre_time > TEST_INTVAL )
	{
		pre_time = cur_time;
		//proc_mcu_com_req( 0, NULL );
		//sys_sleep( 2000 );
		proc_mcu_status_req( 0 , NULL );
		//print_dbg( DBG_INFO,1, "SEND INFO CMD");
	}
	
	check_mcu_sr_timeout( );

	return ERR_SUCCESS;
}

/* mcu module loop idle 내부 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
static int internal_idle( void * ptr )
{
	( void )ptr;

	if( g_release == 0 )
	{
		create_mcu_thread();
		set_mcu_fd( );
		idle_mcu_func();
	}
	return ERR_SUCCESS;
}

static int release_buf ( void )
{
	int i = 0;

	if ( g_mcu_queue.ptrlist_buf != NULL )
	{
		free( g_mcu_queue.ptrlist_buf );
		g_mcu_queue.ptrlist_buf  = NULL;
	}

	if ( g_mcu_queue.ptrrecv_buf != NULL )
	{
		free( g_mcu_queue.ptrrecv_buf );
		g_mcu_queue.ptrrecv_buf  = NULL;
	}

	if ( g_mcu_queue.ptrsend_buf != NULL )
	{
		free( g_mcu_queue.ptrsend_buf );
		g_mcu_queue.ptrsend_buf  = NULL;
	}

	if( g_ptrread_sr != NULL )
	{ 
		free( g_ptrread_sr  );
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
	if ( g_mcu_sem_id > 0 )
	{
		destroy_sem( g_mcu_sem_id, g_mcu_sem_key );
		g_mcu_sem_id = -1;
	}

	return ERR_SUCCESS;
}

/* mcu module의 release 내부 작업 */
static int internal_release( void )
{
	g_release = 1;
	
	release_mcu_fd();
	//release_mcu_thread();

	print_dbg(DBG_INFO, 1, "Release of MCU Module");
	return ERR_SUCCESS;
}

/*================================================================================================
외부  함수  정의 
================================================================================================*/
int init_mcu( send_data_func ptrsend_data )
{
	/* CONTROL module의 함수 포인트 등록 */
	g_ptrsend_mcu_data_control_func = ptrsend_data ;	/* CONTROL module로 센서 , 장애  데이터를 보낼 수 있는 함수 포인터 */

	/* 기타 초기화 작업 */
	internal_init();

	return ERR_SUCCESS;
}

/* mcu module loop idle 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
int idle_mcu( void * ptr )
{
	return internal_idle( ptr );
}

/* 동적 메모리, fd 등 resouce release함수 */
int release_mcu( void )
{
	/* release 작업  */
	return internal_release(); 
}

/* CONTROL Module로 부터 제어, 설정, update 정보를 받는 외부 함수 */
int recv_ctrl_data_mcu( unsigned short inter_msg, void * ptrdata )
{
	return internal_mcu_recv_ctrl_data( inter_msg, ptrdata );
}
