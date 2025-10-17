/*********************************************************************
	file : pwr_mng.c
	description :
	product by tory45( tory45@empal.com) ( 2016.08.05 )
	update by ?? ( . . . )

 ********************************************************************/
#include "com_include.h"

#include "pwr_mng.h"
#include "sem.h"
#include "control.h"

//#include "fms_client.h"
//#include "op_server.h"
//#include "update_client.h"
#include "radio.h"
#include "gpio_mcu.h"

#define MAX_SAME_CNT    1	

#define P_OK			1
#define P_NO			0

#define P_WAIT			0
#define P_START			1
#define P_END			2

#define P_STEP_0		0
#define P_STEP_1		1
#define P_STEP_2		2
#define P_STEP_3 		3
#define MAX_PROC_STEP 	P_STEP_2

#define P_ERR_NO		0	
#define MS 				1000
#define MIN_VAL			( 5  * MS )

#define AUTO_MODE		9
#define RELEASE_MODE	1
/*
전원제어기 정보 
typedef struct pwr_dev_info_
{
	BYTE dev_pos;
	BYTE req_val;
	BYTE resp_val;
}pwr_dev_info_t;

typedef strcut pwr_dev_mng_
{
	int cnt;
	pwr_dev_info_t dev[ MAX_PWR_DEV_CNT ];
}pwr_dev_mng_t ;
*/

/* 항목 추가시 change_auto_on off 함수에서 항목 초기화 할것 */
typedef struct pwr_sts_
{
	volatile int clientid;	
	volatile int fcltcode;
	volatile int pwr_delay;
	volatile int snsr_sub_type;
	volatile int maxstep;
	volatile int curstep;
	volatile int onoff;
	volatile int eqp_first_step; 			/* radio에게 첫번째 POWR ON 요청 하는 단계 유무 */
	volatile int eqp_last_step;				/* eqp의 마지막 보고 단계 유무 */
	volatile int rtu_first_step;				/* rtu의 최종 보고 단계 유무 */
	volatile int rtu_last_step;				/* rtu의 최종 보고 단계 유무 */
	volatile int start_ping;				/* 감지 장비 ping 시작 유무      0: Wait,   1: 시작   1이상 : 진행 중 */
	volatile int ping_sts;					/* 감지 장비 ping 상태  ( 통신 에러 : 전원 제어 시작 ) O:미확인 1:GOOD,  -1:BAD */
	char szip[ MAX_IP_SIZE];				/* ping target ip (감시 장비 ip ) */
	volatile int pwr_control_step;			/* 전원 ON/OFF 를 시작 유무  0: Wait,    1: Start, 2 종료( 확인 완료 )  */

	volatile int automode;
	volatile UINT32 auto_off_end_time;
	volatile UINT32 auto_on_start_time;
    volatile int ecurstatus; 
	fclt_ctrl_eqponoff_t eqp_onoff;			/* 전원 ONOFF 메시지 */
	
	/* 2016 11 16 */
	volatile int proc_step;					/* 감시 장치 전원 제어 단계 : 0~ 2단계 */
	volatile int proc_error;				/* 감시 장치 전원 제어 시 최종 에러 상태 */
	volatile int proc_sts;					/* NONE, START, INT, END */	

	/* off time set */
	volatile UINT32 set_eoff_step1_st_time;		/* eqp 전원 off 요청 시간 */
	volatile UINT32 set_eoff_step1_end_time;		/* eqp 전원 off 응답 시간  */

	/* on time set */
	volatile UINT32 set_eon_pwr_end_time;	/* eqp 전원 제어기  종료 시간 */
	volatile UINT32 set_eon_arp_end_time;	/* EQP OS 부팅 대기 종료 시간 */
	volatile UINT32 set_eon_step1_st_time;		/* EQP 전원 ON 요청 시작 시간 */
	volatile UINT32 set_eon_step1_end_time; /* EQP 전원 ON 첫번째 응답 시간 */

	/* common time set */
	volatile UINT32 set_pwr_ctl_st_time;	/* 전원 제어 시작 시간 */
	volatile int set_pwr_delay_pos;			/* 전원 제어 delay값  위치 */

	volatile int ping_same_cnt;
}pwr_sts_t;

/*================================================================================================
 전역 변수 
================================================================================================*/
static volatile int g_ctrl_func_cnt = 0;
static volatile int g_radio_func_cnt = 0;
static volatile int g_release		= 0;
static volatile int g_chk_ping	= 0;

static volatile int g_fre_fclt_code	= EFCLT_EQP01;
static volatile int g_frs_fclt_code	= EFCLT_EQP01+1;
static volatile int g_sre_fclt_code	= EFCLT_EQP01+2;
static volatile int g_fde_fclt_code	= EFCLT_EQP01+3;

static pwr_sts_t 		g_pwr_sts;
static pwr_mthod_mng_t  g_pwr_mthod;
static volatile int g_clientid = 0;

//static fclt_ctrl_eqponoff_t g_auto_eqp_onoff;

/* 2016 11 14 new protocol ( on / off option ) */
static fclt_ctrl_eqponoff_optionoff_t g_pwr_off_opt;
static fclt_ctrl_eqponoff_optionon_t g_pwr_on_opt;

/*================================================================================================
  내부 함수선언
================================================================================================*/
/* thread ( power manager )*/
static volatile int g_start_pwr_mng_pthread 	= 0; 
static volatile int g_end_pwr_mng_pthread 		= 1; 
static pthread_t pwr_mng_pthread;
static void * pwr_mng_pthread_idle( void * ptrdata );

/* thread ( timer )*/
static volatile int g_start_timer_pthread 	= 0; 
static volatile int g_end_timer_pthread 		= 1; 
static pthread_t timer_pthread;
static void * timer_pthread_idle( void * ptrdata );

static int internal_init( void );
static int internal_idle( void );
static int internal_release( void );
static int run_arp( int fcltcode  );

static int change_auto_on( void );
static int eqp_pwr_on_proc_1( void );
static int onoff_option_default( void );

static int send_error_eqp_ctl_resp( int errcode, int err_step, int max_step, int status ,int release );
static int set_pwr_off_end( void );

static int set_pwr_on_end( void );
static int set_pwr_on_ing( void );

/* power manager -> radio */
static int send_radio_pwr_start( char * ptronoff  );

/* control -> power manager */
static int recv_ctrl_eqp_pwr_req( UINT16 inter_msg, void * ptrdata );
static int recv_ctrl_eqp_pwr_ack( UINT16 inter_msg, void * ptrdata );
static int recv_ctrl_eqp_res_ack( UINT16 inter_msg, void * ptrdata );

/* radio -> power manager */
static int recv_radio_eqp_pwr_resp( UINT16 inter_msg, void * ptrdata );
static int recv_radio_eqp_pwr_req( UINT16 inter_msg, void * ptrdata );

/*================================================================================================
함수 포인터  
================================================================================================*/
static proc_func_t ctrl_proc_list[] =
{
	/* op pwr control */
	{ EFT_EQPCTL_POWER, 				recv_ctrl_eqp_pwr_req},	/* 전원제어 요청 ON/OFF */
	{ EFT_EQPCTL_POWERACK, 				recv_ctrl_eqp_pwr_ack},	/* 전원제어 요청 응답  */
	{ EFT_EQPCTL_RESACK, 				recv_ctrl_eqp_res_ack},	/* 전원제어 상태 응답  */

	{ 0, NULL }
};

static proc_func_t radio_proc_list[] =
{
	/* radio pwr resp */
	{ EFT_EQPCTL_RES, 				recv_radio_eqp_pwr_resp},	/* 전원제어 상태 응답 */
	{ EFT_EQPCTL_POWER, 			recv_radio_eqp_pwr_req},	/* 전원제어 요청 ON/OFF */

	{ 0, NULL }
};
/*================================================================================================
내부 함수  정의 
================================================================================================*/
static int recal_step( int *ptrmax_step, int * ptrcur_step )
{
	int max_step, cur_step;

	max_step = *ptrmax_step;
	cur_step = *ptrcur_step;

	/* Auto mode는 총 4단계이다 */
	if ( g_pwr_sts.automode == 1 )
	{
		max_step += 2;
		if ( g_pwr_sts.onoff == EFCLTCTLCMD_ON )
		{
			cur_step += 2;
		}
	}

	*ptrmax_step = max_step;
	*ptrcur_step = cur_step;

	return ERR_SUCCESS;
}

static int send_err_to_server( int errcode, int err_step, int max_step )
{
	int ret = ERR_SUCCESS;
    /* 에러 임을 보낸다 */

	/* Auto mode는 총 4단계이다 */
	recal_step( &max_step, &err_step );

    ret = send_error_eqp_ctl_resp( errcode, err_step, max_step, g_pwr_sts.proc_sts, 0 );
    
    return ret;
}

static int check_overtime_eqp_daemon_run( void )
{
	int arp_end_time ;
	UINT32 cur_time ;
	int intv;
	int daemon_run_timeout;
	int ret = 0;

	arp_end_time = g_pwr_sts.set_eon_arp_end_time;
	
	if ( arp_end_time == 0 )
		return ret;

	/* 통신이 되면  Daemon이 살아난 경우이다 . */
	if ( get_fclt_netsts_ctrl( g_pwr_sts.fcltcode ) == ESNSRCNNST_NORMAL )
	{
		g_pwr_sts.set_eon_arp_end_time = 0;
		return 1;
	}

	cur_time 			= get_sectick();
	daemon_run_timeout 	= ( g_pwr_on_opt.iwaittime_eqpdmncnn_ms/ MS );
	intv 				= cur_time - arp_end_time;

	if( intv >= daemon_run_timeout )
	{
		/* 전원 제어 종료 시간   초기화 */
		g_pwr_sts.set_eon_arp_end_time = 0;
		send_err_to_server( EEQPONOFFRSLT_PROCTIMEOUTOVER, P_STEP_1, MAX_PROC_STEP );
		print_dbg( DBG_ERR,1, "######## Timeout of EQP Daemon Run Time EQP:: ARP END TIME:%u, CUR TIME:%u , Daemon Run Time :%u ########", arp_end_time, cur_time, daemon_run_timeout );
		ret =1 ;
	}

	return ret;
}

/* 전원 제어 ON 후 OS가 부팅되는 시간까지 대기하였는지를 체크한다 */
static int check_eon_os_ontimeout( void )
{
	int pwr_end_time ;
	UINT32 cur_time ;
	int intv;
	int os_on_timeout;
	int ret = 0;

	pwr_end_time = g_pwr_sts.set_eon_pwr_end_time;
	
	if ( pwr_end_time == 0 )
		return ret;

	cur_time 		= get_sectick();
	os_on_timeout 	= ( g_pwr_on_opt.iwaittime_eqposstart_ms / MS );
	intv 			= cur_time - pwr_end_time;

	if( intv >= os_on_timeout )
	{
		/* 전원 제어 종료 시간   초기화 */
		g_pwr_sts.set_eon_pwr_end_time = 0;
		send_err_to_server( EEQPONOFFRSLT_PROCTIMEOUTOVER, P_STEP_1, MAX_PROC_STEP );
		print_dbg( DBG_ERR,1, "######## Timeout of EQP OS Booting Time EQP:: PWR END TIME:%u, CUR TIME:%u , OS Booting Time :%u ########", pwr_end_time, cur_time, os_on_timeout );
		ret =1 ;
	}

	return ret;
}

/* EQP 전원 ON 1단계 요청 후 응답 대기 시간을 확인하여 
   대기 시간이 초과하였을 경우 강제로 ON 2단계 응답을 강제로 대기한다. */
static int check_eon_step1_end_timeout( void )
{
	int st_time ;
	UINT32 cur_time =0;
	int intv;
	int resp_timeout;
	int ret = 0;

	st_time 			= g_pwr_sts.set_eon_step1_st_time;

	/* eqp on step 1 요청 없음 */
	if ( st_time == 0 )
		return ret;

	cur_time 			= get_sectick();
	resp_timeout 		= ( g_pwr_on_opt.iwaittime_step1_ms / MS );  /* ms -> sec */
	intv 				= cur_time - st_time;

	if( intv >= resp_timeout )
	{
		/* eqp on step1  시간  초기화 */
		g_pwr_sts.set_eon_step1_st_time	= 0;

		/* send err to fms */
		send_err_to_server( EEQPONOFFRSLT_PROCTIMEOUTOVER, P_STEP_1, MAX_PROC_STEP );
		print_dbg( DBG_ERR,1, "######## Timeout of EQP ON STEP 1 Response:: Start TIME:%u, CUR TIME:%u , TIMEOUT:%u ########", st_time, cur_time, resp_timeout );

		/* 강제로 STEP2을 진행한다 */
		set_pwr_on_ing();
		print_dbg( DBG_INFO,1, "######## Force Wait of ON STEP 2 ########");

		ret =1 ;
	}

	return ret ;
}

/* EQP 전원 ON 1단계 응답 후 2단계 응답 대기 시간 체크  */
static int check_eon_step2_end_timeout( void )
{
	int end_time ;
	UINT32 cur_time =0;
	int intv;
	int resp_timeout;
	int ret = 0;

	end_time 			= g_pwr_sts.set_eon_step1_end_time;

	/* eqp on step 1 요청 없음 */
	if ( end_time == 0 )
		return ret;

	cur_time 			= get_sectick();
	resp_timeout 		= ( g_pwr_on_opt.iwaittime_step2_ms / MS );  /* ms -> sec */
	intv 				= cur_time - end_time;

	if( intv >= resp_timeout )
	{
		/* eqp on step1  시간  초기화 */
		g_pwr_sts.set_eon_step1_end_time	= 0;

		/* send err to fms ON Process End */
		recv_pwr_ctrl_release_pwr_mng( EEQPONOFFRSLT_PROCTIMEOUTOVER, P_STEP_2, MAX_PROC_STEP );
		print_dbg( DBG_ERR,1, "######## Timeout of EQP ON STEP 2 Response:: Start TIME:%u, CUR TIME:%u , TIMEOUT:%u ########", end_time, cur_time, resp_timeout );

		/* 강제로 ON Process를 종료 한다.  */
		set_pwr_on_end();
		print_dbg( DBG_INFO,1, "######## Force End of NO Process ########");

		ret =1 ;
	}

	return ret ;
}

/* EQP 전원 OFF 응답 후 EQP의 OS 종료 대기 시간  체크 */
static int check_eoff_os_closetimeout( void )
{
	int end_time ;
	UINT32 cur_time =0;
	int intv;
	int os_close_timeout;
	int ret = 0;

	end_time 			= g_pwr_sts.set_eoff_step1_end_time;

	/* eqp off 요청 없음 */
	if ( end_time == 0 )
		return ret;

	cur_time 			= get_sectick();
	os_close_timeout 	= ( g_pwr_off_opt.iwaittime_eqposend_ms / MS );  /* ms -> sec */
	intv 				= cur_time - end_time;

	if( intv >= os_close_timeout )
	{
		/* eqp off 종료 시간  초기화 */
		g_pwr_sts.set_eoff_step1_end_time	= 0;
		/* send err to fms */
		send_err_to_server( EEQPONOFFRSLT_PROCTIMEOUTOVER, P_STEP_1, MAX_PROC_STEP );
		print_dbg( DBG_ERR,1, "######## Timeout of EQP OS Close Time EQP:: END TIME:%u, CUR TIME:%u , TIMEOUT:%u ########", end_time, cur_time, os_close_timeout );
		ret =1 ;
	}

	return ret ;
}

/*
	EQP에 전원 OFF 요청  후 Waite time까지 응답이 없을 경우 
	서버에 TIMEOUT을 리턴하고 
	강제로 STEP2를 진행한다. 
*/
static int check_eoff_step1_end_timeout( void )
{
	int st_time ;
	UINT32 cur_time =0;
	int intv;
	int eoff_resp_timeout ;

	st_time 			= g_pwr_sts.set_eoff_step1_st_time;

	/* eqp off 요청 없음 */
	if ( st_time == 0 )
		return ERR_SUCCESS;

	cur_time 			= get_sectick();
	eoff_resp_timeout 	= ( g_pwr_off_opt.iwaittime_step1_ms / MS );  /* ms -> sec */
	intv 				= cur_time - st_time;

	if( intv >= eoff_resp_timeout )
	{
		/* eqp off 시작 시간 초기화 */
		g_pwr_sts.set_eoff_step1_st_time	= 0;

		/* send err to fms */
		send_err_to_server( EEQPONOFFRSLT_PROCTIMEOUTOVER, P_STEP_1, MAX_PROC_STEP );
		print_dbg( DBG_ERR,1, "######## Timeout of EQP STEP1 Response Time:: ST TIME:%u, CUR TIME:%u, TIMEOUT:%u ########", st_time, cur_time, eoff_resp_timeout );

		/* 강제 NEXT STEP */
		set_pwr_off_end();
		print_dbg( DBG_INFO,1, "######## Force Start of OFF STEP 2 ######## ");
	}

	return ERR_SUCCESS;
}

static void *  timer_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status = 0;
	int step = 0;

	g_end_timer_pthread = 0;

	while( g_release == 0 && g_start_timer_pthread == 1 )
	{
		sys_sleep( 1500 );
		step = g_pwr_sts.proc_step ;

		if ( step != P_STEP_0 )
		{
			check_eoff_step1_end_timeout( );

			check_eon_step1_end_timeout( );
			check_eon_step2_end_timeout( );
		}
		else
		{
			//printf( ".....................nnnnnnn.................\r\n");
		}
	}

	if( g_end_timer_pthread == 0 )
	{
		pthread_join( timer_pthread , ( void **)&status );
		g_start_timer_pthread 	= 0;
		g_end_timer_pthread 	= 1;
	}
	return ERR_SUCCESS;
}


static int run_arp( int fcltcode   )
{
	char output[ 200 ];
	char cmdstr[ 200 ];
	BYTE space_buf[ 10 ];
	char recv_cnt_buf[ 10 ];

	int i, len, space_cnt = 0;
	int cnt_len, bcnt_find = 0;
	int recv_cnt = 0;
	int net_sts = NOT_ALIVE;
	int pos_3, pos_4;
	int send_cnt	= 2;
	char szip[ MAX_IP_SIZE ];
	char * ptrifname = NULL;

	memset(&szip, 0, sizeof( szip ));
	memset( output, '\0', sizeof( output ) );
	memset( cmdstr, '\0', sizeof( cmdstr ) );

	ptrifname = get_ifname_shm();

	if ( get_fcltip_ctrl( fcltcode, szip ) != ERR_SUCCESS )
	{
		print_dbg( DBG_ERR,1, "not have fclt:%d ip[%s:%d:%s]",fcltcode, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	sprintf( cmdstr, "sudo arping -c %d %s -i %s | grep 'received' ", send_cnt, szip, ptrifname );
	//printf("================================== %s...\n", cmdstr );

	FILE * pcmd = popen( cmdstr, "r" );

	if( NULL != pcmd ) 
	{
		while(NULL != fgets( output, sizeof( output ), pcmd ) );
		{
			len = strlen( output );

			if ( len >= 200 )
				output[ 200 ] = 0x00;
			else
				output[ len ] = 0x00;
			
			//printf("Arp Output:(%s)..len:%d.........................\r\n", output, len  );

			/* example output for FMS RTU :"4 packets transmitted, 4 packets received, 0% unanssered */
			if ( len > 10 )
			{
				memset( space_buf, 0, sizeof( space_buf ));
				memset( recv_cnt_buf, 0, sizeof( recv_cnt_buf ));

				/* token of space */
				for( i = 0 ; i < len ; i++ )
				{
					if ( output[ i ] == 0x20 )
					{
						space_buf[ space_cnt++ ] = i;
					}
					/* 4번째 스페이스에 ARP 받은 팻킷 카운트 정보 */
					if ( space_cnt == 4 ) 
					{
						bcnt_find = 1;
						//printf("==========find ====================\n");
						break;
					}
				}

				/* find recv count string */
				if ( bcnt_find == 1 )
				{
					pos_3 = space_buf[ 2 ];
					pos_4 = space_buf[ 3 ];

					cnt_len = 0;
					cnt_len = pos_4 - pos_3;

					//printf("========== 3 Pos:%d 4 Pos: %d ====================\n", pos_3, pos_4 );

					memcpy( recv_cnt_buf, &output[ pos_4 - 1 ],  cnt_len - 1 );

					/* valid recv count string */
					if ( isNumStr( recv_cnt_buf ) == 1 )
					{
						/* recv cnt > 2 */
						recv_cnt = atoi( recv_cnt_buf );
						if( recv_cnt >= 1 )
						{
							net_sts = ALIVE; 
						}
					}
				}

			}
		}

		//printf( "arping start time :%u, end time = %u, inter time = %u....\n", start_time, end_time, end_time - start_time );
		if ( pcmd != NULL )
		{
			pclose( pcmd );
		}
	}
	else
	{
		print_dbg(DBG_ERR, 1,"NOT Opne ARP Command" );
	}
	if ( net_sts == ALIVE )
	{
		print_dbg(DBG_INFO, 1, "######## ARP Status:: Alived of RADIO Network(ARP Recv Cnt:%d, Sts:%d, IP:%s)", recv_cnt , net_sts, szip );
	}
	else
	{
		print_dbg(DBG_INFO, 1, "######## ARP Status:: Not Alive of RADIO Network(ARP Recv Cnt:%d, Sts:%d, IP:%s)", recv_cnt , net_sts, szip );
	}

	return net_sts;
}

static int set_power_control( int onoff )
{
	int last_step = 0;
	int pos;
	BYTE comport = 0;
	set_port_t set_port;
	UINT32 cur_time, inter_time;
	int ret = ERR_SUCCESS;
	memset( &set_port, 0, sizeof( set_port));
	int fcltcode = 0;
	UINT32 delay_time;
	int delay_pos ;	
	int err_step ;

	/* 현재 단계의 전원 제어 포트와 요청 값을  찾는다 */
	last_step 	= g_pwr_mthod.last_step;
	cur_time 	= get_sectick();;
	delay_pos 	= g_pwr_sts.set_pwr_delay_pos;	

	/* delay pos 가 배열 크기보다 클 경우 */
	if ( delay_pos >= MAX_PWR_SWITCH_CNT )
		delay_pos = MAX_PWR_SWITCH_CNT -1;

	/* 전원 제어 요청할 단계가 아니면 */
	if ( onoff == EFCLTCTLCMD_OFF && (g_pwr_sts.eqp_last_step == P_NO  || g_pwr_sts.ping_sts != NOT_ALIVE ))
	{
		return ERR_RETURN;
	}

	if ( onoff == EFCLTCTLCMD_ON && g_pwr_sts.rtu_first_step != P_OK )
	{
		return ERR_RETURN;
	}
	
	/* 전원 제어 요청이 끝난경우 */
	if ( g_pwr_sts.pwr_control_step == P_END )
	{
		return ERR_RETURN;
	}

	/* 더이상 보낼것이 없을 경우 전원제어 단계를 끝낸다..*/
	if ( last_step != 0 && last_step >= g_pwr_mthod.cnt )
	{
		g_pwr_sts.pwr_control_step = P_END;
		if ( g_pwr_sts.onoff == EFCLTCTLCMD_OFF )
		{
			g_pwr_sts.start_ping = P_END;
		}
		else
		{
			/* ON process 일경우 ARP를 시작하고 OS 부팅시간 대기 시간 체크를 위해
			   전원 제어기 마지막 시간을 설정한다. 
			 */
			g_pwr_sts.start_ping  			= P_START;
			g_pwr_sts.set_eon_pwr_end_time 	= get_sectick();
		}
		return ERR_RETURN;
	}

	/*  전원 제어 요청 시간 설정 
	 	전원 제어 요청 시 설정된 delay 시간 동안 wait한다. 
		전원 제어 delay 시간은 첫번째는 바로 제어하고 
		두번째부터  delay[ 0 ]번지 데이터를 읽는다.
	 */
	if ( g_pwr_sts.set_pwr_ctl_st_time != 0 )
	{
		inter_time = cur_time - g_pwr_sts.set_pwr_ctl_st_time ;

		if ( onoff == EFCLTCTLCMD_ON )
		{
			delay_time = ( g_pwr_on_opt.idelay_powerswitch_ms[ delay_pos ] / MS );
		}
		else
		{
			delay_time = ( g_pwr_off_opt.idelay_powerswitch_ms[ delay_pos ] / MS );
		}
	
		/* delay time 최소 시간을 5초로한다 */
		if ( delay_time < 5 )
			delay_time = 5;
	
		if ( inter_time < delay_time )
		{
			return ERR_RETURN;
		}
		
		print_dbg( DBG_NONE,1, "EQP PWR OFF Delay POS_VAL[ %d, %d ], [%s:%d:%s]",delay_pos, delay_time, __FILE__,__LINE__,__FUNCTION__);
	}
	
	/* 만약 전원 제어 요청 후 delay 타임 까지도 응답이 없을 경우 
	   서버로 에러를 전송하고 강제로 다음 전원 제어 포트로 넘긴다 
	 */
	if ( g_pwr_mthod.send_flag  == 1 )
	{
		if ( onoff == EFCLTCTLCMD_ON )
			err_step = P_STEP_1;
		else
			err_step = P_STEP_2;

		send_err_to_server( EEQPONOFFRSLT_PROCTIMEOUTOVER, err_step, MAX_PROC_STEP );

		g_pwr_mthod.last_step++;
		g_pwr_mthod.send_flag = 0;
		
		last_step 	= g_pwr_mthod.last_step;
		print_dbg( DBG_ERR,1, "######## Timeout of Power Control Response last step :%d, [%s:%d:%s]",last_step, __FILE__,__LINE__,__FUNCTION__);
	}

	g_pwr_mthod.dev[ last_step ].req_val =  onoff;

	pos 				= g_pwr_mthod.dev[ last_step ].dev_pos;
	comport				= g_pwr_mthod.dev[ last_step ].comport;

	set_port.comport 	= comport;
	set_port.req_val 	= onoff;
	set_port.pos	 	= pos;

	/* 시리얼 통신  장애 시 알수 없음 에러를 보낸 다 */
	fcltcode = g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.efclt;
	if ( get_sr_netsts_ctrl( fcltcode ) == ESNSRCNNST_ERROR )
	{
		print_dbg( DBG_ERR,1, "Can not EQP POWER Control Becase of POWER CTRL TRANS ERROR(%d),[%s:%d:%s]",fcltcode,__FILE__,__LINE__,__FUNCTION__);
		recv_pwr_ctrl_release_pwr_mng( EEQPONOFFRSLT_RTUPWCTRLLR_NOTCNN, err_step, MAX_PROC_STEP );
		return ERR_RETURN; 
	}

	ret = set_eqp_pwr_value_ctrl( &set_port );
	if ( ret == ERR_SUCCESS )
	{
		g_pwr_mthod.send_flag = 1;
	}

	/* 첫번째 전원 제어 요청을 할 경우에만 delay 위치를 +1 한다
		전원 제어 delay 시간은 첫번째는 바로 제어하고 
		두번째부터  delay[ 0 ]번지 데이터를 읽는다.
	*/
	if ( g_pwr_sts.set_pwr_ctl_st_time > 0 )
	{
		g_pwr_sts.set_pwr_delay_pos++;
	}

	g_pwr_sts.set_pwr_ctl_st_time  = cur_time;

	print_dbg( DBG_INFO,1, "######## Request Of POWER Contorl to PWR C, COM:%d, POS:%d, REQ :%d, Time:%u", comport, pos, onoff,get_sectick()  );

	return ret;
}

static int check_ping_start( void )
{
	static volatile int pre_ret = 0;
	static volatile int first 	= 0;
	int ret = 0;
	int os_off_overtime;
	int os_on_overtime;

	/* ping 시작 */
	if ( g_pwr_sts.start_ping  == P_START )
	{
		if (first == 0 )
		{
			print_dbg( DBG_INFO, 1, "Start Of Arping Target Device" );
		}
		
		first = 1;
		ret = run_arp( g_pwr_sts.fcltcode );	
		/* ret 1 : Not Alive , 2: Alive */
		/* OFF 전원 일때 */
		if ( g_pwr_sts.onoff == EFCLTCTLCMD_OFF )
		{
			if ( ret == NOT_ALIVE && pre_ret == NOT_ALIVE )
			//if ( ret == ALIVE && pre_ret == ALIVE )
			{
				g_pwr_sts.ping_same_cnt++;
			}
			else
			{
				g_pwr_sts.ping_same_cnt = 0;
			}
			
			/* ARP 처리가 끝나지 않더라도 OS Close Timeout에 도달하면 강제 전원제어 처리한다 */
			os_off_overtime = check_eoff_os_closetimeout();
			
			/* 동일한 상태가 2회 일경우 */
			if ( g_pwr_sts.ping_same_cnt >= MAX_SAME_CNT || os_off_overtime == 1 )
			{
				/* PING_종료 */
				g_pwr_sts.start_ping	= P_END;
				g_pwr_sts.ping_sts 		= NOT_ALIVE;
				g_pwr_sts.ping_same_cnt = 0;
				pre_ret 				= 0;
				first 					= 0;

				/* ARP 종료 후 off step1 end 시간을 0으로 설정한다 */
				g_pwr_sts.set_eoff_step1_end_time = 0;
				print_dbg( DBG_INFO, 1, "######## ARP::The remote RADIO Monitor has been shut down...");
			}
		}
		else
		{
			/* ON 전원일때 */
			if ( ret == ALIVE && pre_ret == ALIVE )
			{
				g_pwr_sts.ping_same_cnt++;
			}
			else
			{
				g_pwr_sts.ping_same_cnt = 0;
			}
			
			/* OS 부팅 대기 시간까지 기다린다 */
			os_on_overtime = check_eon_os_ontimeout();
			if ( g_pwr_sts.ping_same_cnt >= ( MAX_SAME_CNT ) || os_on_overtime == 1 )
			{
				g_pwr_sts.start_ping 			= P_END;
				g_pwr_sts.ping_sts 				= ALIVE;
				g_pwr_sts.ping_same_cnt 		= 0;
				pre_ret 						= 0;
				first 							= 0;
				
				/* ARP [ OS 부팅 완료 ] 완료  시간을 설정한다 */
				g_pwr_sts.set_eon_pwr_end_time 	= 0;
				g_pwr_sts.set_eon_arp_end_time 	= get_sectick();

				print_dbg( DBG_INFO, 1, "######## ARP::The remote RADIO Monitor has been running.....");
			}
		}

		pre_ret = ret;
	}
	else
	{
		g_pwr_sts.ping_same_cnt = 0;
		pre_ret = 0;
	}

	return ret;
}

static int check_pwr_control_start( void )
{
    int onoff;
    int ping_sts;
    int pwr_ctrl_step = 0;
    int eqp_last;
    int ret = ERR_SUCCESS;
    int rtu_first_step = 0;

    onoff 			= g_pwr_sts.onoff;
    ping_sts 		= g_pwr_sts.ping_sts; 
    pwr_ctrl_step	= g_pwr_sts.pwr_control_step;
    eqp_last        = g_pwr_sts.eqp_last_step;
    rtu_first_step  = g_pwr_sts.rtu_first_step;

	if ( onoff == EFCLTCTLCMD_OFF )
	{
		/* 전원제어 상태가 아니고 
		   서버에서 마지막 RES ACK를 받으면
		   전원 제어를 시작한다. 
		 */
		if ( pwr_ctrl_step == P_WAIT  && eqp_last == P_OK )
		{
			/* 서버로부터 RES ACK를 받고 PING으로 OS가 다운되어 있을 경우 전원 SHUTDOWN함 */
			if ( ping_sts == NOT_ALIVE )
			{
				g_pwr_sts.pwr_control_step = P_START;	
				ret = set_power_control( EFCLTCTLCMD_OFF );
				/* Start Power OFF Control : 전원제어 시작 */
				if ( ret != ERR_SUCCESS )
				{
					g_pwr_sts.pwr_control_step = P_WAIT;
				}
			}
		}
	}
	else
	{
		/* 전원 제어 상태가 아니고 
		   서버로부터 첫번째 PWR ACK를 받으면 
		   전원 제어기를 제어한다.  */

		if ( pwr_ctrl_step == P_WAIT && rtu_first_step == P_OK )
		{
			g_pwr_sts.pwr_control_step = P_START;
			ret = set_power_control( EFCLTCTLCMD_ON );

			if ( ret != ERR_SUCCESS )
			{
				g_pwr_sts.pwr_control_step = P_WAIT;
			}
		}
	}

	return ret;
}

static int check_pwr_control_next( void )
{
	set_power_control( g_pwr_sts.onoff );

	return ERR_SUCCESS;
}

/* eqp  전원제어 예외 에러가 발생 했을 경우 보낸다 */
static int send_error_eqp_ctl_resp( int errcode, int err_step, int max_step, int status, int release )
{
	fclt_ctrl_eqponoffrslt_t onoff_resp;
	inter_msg_t inter_msg;
	int cmd = EFT_EQPCTL_RES;
	tid_t tid;
	static volatile int err_send_cnt  = 0;
	int ret = 0;
	time_t cur_time;

	memset(&onoff_resp, 0, sizeof( onoff_resp));
	memset(&inter_msg, 0, sizeof( inter_msg ));
	memset( &tid, 0, sizeof( tid ));
	
	/* 마지막 ERR 상태를 저장한다 */
	g_pwr_sts.proc_error 					= errcode;

	if ( g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.nsiteid == 0 && g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.nfcltid == 0 )
	{
		print_dbg( DBG_ERR,1, "Can not Err Msg send to server,[%s:%d:%s]",__FILE__,__LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	if ( err_send_cnt++ > MAX_SEND_CNT )
	{
		err_send_cnt = 1;
	}

	cur_time = time( NULL );

	tid.cur_time 	= cur_time;
	tid.siteid 		= get_my_siteid_shm();
	tid.sens_type 	= ESNSRTYP_DO;
	tid.seq 		= err_send_cnt;

	cnvt_fclt_tid( &tid );
	g_pwr_sts.eqp_onoff.ctl_base.tdbtans.tdatetime = (UINT32)cur_time;
	
	onoff_resp.ctl_base.tfcltelem.nsiteid 	= g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.nsiteid;
	onoff_resp.ctl_base.tfcltelem.nfcltid	= g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.nfcltid;
	onoff_resp.ctl_base.tfcltelem.efclt 	= g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.efclt;
	onoff_resp.ctl_base.esnsrtype 			= g_pwr_sts.eqp_onoff.ctl_base.esnsrtype;
	onoff_resp.ctl_base.esnsrsub 			= g_pwr_sts.eqp_onoff.ctl_base.esnsrsub;
	onoff_resp.ctl_base.idxsub				= g_pwr_sts.eqp_onoff.ctl_base.idxsub;
	memcpy(&onoff_resp.ctl_base.suserid ,   &g_pwr_sts.eqp_onoff.ctl_base.suserid, MAX_USER_ID) ;
	onoff_resp.ctl_base.ectltype 			= g_pwr_sts.eqp_onoff.ctl_base.ectltype;
	onoff_resp.ctl_base.ectlcmd 			= g_pwr_sts.eqp_onoff.ctl_base.ectlcmd;
	onoff_resp.ctl_base.op_ip 				= g_pwr_sts.eqp_onoff.ctl_base.op_ip;

	onoff_resp.emaxstep 					= max_step;
	onoff_resp.ecurstep 					= err_step ;
	onoff_resp.ecurstatus 					= status;
	onoff_resp.ersult                       = errcode ;

	memcpy( onoff_resp.ctl_base.tdbtans.sdbfclt_tid, tid.szdata, MAX_DBFCLT_TID );
	onoff_resp.ctl_base.tdbtans.tdatetime = (UINT32)cur_time ;
	/* power manager로 전송 */
	inter_msg.msg		= cmd;
	inter_msg.client_id = g_pwr_sts.clientid ;
	inter_msg.ptrbudy	= &onoff_resp;
	
	if ( release == 1 )
	{
   		inter_msg.ndata		= RELEASE_MODE; 
	}
	/* 서버 전송 */
    
    ret = send_eqp_pwr_data_ctrl( cmd , &inter_msg, SEND_FMS );
    //print_dbg( DBG_INFO, 1, "Send Error  EQP POWR Control Ret Sts:%d", ret );
    print_dbg( DBG_LINE, 1, NULL );

    return ret;
}

/* off 단계에서만 사용 */
static int send_complete_eqp_ctl_resp( void )
{
	fclt_ctrl_eqponoffrslt_t onoff_resp;
	inter_msg_t inter_msg;
	int cmd = EFT_EQPCTL_RES;
	tid_t tid;
	static volatile int complet_send_cnt  = 1;
	int ret = 0;
	time_t cur_time;

	memset(&onoff_resp, 0, sizeof( onoff_resp));
	memset(&inter_msg, 0, sizeof( inter_msg ));
	memset( &tid, 0, sizeof( tid ));

	/* 전원제어 마지막 단계 설정 OFF 경우 */
	g_pwr_sts.rtu_last_step = P_OK;
	g_pwr_sts.proc_sts 		= EFCLTCTLST_END;

	if ( complet_send_cnt++ > MAX_SEND_CNT )
	{
		complet_send_cnt = 1;
	}

	cur_time = time( NULL );

	tid.cur_time 	= cur_time;
	tid.siteid 		= get_my_siteid_shm();
	tid.sens_type 	= ESNSRTYP_DO;
	tid.seq 		= complet_send_cnt;

	cnvt_fclt_tid( &tid );

	onoff_resp.ctl_base.tfcltelem.nsiteid 	= g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.nsiteid;
	onoff_resp.ctl_base.tfcltelem.nfcltid	= g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.nfcltid;
	onoff_resp.ctl_base.tfcltelem.efclt 	= g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.efclt;
	onoff_resp.ctl_base.esnsrtype 			= g_pwr_sts.eqp_onoff.ctl_base.esnsrtype;
	onoff_resp.ctl_base.esnsrsub 			= g_pwr_sts.eqp_onoff.ctl_base.esnsrsub;
	onoff_resp.ctl_base.idxsub				= g_pwr_sts.eqp_onoff.ctl_base.idxsub;
	memcpy(&onoff_resp.ctl_base.suserid ,   &g_pwr_sts.eqp_onoff.ctl_base.suserid, MAX_USER_ID) ;
	onoff_resp.ctl_base.ectltype 			= g_pwr_sts.eqp_onoff.ctl_base.ectltype;
	onoff_resp.ctl_base.ectlcmd 			= g_pwr_sts.eqp_onoff.ctl_base.ectlcmd;
	onoff_resp.ctl_base.op_ip				= g_pwr_sts.eqp_onoff.ctl_base.op_ip;

	onoff_resp.emaxstep 					= g_pwr_sts.maxstep;
	onoff_resp.ecurstep 					= g_pwr_sts.curstep+1 ;
	onoff_resp.ecurstatus 					= EFCLTCTLST_END;
	onoff_resp.ersult                       = EEQPONOFFRSLT_OK;

	memcpy( onoff_resp.ctl_base.tdbtans.sdbfclt_tid, tid.szdata, MAX_DBFCLT_TID );
	onoff_resp.ctl_base.tdbtans.tdatetime = (UINT32)cur_time;

	/* power manager로 전송 */
	inter_msg.msg		= cmd;
	inter_msg.client_id = g_pwr_sts.clientid ;
	inter_msg.ptrbudy	= &onoff_resp;

	/* 수동 모드 일 경우 제어 상태를 종료한다. */
	/* 수동 후 자동 전환이 10초 임으로 제어 후 release 한다 
	 단 자동 ON 시작 후 NDATA= 9로 설정해야 한다. */
	//if( g_pwr_sts.automode != P_OK )
	{
		inter_msg.ndata = RELEASE_MODE;
	}
	/* DB 등록 */

	/* 서버 전송 */
	ret = send_eqp_pwr_data_ctrl( cmd , &inter_msg, SEND_FMS );
	print_dbg( DBG_INFO, 1, "Send Complete EQP POWR Control::Ret Sts:%d", ret );

	if ( g_pwr_sts.rtu_last_step == P_OK  )
	{
		/* OFF prccess 종료 */

		print_dbg( DBG_INFO,1, "############ End of EQP Power OFF:[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);

		if ( g_pwr_sts.automode == P_OK )
		{
			change_auto_on( );	
		}
		else
		{
			memset( &g_pwr_sts, 0, sizeof( g_pwr_sts ));
			/* 감시 장치 전원에 단계를 초기화 한다 */
		}
	}
	print_dbg( DBG_LINE, 1, NULL );

  return ret ;
}

static int send_eqp_power_on( void )
{
    int ret = ERR_SUCCESS;

    ret= send_radio_pwr_start("ON" );

    return ret;
}

/* =========================================================================
   - OP에서 전원 OFF 시작 요청 후 서버에서 POWER ACK에 상관없이 
   - RADIO에 전원 OFF 요청
 ==========================================================================*/
static int send_radio_pwr_start( char * ptronoff  )
{
	inter_msg_t inter_msg;
	int ret = ERR_SUCCESS;
	int cmd = EFT_EQPCTL_POWER;
	int curstep = 0;

	memset( &inter_msg, 0, sizeof( inter_msg ));

#if 1
	/* 네트워크 장애 시 알수 없음 에러를 보낸 다 */
	if ( get_fclt_netsts_ctrl( g_pwr_sts.fcltcode ) == ESNSRCNNST_ERROR )
	{
		print_dbg( DBG_ERR,1, "Can not EQP POWER Control Becase of EQP NETWORK ERROR( %d ), [%s:%d:%s]",g_pwr_sts.fcltcode, __FILE__, __LINE__,__FUNCTION__);

		if( g_pwr_sts.onoff == EFCLTCTLCMD_OFF )
			curstep = P_STEP_1;
		else
			curstep = P_STEP_1;

		recv_pwr_ctrl_release_pwr_mng( EEQPONOFFRSLT_EQPDISCONNECTED, curstep, MAX_PROC_STEP );
		return ERR_RETURN;
	}
#endif

	/* power manager로 전송 */
	inter_msg.msg		= cmd;
	inter_msg.client_id = g_pwr_sts.clientid ;
	inter_msg.ptrbudy	= &g_pwr_sts.eqp_onoff;

	ret = recv_ctrl_data_radio( cmd , &inter_msg);

	if ( ret != ERR_SUCCESS )
	{
		print_dbg( DBG_ERR,1, "Error of Send to RADIO that EQP Power %s :%d [%s:%d:%s]", ptronoff, ret, __FILE__, __LINE__,__FUNCTION__);
		memset(&g_pwr_sts, 0, sizeof( g_pwr_sts ));
	}
	else
	{
		print_dbg( DBG_INFO,1, "Send to RADIO that EQP Power %s:%d [%s:%d:%s]", ptronoff,  ret, __FILE__, __LINE__,__FUNCTION__);
	}

	return ret;
}


static int check_pwr_control_complete( void )
{
	int pwr_step = 0;
	int onoff =0;
	int rtu_last_step = 0;
	int eqp_first_step = 0;
	int ret = 0;
	int ping_sts = 0;

	pwr_step  	= g_pwr_sts.pwr_control_step;
	onoff 	  	= g_pwr_sts.onoff;
	rtu_last_step = g_pwr_sts.rtu_last_step;
	eqp_first_step = g_pwr_sts.eqp_first_step ;

	ping_sts 	= g_pwr_sts.ping_sts;

	/* 전원제어 상태가 끝나고 
	   서버로 마지막 단계를 전송하지 않았을 경우 
	   마지막 EQP CTRL RES 를 서버로 보낸다.  */
	if ( onoff == EFCLTCTLCMD_OFF && rtu_last_step == P_NO  )
	{
		if ( pwr_step == P_END )
		{
			/* FMS 서버로 마지막 상태 정보를 보낸다 */
			/* ON상태는 서버로 보내는 Complete 메시지가 없다.  */
			send_complete_eqp_ctl_resp();
		}
	}
	else if ( onoff == EFCLTCTLCMD_ON )
	{
		/* 전원제어기 제어가 끝나고 
		   핑이 살아 있으면 
		   EQP로 전원 ON 명령어를 RADIO로 보낸다 */

		if ( eqp_first_step == P_NO && pwr_step == P_END )
		{
			if ( ping_sts == ALIVE )
			{
				/* EQP Daemon이 실행하는 시간까지 대기가 끝나는 경우에만 EQP에 ON 명령어를 요청한다 */
				if( check_overtime_eqp_daemon_run() == 1 )
				{
					ret = send_eqp_power_on();
					if ( ret == ERR_SUCCESS )
					{
						g_pwr_sts.eqp_first_step = P_OK ;
						/* EQP에 ON 요청한 시간을 설정한다 */
						g_pwr_sts.set_eon_step1_st_time = get_sectick();
					}
				}
			}
		}
	}

	return ERR_SUCCESS;
}

/*=================================================================
  AUTO OFF가 종료 된 후 일정 시간 이후 자동으로 ON요청을 한다. 
  ================================================================*/
static int init_pwr_mthod( void )
{
	int i, cnt;

	/* power mthod 초기화 */
	cnt = g_pwr_mthod.cnt;

	g_pwr_mthod.last_step = 0;
	g_pwr_mthod.send_flag = 0;

	g_pwr_mthod.last_ctrl_time = 0;

	for( i = 0 ; i < cnt ; i++ )
	{
		if( g_pwr_sts.onoff == EFCLTCTLCMD_OFF)
		{
			g_pwr_mthod.dev[ i ].req_val = 0;
		}
		else 
		{
			g_pwr_mthod.dev[ i ].req_val = 1;
		}

		g_pwr_mthod.dev[ i ].last_val = 99;
	}

	return ERR_SUCCESS;
}

static int change_auto_on( void )
{
	UINT32 cur_time;

	/* 기존 OFF 정보 초기화 */
    g_pwr_sts.maxstep				= 0;
    g_pwr_sts.curstep				= 0;
    g_pwr_sts.onoff					= 0;
    g_pwr_sts.eqp_first_step		= 0;
    g_pwr_sts.eqp_last_step			= 0;
    g_pwr_sts.rtu_last_step			= 0;	
    g_pwr_sts.rtu_first_step  		= 0;
    g_pwr_sts.start_ping			= 0;	
    g_pwr_sts.ping_sts				= 0;
    g_pwr_sts.pwr_control_step		= 0;
    g_pwr_sts.auto_off_end_time		= 0;
    g_pwr_sts.auto_on_start_time	= 0;
    g_pwr_sts.ecurstatus        	= 0;
	g_pwr_sts.set_eoff_step1_st_time= 0;	/* eqp 전원 off 요청 시간 */
	g_pwr_sts.set_eoff_step1_end_time= 0;	/* eqp 전원 off 응답 시간  */

	g_pwr_sts.set_pwr_ctl_st_time	= 0;	/* 전원 제어 시작 시간 */
	g_pwr_sts.set_pwr_delay_pos		= 0;	/* 전원 제어 delay값  위치 */

	g_pwr_sts.set_eon_pwr_end_time	= 0;	/* eqp 전원 제어기  종료 시간 */
	g_pwr_sts.set_eon_arp_end_time	= 0;	/* eqp os 부팅 대기 종료 시간 */
	g_pwr_sts.set_eon_step1_st_time	= 0;	/* EQP 전원 ON 요청 시작 시간 */
	g_pwr_sts.set_eon_step1_end_time= 0; 	/* EQP 전원 ON 첫번째 응답 시간 */

    g_pwr_sts.ping_same_cnt 		= 0;

	/* off -> on 변경 */
	g_pwr_sts.eqp_onoff.ctl_base.ectlcmd = EFCLTCTLCMD_ON;
	g_pwr_sts.onoff					= EFCLTCTLCMD_ON;

	/* Automode일 경우 바로 ON STEP 1으로 설정한다. */
	g_pwr_sts.proc_step				= P_STEP_1;
	g_pwr_sts.proc_error			= P_ERR_NO;
	g_pwr_sts.proc_sts				= EFCLTCTLST_NONE;

	cur_time = get_sectick();

	/* off 변경 시간 등록 */
	g_pwr_sts.auto_off_end_time 	= cur_time;

	init_pwr_mthod();

	print_dbg( DBG_INFO,1, "############ Change of EQP Power OFF TO ON(Time:%u) [%s:%d:%s]",cur_time, __FILE__, __LINE__,__FUNCTION__);
	return ERR_SUCCESS;
}

static int send_auto_on_start_toserver( void )
{
	tid_t tid;
	inter_msg_t inter_msg;
	int ret = ERR_SUCCESS;
	UINT16 cmd = EFT_EQPCTL_POWER;
	static volatile int auto_send_cnt  = 0;
	time_t cur_time ;

	memset( &tid, 		0, sizeof( tid ));
	memset( &inter_msg, 0, sizeof( inter_msg ));


	if ( auto_send_cnt ++ > MAX_SEND_CNT )
	{
		auto_send_cnt  = 1;
	}

	cur_time 		= time( NULL );
	tid.cur_time 	= cur_time;
	tid.siteid 		= get_my_siteid_shm();
	tid.sens_type 	= ESNSRTYP_DO;
	tid.seq 		= auto_send_cnt ;

	cnvt_fclt_tid( &tid );

	/* tid 변경 */
	g_pwr_sts.eqp_onoff.ctl_base.tdbtans.tdatetime = (UINT32)cur_time;
	memcpy( g_pwr_sts.eqp_onoff.ctl_base.tdbtans.sdbfclt_tid, tid.szdata, MAX_DBFCLT_TID );

	/* power manager로 전송 */
	inter_msg.msg		= cmd;
	inter_msg.client_id = g_pwr_sts.clientid ;
	inter_msg.ptrbudy	= &g_pwr_sts.eqp_onoff;
	inter_msg.ndata		= AUTO_MODE;

	/* 서버 전송 */
	print_dbg( DBG_LINE, 1, NULL );
	ret = send_eqp_pwr_data_ctrl( cmd, &inter_msg, SEND_FMS );
	if ( ret == ERR_SUCCESS )
	{
		g_pwr_sts.auto_on_start_time = get_sectick();
		print_dbg( DBG_INFO,1, "############ Send to FMS Server that Start of Auto Power ON:(Time:%u)[%s:%d:%s]",get_sectick(), __FILE__, __LINE__,__FUNCTION__);
	}

	return ret ;
}

static int check_auto_mode( void )
{
    int automode = 0;
    UINT32 off_end_time;
    UINT32 on_start_time;
    UINT32 cur_time;
    UINT32 inter_time;
    int ret = ERR_RETURN;

	automode 		= g_pwr_sts.automode;
	off_end_time 	= g_pwr_sts.auto_off_end_time;
	on_start_time 	= g_pwr_sts.auto_on_start_time;
	cur_time 		= get_sectick();

	if ( (automode == P_OK ) && ( off_end_time > 0 ) && (on_start_time == P_NO ))
	{
		inter_time = cur_time - off_end_time;

		//if ( inter_time > 3 )
		if ( inter_time > MAX_AUTO_ON_START_TIME )
		{
			print_dbg( DBG_INFO,1, "############ Start of EQP Power OFF TO ON (Auto Mode Time:%u) [%s:%d:%s]",cur_time, __FILE__, __LINE__,__FUNCTION__);

			g_pwr_sts.auto_on_start_time = cur_time;
			send_auto_on_start_toserver();

			g_pwr_sts.pwr_control_step = P_WAIT;
			/* AUTO ON 일 경우 초기화 한다 */
			g_pwr_sts.proc_step 	= P_STEP_1;
			g_pwr_sts.proc_error 	= P_ERR_NO;
			g_pwr_sts.proc_sts 		= EFCLTCTLST_START;
			ret = eqp_pwr_on_proc_1();
		}
	}
	return ret;
}


/* CONTROL Module로 부터 수집 데이터를 받는 내부 함수 */
static int internal_recv_ctrl_data( unsigned short inter_msg, void * ptrdata )
{
	UINT16 msg;
	proc_func ptrfunc = NULL;
	int i, cnt;

	if( inter_msg != 0 && ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	cnt = g_ctrl_func_cnt;

	for( i = 0 ; i < cnt  ; i++ )
	{
		msg 	= ctrl_proc_list[ i ].inter_msg;
		ptrfunc	= ctrl_proc_list[ i ].ptrproc; 

		if ( ptrfunc != NULL && msg == inter_msg )
		{
			return ptrfunc( msg, ptrdata );
		}
	}

	return ERR_RETURN;
}

static int internal_recv_radio_data( unsigned short inter_msg, void * ptrdata )
{
	UINT16 msg;
	proc_func ptrfunc = NULL;
	int i, cnt;

	if( inter_msg != 0 && ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	cnt = g_radio_func_cnt;
	
	for( i = 0 ; i < cnt  ; i++ )
	{
		msg 	= radio_proc_list[ i ].inter_msg;
		ptrfunc	= radio_proc_list[ i ].ptrproc; 

		if ( ptrfunc != NULL && msg == inter_msg )
		{
			return ptrfunc( msg, ptrdata );
		}
	}
	return ERR_RETURN;
}

static int internal_recv_pwr_off_option( unsigned short inter_msg, void * ptrdata )
{
	int i ;
	inter_msg_t * ptrmsg =NULL;
	inter_msg_t send_msg;
	UINT16 cmd;
	fclt_ctrl_eqponoff_optionoff_t off_opt;
	int clientid ;

	if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
        return ERR_RETURN;
    }

	ptrmsg = ( inter_msg_t *)ptrdata;
	clientid = ptrmsg->client_id;

	if ( ptrmsg->ptrbudy == NULL )
    {
        print_dbg( DBG_ERR,1, "is budy [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
        return ERR_RETURN;
    }
	
	
	memset( &send_msg, 0, sizeof( send_msg ));
	memset( &off_opt, 0, sizeof( off_opt ));

	memcpy(&off_opt, ptrmsg->ptrbudy , sizeof( fclt_ctrl_eqponoff_optionoff_t ));

	if ( off_opt.bgetorset == 1 )
	{
		print_dbg( DBG_NONE, 1, "############# Recv Set EQP PWR Off Option ################" );
		memcpy(&g_pwr_off_opt, ptrmsg->ptrbudy , sizeof( fclt_ctrl_eqponoff_optionoff_t ));

		/* on option default */
		for( i = 0 ; i < MAX_PWR_SWITCH_CNT ; i++ )
		{
			if ( g_pwr_off_opt.idelay_powerswitch_ms[ i ] < MIN_VAL )
			{
				g_pwr_off_opt.idelay_powerswitch_ms[ i ] = MIN_VAL; 
				print_dbg( DBG_ERR, 1, "Set Min valule Power OFF IDelay Timeout[ %d, %d ]", i, g_pwr_off_opt.idelay_powerswitch_ms[ i ] );
			}
			print_dbg( DBG_SAVE, 1, "Power OFF IDelay Timeout[ %d, %d ]", i, g_pwr_off_opt.idelay_powerswitch_ms[ i ]) ;
		}

		if ( g_pwr_off_opt.iwaittime_step1_ms < MIN_VAL )
		{
			g_pwr_off_opt.iwaittime_step1_ms = MIN_VAL;
			print_dbg( DBG_ERR, 1, "Set Min value Power Off EQP STEP1 End Timeout : %d", g_pwr_off_opt.iwaittime_step1_ms );
		}

		if ( g_pwr_off_opt.iwaittime_eqposend_ms < MIN_VAL )
		{
			g_pwr_off_opt.iwaittime_eqposend_ms = MIN_VAL;
			print_dbg( DBG_ERR, 1, "Set Min value Power Off EQP OS End Timeout : %d"	, g_pwr_off_opt.iwaittime_eqposend_ms );
		}

		print_dbg( DBG_SAVE, 1, "Power Off EQP STEP1 End Timeout : %d", g_pwr_off_opt.iwaittime_step1_ms );
		print_dbg( DBG_SAVE, 1, "Power Off EQP OS End Timeout : %d"	, g_pwr_off_opt.iwaittime_eqposend_ms );
		
		memcpy( &off_opt, &g_pwr_off_opt, sizeof( off_opt ));

		cmd 				= EFT_EQPCTL_POWEROPT_OFFRES;
		send_msg.client_id	= clientid;
		send_msg.msg 		= cmd;
		send_msg.ptrbudy 	= &off_opt;

		/* send cur step to op */
		send_eqp_pwr_data_ctrl( cmd, (void*)&send_msg, SEND_OP );
	}
	else
	{
		/* Get of Option */
		print_dbg( DBG_NONE, 1, "############# Recv Get EQP PWR Off Option ################" );
		memcpy( &off_opt, &g_pwr_off_opt, sizeof( off_opt ));

		cmd 				= EFT_EQPCTL_POWEROPT_OFFRES;
		send_msg.client_id	= clientid;
		send_msg.msg 		= cmd;
		send_msg.ptrbudy 	= &off_opt;

		/* send cur step to op */
		send_eqp_pwr_data_ctrl( cmd, (void*)&send_msg, SEND_OP );
	}

	return ERR_SUCCESS;
}

static int internal_recv_pwr_on_option( unsigned short inter_msg, void * ptrdata )
{
	int i ;
	inter_msg_t * ptrmsg =NULL;
	inter_msg_t send_msg;
	UINT16 cmd;
	fclt_ctrl_eqponoff_optionon_t on_opt;
	int clientid ;

	if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
        return ERR_RETURN;
    }

	ptrmsg = ( inter_msg_t *)ptrdata;
	clientid = ptrmsg->client_id;
	
	memset( &send_msg , 0, sizeof( send_msg ));
	memset( &on_opt, 0, sizeof( on_opt ));

	if ( ptrmsg->ptrbudy == NULL )
    {
        print_dbg( DBG_ERR,1, "is budy [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
        return ERR_RETURN;
    }
	
    memcpy(&on_opt, ptrmsg->ptrbudy , sizeof( fclt_ctrl_eqponoff_optionon_t ));

	/* Set of Option */
	if ( on_opt.bgetorset == 1 )
	{
		print_dbg( DBG_NONE, 1, "############# Recv EQP PWR ON Option ################" );

    	memcpy(&g_pwr_on_opt, ptrmsg->ptrbudy , sizeof( fclt_ctrl_eqponoff_optionon_t ));

		/* on option default */
		for( i = 0 ; i < MAX_PWR_SWITCH_CNT ; i++ )
		{
			if ( g_pwr_on_opt.idelay_powerswitch_ms[ i ] < MIN_VAL )
			{
				g_pwr_on_opt.idelay_powerswitch_ms[ i ] = MIN_VAL;
				print_dbg( DBG_ERR, 1, "Set Min valule Power ON IDelay Timeout[ %d, %d ]", i, g_pwr_on_opt.idelay_powerswitch_ms[ i ] );
			}
			print_dbg( DBG_SAVE, 1, "Power ON IDelay Timeout[ %d, %d ]", i, g_pwr_on_opt.idelay_powerswitch_ms[ i ] );

		}

		if ( g_pwr_on_opt.iwaittime_eqposstart_ms < MIN_VAL )
		{
			g_pwr_on_opt.iwaittime_eqposstart_ms = MIN_VAL;
			print_dbg( DBG_ERR, 1, "Set Min value Power ON OS Start Timeout : %d", g_pwr_on_opt.iwaittime_eqposstart_ms );
		}

		if ( g_pwr_on_opt.iwaittime_eqpdmncnn_ms < MIN_VAL )
		{
			g_pwr_on_opt.iwaittime_eqpdmncnn_ms= MIN_VAL;
			print_dbg( DBG_ERR, 1, "Set Min value Power ON EQP Daemon Connect Timeout : %d", g_pwr_on_opt.iwaittime_eqpdmncnn_ms );
		}

		if ( g_pwr_on_opt.iwaittime_step1_ms < MIN_VAL )
		{
			g_pwr_on_opt.iwaittime_step1_ms = MIN_VAL;
			print_dbg( DBG_ERR, 1, "Set Min value Power ON EQP STEP1 End Timeout : %d", g_pwr_on_opt.iwaittime_step1_ms);
		}

		if ( g_pwr_on_opt.iwaittime_step2_ms < MIN_VAL )
		{
			g_pwr_on_opt.iwaittime_step2_ms = MIN_VAL; 
			print_dbg( DBG_ERR, 1, "Set Min value Power ON EQP STEP2 End Timeout : %d", g_pwr_on_opt.iwaittime_step2_ms);
		}

		print_dbg( DBG_SAVE, 1, "Power ON OS Start Timeout : %d"			, g_pwr_on_opt.iwaittime_eqposstart_ms );
		print_dbg( DBG_SAVE, 1, "Power ON EQP Daemon Connect Timeout : %d"	, g_pwr_on_opt.iwaittime_eqpdmncnn_ms );
		print_dbg( DBG_SAVE, 1, "Power ON EQP STEP1 End Timeout : %d"		, g_pwr_on_opt.iwaittime_step1_ms);
		print_dbg( DBG_SAVE, 1, "Power ON EQP STEP2 End Timeout : %d"		, g_pwr_on_opt.iwaittime_step2_ms);

		memcpy( &on_opt, &g_pwr_on_opt, sizeof( on_opt ));

		cmd 				= EFT_EQPCTL_POWEROPT_ONRES;
		send_msg.client_id	= clientid;
		send_msg.msg 		= cmd;
		send_msg.ptrbudy 	= &on_opt;

		/* send cur step to op */
		send_eqp_pwr_data_ctrl( cmd, (void*)&send_msg, SEND_OP );
	}
	else
	{
		/* Get of Option */
		print_dbg( DBG_NONE, 1, "############# Recv Get EQP PWR ON Option ################" );
		memcpy( &on_opt, &g_pwr_on_opt, sizeof( on_opt ));

		cmd 				= EFT_EQPCTL_POWEROPT_ONRES;
		send_msg.client_id	= clientid;
		send_msg.msg 		= cmd;
		send_msg.ptrbudy 	= &on_opt;

		/* send cur step to op */
		send_eqp_pwr_data_ctrl( cmd, (void*)&send_msg, SEND_OP );
	}

	return ERR_SUCCESS;
}

static int internal_recv_eqp_status( unsigned short inter_msg, void * ptrdata )
{
	int clientid = 0;
	UINT16 cmd ;

	inter_msg_t * ptrmsg =NULL;
	inter_msg_t send_msg ;
	fclt_ctlstatus_eqponoff_t req_status;
	fclt_ctlstatus_eqponoffrslt_t resp_status;

	if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
        return ERR_RETURN;
    }

	memset( &req_status, 	0, sizeof( req_status ));
	memset( &resp_status, 	0, sizeof( resp_status ));
	memset( &send_msg, 		0, sizeof( send_msg ));

	ptrmsg = ( inter_msg_t *)ptrdata;
	clientid = ptrmsg->client_id;

	if ( ptrmsg->ptrbudy == NULL )
    {
        print_dbg( DBG_ERR,1, "is budy [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
        return ERR_RETURN;
    }

	memcpy( &req_status, ptrmsg->ptrbudy, sizeof( req_status ));

	/* 제어 상태 아님 */
	if ( g_pwr_sts.proc_step == P_STEP_0 )
	{
		resp_status.ectlcmd 	= EFCLTCTLCMD_NONE;
		resp_status.emaxstep 	= EEQPCTL_STEP_NONE;
		resp_status.ecurstep 	= EEQPCTL_STEP_NONE;
		resp_status.ecurstatus	= EFCLTCTLST_NONE;
		resp_status.ersult		= EEQPONOFFRSLT_UNKNOWN;
	}
	else
	{
		/* Auto mode는 총 4단계이다 */
		int max_step, cur_step;

		max_step = MAX_PROC_STEP;
		cur_step = g_pwr_sts.proc_step;
		
		recal_step( &max_step, &cur_step );

		resp_status.ectlcmd 	= g_pwr_sts.onoff;
		resp_status.emaxstep 	= max_step;
		resp_status.ecurstep	= cur_step; ;
		resp_status.ecurstatus	= g_pwr_sts.proc_sts;

		if ( g_pwr_sts.proc_error == P_ERR_NO )
		{
			resp_status.ersult 	= EEQPONOFFRSLT_OK;
		}
		else
		{
			resp_status.ersult 	= g_pwr_sts.proc_error;
		}
	}
#if 0
	resp_status.tfcltelem.nsiteid	= g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.nsiteid;
	resp_status.tfcltelem.nfcltid	= g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.nfcltid;
	resp_status.tfcltelem.efclt		= g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.efclt;
#endif

	resp_status.tfcltelem.nsiteid 	= get_my_siteid_shm() ;
	resp_status.tfcltelem.nfcltid 	= req_status.tfcltelem.nfcltid ;
	resp_status.tfcltelem.efclt 	= req_status.tfcltelem.efclt;

	cmd 				= EFT_EQPCTL_STATUS_RES;
	send_msg.client_id	= clientid;
	send_msg.msg 		= cmd;
	send_msg.ptrbudy 	= &resp_status;

	/* send cur step to op */
    send_eqp_pwr_data_ctrl( cmd, (void*)&send_msg, SEND_OP );

	return ERR_SUCCESS;
}

static int internal_set_ctrl_pwr_c_data( pwr_c_data_t * ptr_c_data )
{
	int last_step = 0;
	int pos , req_val, val ;
	
	/* 현재 단계의 전원 제어 포트와 요청 값을  찾는다 */
	last_step 	= g_pwr_mthod.last_step;
	pos 		= g_pwr_mthod.dev[ last_step ].dev_pos;
	req_val 	= g_pwr_mthod.dev[ last_step ].req_val;

	/* 현재 제어 단계가 아님 */
	if ( g_pwr_sts.pwr_control_step != P_START )
	{
		return ERR_SUCCESS;
	}

	if ( pos >= MAX_PWR_C_PORT_CNT )
	{
		print_dbg( DBG_ERR, 1, "Overflow PWR C PORT NUM:%d, step:%d,[%s:%d:%s]", pos,last_step, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	val = ptr_c_data->val[ pos ];

	print_dbg( DBG_INFO,1, "######## Recv PWR C Result last_step:%d, Port Pos:%d, req_val:%d, resp_val:%d", last_step, pos, req_val, val );

	if ( val != req_val )
	{
		print_dbg( DBG_ERR, 1, "Is not same val REQ(%d) and RESP(%d)[%s:%d:%s]", req_val, val , __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	else
	{
		g_pwr_mthod.dev[ last_step ].last_val = val;
		g_pwr_mthod.last_step++;
		g_pwr_mthod.send_flag = 0;
	}

	return ERR_SUCCESS;
}

static int internal_set_ctrl_mcu_etc_data( BYTE * ptrdoe )
{
	int last_step = 0;
	int pos , req_val, val ;
	
	/* 현재 단계의 전원 제어 포트와 요청 값을  찾는다 */
	last_step 	= g_pwr_mthod.last_step;
	pos 		= g_pwr_mthod.dev[ last_step ].dev_pos;
	req_val 	= g_pwr_mthod.dev[ last_step ].req_val;

	/* 현재 제어 단계가 아님 */
	if ( g_pwr_sts.pwr_control_step != P_START )
	{
		return ERR_SUCCESS;
	}

	if ( pos >= MAX_GPIO_CNT )
	{
		print_dbg( DBG_ERR, 1, "Overflow DOE  PORT NUM:%d, step:%d[%s:%d:%s]", pos, last_step, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	val = ptrdoe[ pos ];

	print_dbg( DBG_INFO,1, "Recv DOE  Result last_step:%d, Port Pos:%d, req_val:%d, resp_val:%d", last_step, pos, req_val, val );

	if ( val != req_val )
	{
		print_dbg( DBG_ERR, 1, "Is not same val REQ(%d) and RESP(%d)[%s:%d:%s]", req_val, val , __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	else
	{
		g_pwr_mthod.dev[ last_step ].last_val = val;
		g_pwr_mthod.last_step++;
		g_pwr_mthod.send_flag = 0;
	}
	return ERR_SUCCESS;
}	

/*
typedef struct pwr_sts_
{
	int fcltcode;
	time_t start_time;
	int automode;
	int onoff;

}pwr_sts_t;
*/

/*==========================================================================
  On Process  
  =========================================================================*/
static int print_proc( int onoff, int step )
{
	print_dbg( DBG_LINE, 1, NULL );
	print_dbg( DBG_INFO,1, "Power %s Process %d", onoff == 1 ? "ON": "OFF", step );

	return ERR_SUCCESS;
}
/* =========================================================================
   - 서버로 부터 첫번째 PWR ACK를 받을 경우 
     -> 전원 ON 제어 할 수 있도록 한다. 
	 -> 쓰레드를 이용한 자동 처리 과정 시작( pwr_mng_pthread_idle );
   ========================================================================*/
static int eqp_pwr_on_proc_1( void )
{
	print_proc( 1, 1 );

	/* 전원 제어 장치 구동 */

	/* 쓰레드 자동 프로세스 시작 */
	g_pwr_sts.rtu_first_step = P_OK;

	return ERR_SUCCESS;
}

/* =========================================================================
   - RADIO 부터 EQP CTL ON RESP 를 받을 경우 
     -> 서버로 전송한다. 
   ========================================================================*/
static int set_pwr_on_ing( void )
{
	g_pwr_sts.eqp_last_step = P_NO ;
	g_pwr_sts.set_eon_step1_st_time = 0;
	g_pwr_sts.set_eon_step1_end_time = get_sectick();
	g_pwr_sts.proc_step = P_STEP_2;

	return ERR_SUCCESS;
}

static int set_pwr_on_end( void )
{
	/* Last Step */
	g_pwr_sts.eqp_last_step = P_OK;
	g_pwr_sts.rtu_last_step = P_OK;

	memset( &g_pwr_sts, 0, sizeof( g_pwr_sts ));
	
	return ERR_SUCCESS;
}

static int eqp_pwr_on_proc_2( fclt_ctrl_eqponoffrslt_t * ptronoff )
{
	int maxstep, curstep;

	maxstep		= ptronoff->emaxstep;
	curstep 	= ptronoff->ecurstep;
	
	print_proc( 1, 2 );

	if ( maxstep == (curstep ) )
	{
		set_pwr_on_end();
		print_dbg( DBG_INFO,1, "############ End of EQP Power ON:[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
	}
	else 
	{
		set_pwr_on_ing();
	}

	return ERR_SUCCESS;
}

/* =========================================================================
   - 서버로부터 CTRL RESP를 받고 마지막 단계의 RESP인경우 ON Process를
     종료 한다. 
   ========================================================================*/
static int eqp_pwr_on_proc_3( void )
{
	return ERR_SUCCESS;
}

/*==========================================================================
  Off Process  
  =========================================================================*/
/* =========================================================================
   RADIO에게 OFF 명령어를 보낸다
   ========================================================================*/
static int eqp_pwr_off_proc_1( void )
{
	int ret = ERR_SUCCESS;

	print_proc( 0, 1 );
	
	ret = send_radio_pwr_start( "OFF" );
	
	if ( ret == ERR_SUCCESS )
	{
		/* EQP 전원 OFF 요청 시작 시간 */
		g_pwr_sts.set_eoff_step1_st_time = get_sectick();
	}

	return ret;
}

/* =========================================================================
   - RADIO로 부터 OFF에 대한 RESP 가 넘어 올경우  
   - MAX STEP과 CUR STEP + 1 가 같을 경우 RADIO는 모든 OFF처리가 종료 
     -> 서버로 부터 RES ACK가 오면 전원 제어 함
	 -> 감시장치에 PING을 보내기 시작한다. 
	 -> 쓰레드를 이용한 자동 처리 과정 시작( pwr_mng_pthread_idle );
 ==========================================================================*/
static int set_pwr_off_end( void )
{
	/* EQP 전원 OFF 요청 시작 시간 초기화 */
	g_pwr_sts.set_eoff_step1_st_time = 0;

	/* EQP 전원 OFF 응답 시간 설정 */
	g_pwr_sts.set_eoff_step1_end_time = get_sectick();

	/* Last Step */
	g_pwr_sts.eqp_last_step = P_OK;

	/* Start Ping */
	g_pwr_sts.start_ping = P_OK;

	/* 전원 제어 OFF의 2단계 처리로 격상 함*/
	g_pwr_sts.proc_step  = P_STEP_2;

	print_dbg( DBG_INFO,1, "######## Start of OFF STEP 2 ######## ");

	return ERR_SUCCESS;
}

static int eqp_pwr_off_proc_2( fclt_ctrl_eqponoffrslt_t * ptronoff )
{
	int curstep;

	//maxstep		= ptronoff->emaxstep;
	curstep 	= ptronoff->ecurstep;
	
	print_proc( 0, 2 );

	//if ( (maxstep -1 ) == (curstep ) )
	if ( curstep == 1 )
	{
		set_pwr_off_end();
	}
	else 
	{
		//g_pwr_sts.eqp_last_step = P_NO;
	}

	return ERR_SUCCESS;
}

static int eqp_pwr_off_proc_3( void )
{
	return ERR_SUCCESS;
}
/*
  OP에서  전원 제어 요청을 받을 경우 정보를 복사 해 놓고 server PWR ACK가 올때까지 기다린다 
 */
static int recv_ctrl_eqp_pwr_req( UINT16 inter_msg, void * ptrdata )
{
	int fcltcode 	= 0;
	int ret			= ERR_RETURN;
	char szip[ MAX_IP_SIZE ];

	inter_msg_t * ptrmsg =NULL;

	ptrmsg = ( inter_msg_t *)ptrdata;
	g_clientid = ptrmsg->client_id;

	if ( ptrmsg->ptrbudy == NULL )
    {
        print_dbg( DBG_ERR,1, "is budy [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
        return ERR_RETURN;
    }

	memset( &g_pwr_sts, 0, sizeof( g_pwr_sts ));
    memset( &g_pwr_sts.eqp_onoff, 0, sizeof(fclt_ctrl_eqponoff_t ));
    memcpy(&g_pwr_sts.eqp_onoff, ptrmsg->ptrbudy , sizeof(fclt_ctrl_eqponoff_t));
    g_pwr_sts.ecurstatus = EFCLTCTLST_START;
    fcltcode = g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.efclt;

    g_pwr_sts.clientid	= g_clientid;
    g_pwr_sts.fcltcode  = fcltcode;
    g_pwr_sts.automode  = g_pwr_sts.eqp_onoff.bautomode;
    g_pwr_sts.onoff		= g_pwr_sts.eqp_onoff.ctl_base.ectlcmd;
	g_pwr_sts.proc_sts 	= EFCLTCTLST_START;
	g_pwr_sts.proc_step = P_STEP_1;

    if ( fcltcode == g_fre_fclt_code )
    {
        g_pwr_sts.snsr_sub_type = ESNSRSUB_EQPFRE_POWER;
	}
	else if ( fcltcode == g_sre_fclt_code )
	{
		g_pwr_sts.snsr_sub_type = ESNSRSUB_EQPSRE_POWER;
	}
	else if ( fcltcode == g_fde_fclt_code )
	{
		g_pwr_sts.snsr_sub_type = ESNSRSUB_EQPFDE_POWER;
	}

	/* 설비코드가 맞지않으면 에러처리 */
	if( ( fcltcode != g_fre_fclt_code ) && 
			( fcltcode != g_sre_fclt_code ) && 
			(fcltcode != g_fde_fclt_code))
	{
		ret = ERR_RETURN;
		memset(&g_pwr_sts, 0, sizeof( g_pwr_sts ));
		print_dbg( DBG_ERR,1, "Invalid FclitCode %d, [%s:%d:%s]",fcltcode, __FILE__, __LINE__,__FUNCTION__);
	}
	else
		ret = ERR_SUCCESS;

	if ( ret == ERR_SUCCESS )
	{

		/* 네트워크 장애 시 알수 없음 에러를 보낸 다 */
		if ( g_pwr_sts.onoff== EFCLTCTLCMD_OFF && get_fclt_netsts_ctrl( fcltcode ) == ESNSRCNNST_ERROR )
		{
			print_dbg( DBG_ERR,1, "Can not EQP POWER Control Becase of EQP NETWORK ERROR( %d ), [%s:%d:%s]",fcltcode, __FILE__, __LINE__,__FUNCTION__);
			recv_pwr_ctrl_release_pwr_mng( EEQPONOFFRSLT_EQPDISCONNECTED, P_STEP_1, MAX_PROC_STEP );
			return ERR_RETURN;
		}

		/* 시리얼 통신  장애 시 알수 없음 에러를 보낸 다 */
		if ( get_sr_netsts_ctrl( fcltcode ) == ESNSRCNNST_ERROR )
		{
			print_dbg( DBG_ERR,1, "Can not EQP POWER Control Becase of POWER CTRL TRANS ERROR(%d),[%s:%d:%s]",fcltcode,__FILE__,__LINE__,__FUNCTION__);
			recv_pwr_ctrl_release_pwr_mng( EEQPONOFFRSLT_RTUPWCTRLLR_NOTCNN, P_STEP_1 , MAX_PROC_STEP );
			return ERR_RETURN;
		}

		print_dbg( DBG_LINE, 1, NULL);
#if 0
		print_dbg( DBG_INFO, 1, "############ Start of EQP Power Control From OP FcltCode:%d, Auto:%d, OnOff:%d ", 
				g_pwr_sts.fcltcode,
				g_pwr_sts.automode,
				g_pwr_sts.onoff);
#endif

		if ( get_pwr_mthod_data_ctrl( g_pwr_sts.snsr_sub_type , &g_pwr_mthod ) != ERR_SUCCESS )
		{
			print_dbg( DBG_ERR,1,"Can not Find FCLT Method Inof sensor sub type:%d [%s:%d:%s]",g_pwr_sts.snsr_sub_type,__FILE__,__LINE__,__FUNCTION__);
			recv_pwr_ctrl_release_pwr_mng( EEQPONOFFRSLT_UNKNOWN, 1, MAX_PROC_STEP );
			return ERR_RETURN;
		}

		if ( g_pwr_mthod.cnt == 0 )
		{
			print_dbg( DBG_ERR, 1, "Can not find FCLT Method PWR Port SiteID:%d, Sensor Type:%d[%s:%d:%s]", get_my_siteid_shm(), g_pwr_sts.snsr_sub_type,__FILE__, __LINE__,__FUNCTION__  );
		}

		init_pwr_mthod();

		/* 감시 장치 진행 단계를 1단계로 설정 */
		g_pwr_sts.proc_sts 	= EFCLTCTLST_ING;

		if ( g_pwr_sts.onoff == EFCLTCTLCMD_OFF )
		{
			/* radio 전송 */
			ret = eqp_pwr_off_proc_1();
			/* 1 - > 3 */
		}
		else
		{
			/* 전원포트 ON 제어 */
			ret = eqp_pwr_on_proc_1();
		}

		if ( ret == ERR_SUCCESS )
		{
			/* 서버 전송 */
			//printf("=======kkkkkkkkkk1\r\n");
			memset( szip, 0, sizeof( szip ));
			cnv_addr_to_strip( g_pwr_sts.eqp_onoff.ctl_base.op_ip, szip );
			//printf("=======kkkkkkkkkk2\r\n");

			print_dbg( DBG_LINE, 1, NULL );
			print_dbg( DBG_INFO,1, "######## Start of EQP PWR !! Recv EQP Power (%d) Control From OP:%d( IP:%s), - STEP 1 - [%s:%d:%s]"
					,g_pwr_sts.onoff, g_clientid,szip, __FILE__, __LINE__,__FUNCTION__);
			ret = send_eqp_pwr_data_ctrl( inter_msg, ptrdata, SEND_FMS );
			print_dbg( DBG_INFO,1, "Send Power Control to FMS:%d, [%s:%d:%s]",g_clientid, __FILE__, __LINE__,__FUNCTION__);
		}
	}
	else
	{
		recv_pwr_ctrl_release_pwr_mng( EEQPONOFFRSLT_UNKNOWN, 1, MAX_PROC_STEP );
	}

	return ret;
}

/* PWR ACK가 서버로 부터 넘어 오면 전원 제어 프로세스를 시작한다 */
static int recv_ctrl_eqp_pwr_ack( UINT16 inter_msg, void * ptrdata )
{
	int ret			= 0;

	inter_msg_t * ptrmsg =NULL;
	fclt_svrtransactionack_t trans_ack;

	ptrmsg = ( inter_msg_t *)ptrdata;

	if ( ptrmsg->ptrbudy == NULL )
	{
		print_dbg( DBG_ERR,1, "is budy [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memset( &trans_ack, 0, sizeof( trans_ack ));
	memcpy( &trans_ack, ptrmsg->ptrbudy , sizeof( trans_ack ));

	if ( trans_ack.iresult != EFCLTERR_NOERROR)
	{
		print_dbg( DBG_ERR,1, "Error of FMS Power ACK:%d [%s:%d:%s]", trans_ack.iresult, __FILE__, __LINE__,__FUNCTION__);
		memset(&g_pwr_sts, 0, sizeof( g_pwr_sts ));
		return ERR_RETURN;
	}
	print_dbg( DBG_INFO ,1,"Recv Power ACK From FMS Server");

	return ret;
}

/* Server에서 RES ACK가 넘어 올 경우  전원 ON/OFF를 처리한다 */
static int recv_ctrl_eqp_res_ack( UINT16 inter_msg, void * ptrdata )
{
	inter_msg_t * ptrmsg =NULL;
	fclt_svrtransactionack_t trans_ack;

	ptrmsg = ( inter_msg_t *)ptrdata;

	if ( ptrmsg->ptrbudy == NULL )
	{
		print_dbg( DBG_ERR,1, "is budy [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memset( &trans_ack, 0, sizeof( trans_ack ));
	memcpy( &trans_ack, ptrmsg->ptrbudy , sizeof( trans_ack ));

	if ( trans_ack.iresult != EFCLTERR_NOERROR)
	{
		print_dbg( DBG_ERR,1, "Error of FMS Power ACK:%d [%s:%d:%s]", trans_ack.iresult, __FILE__, __LINE__,__FUNCTION__);
		memset(&g_pwr_sts, 0, sizeof( g_pwr_sts ));
		return ERR_RETURN;
	}
	
	print_dbg( DBG_INFO,1, "Recv CTRL RESP ACK from FMS :%d [%s:%d:%s]", trans_ack.iresult, __FILE__, __LINE__,__FUNCTION__);

	if ( g_pwr_sts.onoff == EFCLTCTLCMD_OFF )
	{
		/* 전원 제어기 제어 , 최종 상태 서버 보고 ,최종 결과 ACK인 경우 종료 처리 */
		eqp_pwr_off_proc_3( );
	}
	else
	{
		eqp_pwr_on_proc_3();
	}

	return ERR_SUCCESS;
}

/*==========================================================================
  radio -> power manager 
  =========================================================================*/
static int recv_radio_eqp_pwr_resp( UINT16 inter_msg, void * ptrdata )
{
	fclt_ctrl_eqponoffrslt_t onoff_resp;
	fclt_ctrl_eqponoffrslt_t * ptronoff_resp = NULL;

	int fcltcode;
	int onoff;
	int maxstep;
	int curstep;
	int cursts;
	int ret = 0;
	inter_msg_t * ptrmsg= NULL;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "isnull ptrdata [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrmsg = ( inter_msg_t *)ptrdata;

	memset( &onoff_resp, 0, sizeof( onoff_resp));
	if ( ptrmsg->ptrbudy == NULL )
	{
		print_dbg( DBG_ERR,1, "isnull ptrdata [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memcpy( &onoff_resp, ptrmsg->ptrbudy, sizeof( onoff_resp ));
	ptronoff_resp = (fclt_ctrl_eqponoffrslt_t * )ptrmsg->ptrbudy;

	fcltcode 	= onoff_resp.ctl_base.tfcltelem.efclt;
	onoff		= onoff_resp.ctl_base.ectlcmd;
	maxstep		= onoff_resp.emaxstep;
	curstep 	= onoff_resp.ecurstep;
	cursts		= onoff_resp.ecurstatus;
    g_pwr_sts.ecurstatus = cursts;

	g_pwr_sts.maxstep = maxstep;
	g_pwr_sts.curstep = curstep;

	print_dbg( DBG_LINE, 1, NULL );
	print_dbg( DBG_INFO,1,"Recv PWR Manager from RADIO ::EQP Status FcltCode:%d, ONOFF:%d, MaxStep:%d, CurStep:%d, CurSts:%d",
			fcltcode, onoff, maxstep, curstep, cursts );

	/* 감시 장비에서 에러로 처리 할 경우 종료 한다. off일 경우 step 1, on 일 경우 step 2  */
	if ( onoff_resp.ersult == EEQPONOFFRSLT_EQPERROR )
	{
		if( onoff == EFCLTCTLCMD_OFF )
			curstep = P_STEP_1;
		else
			curstep = curstep;

		print_dbg( DBG_ERR,1, "######## End of EQP PWR Process Because of Error Result( %d ), [%s:%d:%s]",g_pwr_sts.fcltcode, __FILE__, __LINE__,__FUNCTION__);
		recv_pwr_ctrl_release_pwr_mng( EEQPONOFFRSLT_EQPERROR , curstep, MAX_PROC_STEP );

		return ERR_RETURN;
	}

	if ( onoff != g_pwr_sts.onoff )
	{
		print_dbg( DBG_INFO,1,"Is not same ONOFF Status Manager ONOFF:%d, EQP ONOFF:%d",g_pwr_sts.onoff, onoff );
		
	}

	/* radion로 부터 resp를 받을 경우 최대 4단계 */
	if( onoff == EFCLTCTLCMD_OFF )
	{
		eqp_pwr_off_proc_2( &onoff_resp  );
	}
	else
	{
		eqp_pwr_on_proc_2( &onoff_resp  );
        if( onoff_resp.emaxstep == onoff_resp.ecurstep)
        {
            /* on 일 경우 무조건 제어 상태를 해제한다 */
            onoff_resp.ersult   = EEQPONOFFRSLT_OK;

			/* On Step을 초기화 한다 */
			g_pwr_sts.proc_step 	= P_STEP_0;
			g_pwr_sts.proc_error 	= P_ERR_NO;
			g_pwr_sts.proc_sts 		= EFCLTCTLST_NONE;

			if ( ptronoff_resp != NULL )
			{
				ptronoff_resp->ecurstatus = EFCLTCTLST_END;
			}

            ptrmsg->ndata = RELEASE_MODE;
        }
	}

	sys_sleep( 1000 );

	if ( ptronoff_resp != NULL )
	{
		ptronoff_resp->ctl_base.op_ip 	= g_pwr_sts.eqp_onoff.ctl_base.op_ip;
		memcpy( ptronoff_resp->ctl_base.suserid , g_pwr_sts.eqp_onoff.ctl_base.suserid , MAX_USER_ID);
	}

	ret = send_eqp_pwr_data_ctrl( inter_msg, ptrdata, SEND_FMS );
	print_dbg( DBG_INFO,1, "Send EQP RESP DATA to Server:%d", ret );

	return ERR_SUCCESS;
}

/* 
   RADIO로부터 POWER제어 요청을 받음
   자동 모드 
*/
static int recv_radio_eqp_pwr_req( UINT16 inter_msg, void * ptrdata )
{
	int fcltcode 	= 0;
	int ret			= ERR_RETURN;
	int clientid	= 0;

	inter_msg_t * ptrmsg =NULL;

	ptrmsg = ( inter_msg_t *)ptrdata;
	clientid = ptrmsg->client_id;

	if ( g_pwr_sts.proc_step != 0 )
	{
		print_dbg( DBG_ERR,1, "Already EQP running.", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	if ( ptrmsg->ptrbudy == NULL )
	{
		print_dbg( DBG_ERR,1, "is budy [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memset( &g_pwr_sts, 0, sizeof( g_pwr_sts ));
	memset( &g_pwr_sts.eqp_onoff, 0, sizeof(fclt_ctrl_eqponoff_t ));
	//memset( &g_auto_eqp_onoff, 0, sizeof(fclt_ctrl_eqponoff_t ));

	memcpy(&g_pwr_sts.eqp_onoff, ptrmsg->ptrbudy , sizeof(fclt_ctrl_eqponoff_t));
	//memcpy(&g_auto_eqp_onoff, ptrmsg->ptrbudy , sizeof(fclt_ctrl_eqponoff_t));
    g_pwr_sts.ecurstatus = EFCLTCTLST_START;

    g_pwr_sts.eqp_onoff.ctl_base.ectlcmd = EFCLTCTLCMD_OFF;
	fcltcode = g_pwr_sts.eqp_onoff.ctl_base.tfcltelem.efclt;
	
	g_pwr_sts.clientid	    = clientid;
	g_pwr_sts.fcltcode      = fcltcode;
	g_pwr_sts.automode      = 1;
	g_pwr_sts.onoff		    = g_pwr_sts.eqp_onoff.ctl_base.ectlcmd;

	g_pwr_sts.proc_sts 		= EFCLTCTLST_START;
	g_pwr_sts.proc_step 	= P_STEP_1;

	if ( fcltcode == g_fre_fclt_code )
	{
		g_pwr_sts.snsr_sub_type = ESNSRSUB_EQPFRE_POWER;
	}
	else if ( fcltcode == g_sre_fclt_code )
	{
		g_pwr_sts.snsr_sub_type = ESNSRSUB_EQPSRE_POWER;
	}
	else if ( fcltcode == g_fde_fclt_code )
	{
		g_pwr_sts.snsr_sub_type = ESNSRSUB_EQPFDE_POWER;
	}

    /* 설비코드가 맞지않으면 에러처리 */
    if( ( fcltcode != g_fre_fclt_code ) && 
            ( fcltcode != g_sre_fclt_code ) && 
            (fcltcode != g_fde_fclt_code))
    {
        ret = ERR_RETURN;
        memset(&g_pwr_sts, 0, sizeof( g_pwr_sts ));
        print_dbg( DBG_ERR,1, "Invalid FclitCode %d, [%s:%d:%s]",fcltcode, __FILE__, __LINE__,__FUNCTION__);
    }
    else
    {
        ret = ERR_SUCCESS;
    }

    if ( ret == ERR_SUCCESS )
    {
		/* 시리얼 통신  장애 시 알수 없음 에러를 보낸 다 */
		if ( get_sr_netsts_ctrl( fcltcode ) == ESNSRCNNST_ERROR )
		{
			print_dbg( DBG_ERR,1, "Can not EQP POWER Control Becase of POWER CTRL TRANS ERROR(%d),[%s:%d:%s]",fcltcode,__FILE__,__LINE__,__FUNCTION__);
			recv_pwr_ctrl_release_pwr_mng( EEQPONOFFRSLT_RTUPWCTRLLR_NOTCNN, P_STEP_1 , MAX_PROC_STEP );
			return ERR_RETURN; 
		}

        print_dbg( DBG_LINE, 1, NULL );
        print_dbg( DBG_INFO,1, "########## Start EQP PWR !! Recv EQP Power Control From EQP (AutoMode):%d, [%s:%d:%s]",clientid, __FILE__, __LINE__,__FUNCTION__);
        
        /* auto mode 시작 시 중복 제어 방지를 위해 9를 control, transfer로 던진다 */
        ptrmsg->ndata = AUTO_MODE;

        //ret = send_eqp_pwr_data_ctrl( inter_msg, ptrdata, SEND_FMS );
        print_dbg( DBG_INFO,1, "Send Power Control to FMS:%d, [%s:%d:%s]",clientid, __FILE__, __LINE__,__FUNCTION__);

        if ( get_pwr_mthod_data_ctrl( g_pwr_sts.snsr_sub_type , &g_pwr_mthod ) != ERR_SUCCESS )
        {
            print_dbg( DBG_ERR,1, "Can not Find FCLT Method Inof sensor sub type:%d [%s:%d:%s]", g_pwr_sts.snsr_sub_type, __FILE__, __LINE__,__FUNCTION__);
			recv_pwr_ctrl_release_pwr_mng( EEQPONOFFRSLT_UNKNOWN, 1, MAX_PROC_STEP );
            return ERR_RETURN;
        }

        if ( g_pwr_mthod.cnt == 0 )
        {
            print_dbg( DBG_ERR, 1, "Can not find FCLT Method PWR Port SiteID:%d, Sensor Type:%d[%s:%d:%s]", get_my_siteid_shm(), g_pwr_sts.snsr_sub_type,__FILE__, __LINE__,__FUNCTION__  );
        }

        init_pwr_mthod();

		/* 자동 모드 STEP1로 설정함. */
		g_pwr_sts.proc_sts 	= EFCLTCTLST_ING;

        if ( g_pwr_sts.onoff == EFCLTCTLCMD_OFF )
        {
            /* radio 전송 */
            ret = eqp_pwr_off_proc_1();
            /* 1 - > 3 */
        }
        else
        {
            /* 전원포트 ON 제어 */
            ret = eqp_pwr_on_proc_1();
        }


        /* 서버 전송 */
		if ( ret == ERR_SUCCESS )
		{
			print_dbg( DBG_LINE, 1, NULL );
			print_dbg( DBG_INFO,1, "Start EQP PWR !! Recv EQP Power Control From Radio - STEP 1 - :%d, [%s:%d:%s]",clientid, __FILE__, __LINE__,__FUNCTION__);
			ret = send_eqp_pwr_data_ctrl( inter_msg, ptrdata, SEND_FMS );
			print_dbg( DBG_INFO,1, "Send Power Control to FMS:%d, [%s:%d:%s]",clientid, __FILE__, __LINE__,__FUNCTION__);
		}
		else
		{
			print_dbg( DBG_ERR,1, "Cannot Start EQP PWR !! Recv EQP Power Control From Radio - STEP 1 - :%d, [%s:%d:%s]",clientid, __FILE__, __LINE__,__FUNCTION__);
		}
        return ret;
    }
	else
	{
		recv_pwr_ctrl_release_pwr_mng( EEQPONOFFRSLT_UNKNOWN, 1, MAX_PROC_STEP );
	}
    return ret;
}

static void *  pwr_mng_pthread_idle( void * ptrdata )
{
    ( void ) ptrdata;
    int status = 0;

	g_end_pwr_mng_pthread = 0;

	while( g_release == 0 && g_start_pwr_mng_pthread == 1 )
	{
		sys_sleep( 1000 );

		/* ping 단계 처리 */
		check_ping_start( );
		sys_sleep( 500 );

		/* 전원 제어 시작 단계 */
		check_pwr_control_start( );
		sys_sleep( 500 );

		/* 전원 제어 응답 확인 */
		check_pwr_control_next( );
		sys_sleep( 500 );

		/* 전원 제어 완료 확인 */
		check_pwr_control_complete();
		sys_sleep( 500 );

		check_auto_mode();

		/* for arp test */
		//run_arp( EFCLT_EQP01 );
	}

	if( g_end_pwr_mng_pthread == 0 )
	{
		pthread_join( pwr_mng_pthread , ( void **)&status );
		g_start_pwr_mng_pthread 	= 0;
		g_end_pwr_mng_pthread 	= 1;
	}

	return ERR_SUCCESS;
}

static int create_timer_thread( void )
{
	int ret = ERR_SUCCESS;

	if ( g_release == 0 && g_start_timer_pthread == 0 && g_end_timer_pthread == 1 )
	{
		g_start_timer_pthread = 1;

		if ( pthread_create( &timer_pthread, NULL, timer_pthread_idle , NULL ) < 0 )
		{
			g_start_timer_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create Timer Thread" );
			ret = ERR_RETURN;
		}
		else
		{
			//print_dbg( DBG_INIT, 1, "mini-SEED Thread Create OK. ");
		}
	}

	return ret;
}

static int create_pwr_mng_thread( void )
{
	int ret = ERR_SUCCESS;

	if ( g_release == 0 && g_start_pwr_mng_pthread == 0 && g_end_pwr_mng_pthread == 1 )
	{
		g_start_pwr_mng_pthread = 1;

		if ( pthread_create( &pwr_mng_pthread, NULL, pwr_mng_pthread_idle , NULL ) < 0 )
		{
			g_start_pwr_mng_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create PWR MNG Thread" );
			ret = ERR_RETURN;
		}
		else
		{
			//print_dbg( DBG_INIT, 1, "mini-SEED Thread Create OK. ");
		}
	}

	return ret;
}

static int onoff_option_default( void )
{
	int i ;

	memset(&g_pwr_off_opt, 0, sizeof( g_pwr_off_opt ));
	memset(&g_pwr_on_opt, 0, sizeof( g_pwr_on_opt ));

	/* on option default */
	for( i = 0 ; i < MAX_PWR_SWITCH_CNT ; i++ )
	{
		g_pwr_on_opt.idelay_powerswitch_ms[ i ] = 7 * MS;	/* default */ 
	}
	g_pwr_on_opt.iwaittime_eqposstart_ms 	= 30 * MS;
	g_pwr_on_opt.iwaittime_eqpdmncnn_ms		= 60 * MS;
	g_pwr_on_opt.iwaittime_step1_ms			= 30 * MS;
	g_pwr_on_opt.iwaittime_step2_ms			= 80 * MS;

	/* off option default */
	for( i = 0 ; i < MAX_PWR_SWITCH_CNT ; i++ )
	{
		g_pwr_off_opt.idelay_powerswitch_ms[ i ] = 7 * MS;	/* default */ 
	}

	g_pwr_off_opt.iwaittime_step1_ms			= 70 * MS;
	g_pwr_off_opt.iwaittime_eqposend_ms			= 60 * MS;

	return ERR_SUCCESS;
}

/*================================================================================================
내부   함수  정의 
================================================================================================*/
static int internal_init( void )
{
	int ret =0;

	memset(&g_pwr_sts, 0, sizeof( g_pwr_sts ));
	memset(&g_pwr_mthod, 0, sizeof( g_pwr_mthod ));

	g_ctrl_func_cnt = ( sizeof( ctrl_proc_list ) / sizeof( proc_func_t ));
	g_radio_func_cnt = ( sizeof( radio_proc_list ) / sizeof( proc_func_t ));
	
	ret = create_pwr_mng_thread();
	
	if ( ret == ERR_SUCCESS )
		ret = create_timer_thread();

	if ( ret == ERR_SUCCESS )
		print_dbg( DBG_INFO, 1, "Success of PWR Manager");

	onoff_option_default();

	return ret;
}

static int internal_idle( void  )
{
	create_pwr_mng_thread();
	create_timer_thread();
	
	return ERR_SUCCESS;
}

/* pwr_mng module의 release 내부 작업 */
static int internal_release( void )
{
	g_release = 1;
	print_dbg( DBG_INFO, 1, "Release of PWR Manager");
	return ERR_SUCCESS;
}

/*================================================================================================
외부  함수  정의 
================================================================================================*/
int init_pwr_mng( void )
{
	internal_init();

	return ERR_SUCCESS;
}

int idle_pwr_mng( void * ptr )
{
	( void )ptr;

	return internal_idle( );
}

int release_pwr_mng( void )
{
	return internal_release(); 
}

int recv_ctrl_data_pwr_mng( unsigned short inter_msg, void * ptrdata )
{
	return internal_recv_ctrl_data( inter_msg, ptrdata );
}

int recv_pwr_ctrl_resp_pwr_mng( unsigned short inter_msg, void * ptrdata )
{
	return internal_recv_radio_data( inter_msg, ptrdata );
}

int recv_pwr_ctrl_release_pwr_mng( int errcode, int err_step, int max_step )
{
	int ret = ERR_SUCCESS;
    /*현재 상태의 전원 상태에 에러를 처리한다. */
    /* 초기화 한다 */
    /* 에러 임을 보낸다 */

	/* Auto mode는 총 4단계이다 */
	recal_step( &max_step, &err_step );

    ret = send_error_eqp_ctl_resp( errcode, err_step, max_step, g_pwr_sts.proc_sts, 1 );
	memset(&g_pwr_sts, 0, sizeof( g_pwr_sts ));
   
	print_dbg( DBG_ERR,1,"######## Force End of EQP Power Process ########" );
    return ret;
}


int set_ctrl_pwr_c_data_pwr_mng( void * ptrpwr_c )
{
	pwr_c_data_t * ptrpwr_data;

	if ( ptrpwr_c == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrpwrc is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrpwr_data = ( pwr_c_data_t *)ptrpwr_c;

	return internal_set_ctrl_pwr_c_data( ptrpwr_data );
}

int set_ctrl_mcu_etc_data_pwr_mng( unsigned char * ptrdoe  )
{
	if ( ptrdoe == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdoe is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	return internal_set_ctrl_mcu_etc_data( ptrdoe );
}

int recv_ctrl_data_pwr_off_opt_mng( unsigned short inter_msg, void * ptrdata )
{
	return internal_recv_pwr_off_option( inter_msg, ptrdata );
}

int recv_ctrl_data_pwr_on_opt_mng( unsigned short inter_msg, void * ptrdata )
{
	return internal_recv_pwr_on_option( inter_msg, ptrdata );
}

int recv_ctrl_data_eqp_status_mng( unsigned short inter_msg, void * ptrdata )
{
	return internal_recv_eqp_status( inter_msg, ptrdata );
}

