/*********************************************************************
	file : apc_ups.c
	description :
	product by tory45( tory45@empal.com) ( 2016.08.05 )
	update by ?? ( . . . )

 ********************************************************************/
#include "com_include.h"
#include "apc_ups.h"

/*================================================================================================
 전역 변수 
================================================================================================*/
static send_data_func  g_ptrsend_apc_ups_data_control_func = NULL;		/* CONTROL 모듈에 설정,제어 정보를 넘기는 함수 포인트 */

/*================================================================================================
  내부 함수선언
================================================================================================*/
static int send_data_control( UINT16 msg, void * ptrdata );

static int internal_init( void );
static int internal_idle( void * ptr );
static int internal_release( void );
/*================================================================================================
내부 함수  정의 
================================================================================================*/

/* CONTROL module에  보낼 데이터(제어, 설정)가  있을 경우 호출 */
static int send_data_control( UINT16 msg, void * ptrdata  )
{
	if ( g_ptrsend_apc_ups_data_control_func != NULL )
	{
		return g_ptrsend_apc_ups_data_control_func( msg, ptrdata );
	}
	return ERR_RETURN;
}

/* CONTROL Module로 부터 수집 데이터를 받는 내부 함수 */
static int internal_recv_ctrl_data( unsigned short msg, void * ptrdata )
{
	if( ptrdata == NULL )
	{
		print_dbg( DBG_INFO, 1, "ptrdata is Null[%s:%s:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}


	return ERR_SUCCESS;
}

/* apc_ups module 초기화 내부 작업 */
static int internal_init( void )
{
	/* warring을 없애기 위한 함수 호출 */
	send_data_control( 0, NULL );

	return ERR_SUCCESS;
}

/* apc_ups module loop idle 내부 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
static int internal_idle( void * ptr )
{
	( void )ptr;


	return ERR_SUCCESS;
}

/* apc_ups module의 release 내부 작업 */
static int internal_release( void )
{
	return ERR_SUCCESS;
}

/*================================================================================================
외부  함수  정의 
================================================================================================*/
int init_apc_ups( send_data_func ptrsend_data )
{
	/* CONTROL module의 함수 포인트 등록 */
	g_ptrsend_apc_ups_data_control_func = ptrsend_data ;	/* CONTROL module로 설정, 제어 데이터를 보낼 수 있는 함수 포인터 */

	/* 기타 초기화 작업 */
	internal_init();

	return ERR_SUCCESS;
}

/* apc_ups module loop idle 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
int idle_apc_ups( void * ptr )
{
	return internal_idle( ptr );
}

/* 동적 메모리, fd 등 resouce release함수 */
int release_apc_ups( void )
{
	/* release 작업  */
	return internal_release(); 
}

/* CONTROL Module로 부터 수집 데이터를 받는 외부 함수 */
int recv_ctrl_data_apc_ups( unsigned short msg, void * ptr )
{
	return internal_recv_ctrl_data( msg, ptr);
}

