#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>

#include "com_include.h"

#include "fms_client.h"
#include "op_server.h"
#include "update_client.h"

#include "control.h"
#include "radio.h"
#include "gpio_mcu.h"
#include "pwr_c.h"
#include "remote.h"
#include "temp.h"
#include "pwr_m.h"
#include "pwr_mng.h"
#include "ctrl_db.h"
#include "websock.h"
#include "apc_ups.h"

#define DEL_CACH_TIME		( 1 * 60 * 60 )		/* 1 hour */
#define CHK_TIME			( 1 * 60 * 10 )		/* 10 min */
//#define ONE_DAY			( 60 * 60 * 24 )
#define ONE_DAY				( 60 * 60 * 5 )
#define MIN_5				( 60 * 5 )

#define STR_DEL_CACH		"sudo sync & echo 1 > /proc/sys/vm/drop_caches"
/*================================================================================================
 전역 변수 
================================================================================================*/

static volatile int main_done 			= 0;
static volatile int g_system_init 		= 0;
static volatile int max_call_func_cnt 	= 0;

static volatile time_t g_cur_time = 0;
static volatile time_t g_old_time = 0;
static volatile time_t g_chk_time = 0;

static char g_sztime [ MAX_TIME_SIZE ];

static int main_idle( void );

static call_func_t call_func_list[ ] =
{
	{ init_fms_client, 			release_fms_client,	NULL },
	{ init_op_server, 			release_op_server,	NULL },
	{ init_update_client, 		release_update_client,	NULL },
	{ init_control, 			release_control,	idle_control},
	{ init_mcu, 				release_mcu, 		NULL		},
	{ init_pwr_c, 				release_pwr_c,		NULL		},
	{ init_remote, 				release_remote,		NULL		}, 
	{ init_temp, 				release_temp,		NULL		}, 
	{ init_pwr_m, 				release_pwr_m, 		NULL		}, 
	{ init_radio, 				release_radio,		NULL },
	{ init_apc_ups, 			release_apc_ups,	NULL },
	{ NULL,						release_pwr_mng,	idle_pwr_mng },
	{ NULL,						release_websock,	idle_websock },
	{ NULL, NULL, NULL }
};

/*================================================================================================
  내부 함수선언
================================================================================================*/
static int main_release( void );
static int main_loop( void );
static int check_cach_del_time( void );

/* thread( main  )*/
static volatile int g_start_main_pthread 	= 0; 
static volatile int g_end_main_pthread 	= 1; 
static pthread_t main_pthread;
static void * main_pthread_idle( void * ptrdata );

/* thread( main  )*/
static volatile int g_start_etc_pthread 	= 0; 
static volatile int g_end_etc_pthread 	= 1; 
static pthread_t etc_pthread;
static void * etc_pthread_idle( void * ptrdata );

/* thread( ntp  )*/
static volatile int g_start_ntp_pthread 	= 0; 
static volatile int g_end_ntp_pthread 	= 1; 
static pthread_t ntp_pthread;
static void * ntp_pthread_idle( void * ptrdata );

/*================================================================================================
내부 함수  정의 
================================================================================================*/
static int check_sys( void )
{
	static char szcmd[ 300 ];

	/* CPU */
	memset( szcmd, 0, sizeof( szcmd ));
	sprintf(szcmd, "sar -p 1 1 | awk '/Average/{print 100-$NF}' > %s", SYS_CPU );
	system_cmd( szcmd );
	sys_sleep( 1000 );

	/* RAM */
	memset( szcmd, 0, sizeof( szcmd ));
	sprintf(szcmd, "sar -r 1 1 | awk '/Average/{print $4}' > %s", SYS_RAM );
	system_cmd( szcmd );
	sys_sleep( 1000 );
	
	/* HARD */
	memset( szcmd, 0, sizeof( szcmd ));
	sprintf(szcmd, "df /dev/sda1 | grep -vE '^Filesystem|tmpfs|cdrom' | awk '{ print $5}' > %s", SYS_HRD );
	system_cmd( szcmd );
	sys_sleep( 1000 );

	print_dbg( DBG_SAVE, 1, "Check of SYSTEM Resource");

	return ERR_SUCCESS;
}

static void *  main_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;
	g_end_main_pthread = 0;

	while( main_done == 0 && g_start_main_pthread == 1 )
	{
		if ( get_rtu_initialize_shm() == 1 )
		{
			main_idle();
			//printf("main....idle...\r\n");
			sys_sleep( 1000 );
		}
	}

	if( g_end_main_pthread == 0 )
	{
		pthread_join( main_pthread , ( void **)&status );
		g_start_main_pthread 	= 0;
		g_end_main_pthread 	= 1;
	}

	return ERR_SUCCESS;
}

static void *  etc_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;
	g_end_etc_pthread = 0;

	while( main_done == 0 && g_start_etc_pthread == 1 )
	{
		if ( get_rtu_initialize_shm() == 1 )
		{
			idle_rtu_fclt_control();
			idle_fms_client( NULL );
			idle_op_server( NULL );
			idle_update_client( NULL );
			//printf("etcn....idle...\r\n");
			sys_sleep( 1000 );
		}
	}

	if( g_end_etc_pthread == 0 )
	{
		pthread_join( etc_pthread , ( void **)&status );
		g_start_etc_pthread 	= 0;
		g_end_etc_pthread 	= 1;
	}

	return ERR_SUCCESS;
}

static void *  ntp_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;
	g_end_ntp_pthread = 0;
	char szntp[ MAX_NTP_ADDR_SIZE ];
	char szcmd[ MAX_PATH_SIZE ];
	UINT32 cur_tick, pre_tick;
	UINT32 int_tick;
	
	UINT32 sys_check_tick = 0;
	UINT32 sys_read_tick = 0;

	pre_tick = 0;
	cur_tick = 0;
	int_tick = 0;

	while( main_done == 0 && g_start_ntp_pthread == 1 )
	{
		//if ( get_rtu_initialize_shm() == 1 )
		{
			/* ntp ntpdate -u xx.xx.xx.xx */
			memset( szntp, 0, sizeof( szntp ));
			memset( szcmd, 0, sizeof( szcmd ));

			cur_tick = get_sectick();
			int_tick = cur_tick - pre_tick;
			
			if ( sys_check_tick == 0 )
				sys_check_tick = cur_tick;
			
			//printf("kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk1\r\n");
			if ( int_tick >=( ONE_DAY ) || pre_tick == 0 )
			{
			//printf("kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk2\r\n");
				if ( get_ntp_addr_shm( szntp ) == ERR_SUCCESS )
				{
			//printf("kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk3\r\n");
					if ( get_rtu_run_mode_shm() != RTU_RUN_SVR_LINK )
					{
						sprintf( szcmd, "ntpdate -u %s", szntp );

						system_cmd( szcmd );
						print_dbg( DBG_LINE, 1, NULL );
						print_dbg( DBG_SAVE, 1, "######### Set of NTP Time :%s", szcmd );
						pre_tick = cur_tick;
					}
				}
			}

			/* system check */
			int_tick = cur_tick - sys_check_tick;
			if ( int_tick >= MIN_5 )
			{
				check_sys();
				sys_check_tick = cur_tick;
			}
			
			int_tick = sys_check_tick - sys_read_tick;
			
			/* system read */
			if ( int_tick >= 30 )
			{
				if ( read_shm_sys_info() == ERR_SUCCESS )
				{
					sys_read_tick = cur_tick;
				}
			}

			sys_sleep( 5000 );
		}
	}

	if( g_end_ntp_pthread == 0 )
	{
		pthread_join( ntp_pthread , ( void **)&status );
		g_start_ntp_pthread 	= 0;
		g_end_ntp_pthread 	= 1;
	}

	return ERR_SUCCESS;
}

static int create_main_thread( void )
{
	int ret = ERR_SUCCESS;

	if ( main_done == 0 && g_start_main_pthread == 0 && g_end_main_pthread == 1 )
	{
		g_start_main_pthread = 1;

		if ( pthread_create( &main_pthread, NULL, main_pthread_idle , NULL ) < 0 )
		{
			g_start_main_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create Main Thread" );
			ret = ERR_RETURN;
		}
		else
		{
			//print_dbg( DBG_INIT, 1, "mini-SEED Thread Create OK. ");
		}
	}

	return ret;
}

static int create_etc_thread( void )
{
	int ret = ERR_SUCCESS;

	if ( main_done == 0 && g_start_etc_pthread == 0 && g_end_etc_pthread == 1 )
	{
		g_start_etc_pthread = 1;

		if ( pthread_create( &etc_pthread, NULL, etc_pthread_idle , NULL ) < 0 )
		{
			g_start_etc_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create ETC Thread" );
			ret = ERR_RETURN;
		}
		else
		{
			//print_dbg( DBG_INIT, 1, "mini-SEED Thread Create OK. ");
		}
	}

	return ret;
}

static int create_ntp_thread( void )
{
	int ret = ERR_SUCCESS;

	if ( main_done == 0 && g_start_ntp_pthread == 0 && g_end_ntp_pthread == 1 )
	{
		g_start_ntp_pthread = 1;

		if ( pthread_create( &ntp_pthread, NULL, ntp_pthread_idle , NULL ) < 0 )
		{
			g_start_ntp_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create NTP Thread" );
			ret = ERR_RETURN;
		}
		else
		{
			//print_dbg( DBG_INIT, 1, "mini-SEED Thread Create OK. ");
		}
	}

	return ret;
}

static int resize_stack( void )
{
	struct rlimit rlim;

	getrlimit( RLIMIT_STACK, &rlim );
	
	print_dbg( DBG_LINE,1,NULL);
	printf( "Current Stack Size : [%lld] Max Current Stack Size : [%lld]\n",(long long) rlim.rlim_cur, (long long)rlim.rlim_max );
	rlim.rlim_cur = (1024 * 1024 * 100);
	rlim.rlim_max = (1024 * 1024 * 100);
	setrlimit( RLIMIT_STACK, &rlim );
	printf( "Current Stack Size : [%lld] Max Current Stack Size : [%lld]\n", (long long)rlim.rlim_cur, (long long)rlim.rlim_max );
	print_dbg( DBG_LINE,1,NULL);

	return ERR_SUCCESS;
}

static int set_keep_alive( void )
{
	char sztemp[ 300 ];

	/* KeepAlive Time : KeepAlive ACK 체크 주기  */
	memset( sztemp, 0, sizeof( sztemp ));
	sprintf(sztemp, "%s", "echo 50 > /proc/sys/net/ipv4/tcp_keepalive_time");
	system_cmd( sztemp );
	sys_sleep( 100 );
	
	/* Interval : KeepAlive time에 ACK가 없을 경우 20초 간격으로 ACK를 보낸다 */
	memset( sztemp, 0, sizeof( sztemp ));
	sprintf(sztemp, "%s", "echo 20 > /proc/sys/net/ipv4/tcp_keepalive_intvl");
	system_cmd( sztemp );
	sys_sleep( 100 );

	/* probes : 3번을 보내도 ACK가 오지 않으면 네트워크오류로 판단 */
	memset( sztemp, 0, sizeof( sztemp ));
	sprintf(sztemp, "%s", "echo 3 > /proc/sys/net/ipv4/tcp_keepalive_probes");
	system_cmd( sztemp );
	sys_sleep( 100 );

	return ERR_SUCCESS;
}

static int make_verify_file( void )
{
	char szpath[ MAX_PATH_SIZE  ];
	BYTE val[ 5 ];

	memset( szpath, 0, sizeof( szpath ));
	memset( val, 0, sizeof( val ));
	val[ 0 ] = CHK_DATA1;
	val[ 1 ] = CHK_DATA2;
	val[ 2] = 0x0A;
	val[ 3] = 0x0D;

	sprintf( szpath, "%s%s", CONF_PART_1, VERIFY_FILE );
	if ( isexist_file( szpath ) == 0 )
	{
		save_binfile( szpath , val, 4 );
	}

	memset( szpath, 0, sizeof( szpath ));
	sprintf( szpath, "%s%s", CONF_PART_2, VERIFY_FILE );
	if ( isexist_file( szpath ) == 0 )
	{
		save_binfile( szpath , val, 4 );
	}

	return ERR_SUCCESS;
}

static int main_idle( void )
{
	int i;

	check_cach_del_time();

	idle tmp_idle ;

	for( i = 0 ; i < max_call_func_cnt ; i++ )
	{
		tmp_idle = call_func_list[ i ].idle_func;
		if ( tmp_idle != NULL )
		{
			tmp_idle( NULL );
		}
		sys_sleep( 200 );
	}

	return ERR_SUCCESS;
}

static int main_release( void )
{
	int i;
	release tmp_release ;

	for( i = 0 ; i < max_call_func_cnt ; i++ )
	{
		tmp_release= call_func_list[ i ].release_func;
		if ( tmp_release != NULL )
		{
			tmp_release(  );
		}
	}

	release_com_conf();
	release_shm();

	return ERR_SUCCESS;
}

static int main_loop( void )
{
	while ( main_done == 0 )
	{
		/* websock이 시작되면 Blocking 됨 */
		//if ( get_rtu_web_shm() == 1 )
#ifdef USE_WEBSOCK 
		init_websock( recv_cmd_data_websock );
#endif
		sys_sleep( 5000 );
	}

	main_release();

	return ERR_SUCCESS;
}

static void sighandler (int sig)
{
	if ((sig == SIGTERM) || (sig == SIGINT))
	{
		if( sig == SIGTERM )
		{
			print_dbg(DBG_INFO, 1, "SIGTREM");
		}
		else
		{
			print_dbg(DBG_INFO, 1, "SIGINIT Ctrl + C");
			main_done = 1;
			release_websock();
			if ( g_system_init == 0 )
			{
				main_release();
			}
		}
	}
}

static void init_sighandler(void)
{
	struct sigaction act;

	act.sa_handler = sighandler;
	sigemptyset( &act.sa_mask );
	act.sa_flags = 0;
	sigaction( SIGTERM, &act, 0 ); /* Terminate application */
	sigaction( SIGINT, &act, 0 ); /* control tty input ^C */
}

static int print_time( int flag, time_t cur_time )
{
	memset( g_sztime, 0, sizeof( g_sztime ));
	get_cur_time( cur_time , g_sztime, NULL );

	print_dbg(DBG_LINE, 1, NULL );
	print_dbg(DBG_NONE,1,"Current Time: %s", g_sztime );

	if ( flag ==  0 )
	{
		g_chk_time = time( NULL );
	}

	print_dbg(DBG_LINE, 1, NULL );

	return ERR_SUCCESS;
}

static int check_cach_del_time( void )
{
	if ( g_cur_time == 0 )
	{
		g_cur_time = time( NULL );
		g_old_time = time( NULL );
		g_chk_time = time( NULL );	
		print_time( 0 , g_chk_time );

		return ERR_SUCCESS;
	}

	g_cur_time = time( NULL );

	if ( g_cur_time - g_chk_time >= CHK_TIME )
	{
		print_time( 0 , g_cur_time );
	}
	else if ( g_cur_time - g_old_time >= DEL_CACH_TIME )
	{
		system_cmd( STR_DEL_CACH );
		print_time( 1 , g_cur_time );
		g_old_time = g_cur_time;
	}

	return ERR_SUCCESS;
}

static int main_pre_init( void )
{
	max_call_func_cnt = ( sizeof( call_func_list ) / sizeof( call_func_t ));
	return ERR_SUCCESS;
}

static int main_init( void )
{
	char sztime[ MAX_TIME_SIZE ];
	time_t cur_time;

	int bfms_client, bop_svr, bupdate;
#if 1
	int bctrl, bradio, bgpio, bpwr_c, bremote, btemp, bpwr_m;
#endif
	memset( sztime, 0, sizeof( sztime ));

	create_ntp_thread();
	sys_sleep( 3000 );

	cur_time = time( NULL );
	get_cur_time( cur_time , sztime, NULL );

	print_dbg( DBG_LINE, 1, NULL );
	print_dbg( DBG_NONE, 1, "Initialization Of Logger Application: %s", sztime );
	print_dbg( DBG_LINE, 1, NULL );

	sys_sleep( 500 );
	/* DB 초기화 */
	bupdate			= init_ctrl_db( );
	if ( bupdate != ERR_SUCCESS )
				print_dbg(DBG_NONE, 1, "Ctrl DB  Module Initialization Fail ");

	bctrl 	= init_control( NULL );
	if ( bctrl != ERR_SUCCESS )
	{
		print_dbg(DBG_NONE, 1, "Control Module Initialization Fail ");
		return ERR_RETURN;
	}

	bgpio	= init_mcu( recv_sns_data_sensor );
	if ( bgpio != ERR_SUCCESS )
				print_dbg(DBG_NONE, 1, "MCU Monitor Module Initialization Fail ");

	bpwr_c	= init_pwr_c( recv_sns_data_sensor );
	if ( bpwr_c != ERR_SUCCESS )
				print_dbg(DBG_NONE, 1, "Power Control Module Initialization Fail ");

	bremote	= init_remote(recv_sns_data_sensor  );
	if ( bremote != ERR_SUCCESS )
				print_dbg(DBG_NONE, 1, "Remote Control Module Initialization Fail ");

	btemp	= init_temp( recv_sns_data_sensor );
	if ( btemp != ERR_SUCCESS )
				print_dbg(DBG_NONE, 1, "Temprature Module Initialization Fail ");

	bpwr_m	= init_pwr_m(recv_sns_data_sensor  );
	if ( bpwr_m != ERR_SUCCESS )
				print_dbg(DBG_NONE, 1, "Power Monitor Module Initialization Fail ");

	bradio	= init_radio( recv_sns_data_sensor  );
	if ( bradio != ERR_SUCCESS )
				print_dbg(DBG_NONE, 1, "Radio Monitor Module Initialization Fail ");

	bpwr_m	= init_apc_ups( recv_sns_data_sensor );
	if ( bpwr_m != ERR_SUCCESS )
				print_dbg(DBG_NONE, 1, "APC UPS  Module Initialization Fail ");

	bpwr_m	= init_pwr_mng( );
	if ( bpwr_m != ERR_SUCCESS )
				print_dbg(DBG_NONE, 1, "Power Manager  Module Initialization Fail ");

	bfms_client 	= init_fms_client( recv_cmd_data_transfer );
	if ( bfms_client != ERR_SUCCESS )
				print_dbg(DBG_NONE, 1, "FMS Client Module Initialization Fail ");

	bop_svr			= init_op_server( recv_cmd_data_transfer );
	if ( bop_svr != ERR_SUCCESS )
				print_dbg(DBG_NONE, 1, "OP Server Module Initialization Fail ");

	bupdate			= init_update_client( recv_update_data_transfer );
	if ( bupdate != ERR_SUCCESS )
				print_dbg(DBG_NONE, 1, "Update Client Module Initialization Fail ");


	/* function reg */
	/*control -> transefer */
	set_trans_module_recv_func( recv_sns_data_fms_client,
							   recv_sns_data_op_server,
							   recv_cmd_data_update_client,
							   recv_ctrl_sns_data_websock );

	/* control -> sensor */
	set_sns_module_recv_func( recv_ctrl_data_radio,
							 recv_ctrl_data_mcu,
							 recv_ctrl_data_pwr_c,
							 recv_ctrl_data_remote,
							 recv_ctrl_data_temp,
							 recv_ctrl_data_pwr_m,
							 recv_ctrl_data_apc_ups );

	g_system_init = 1;
	set_rtu_initialize_shm( 1 );
	
	create_main_thread();
	create_etc_thread();

	print_dbg( DBG_LINE, 1, NULL  );

	return ERR_SUCCESS;
}

int main( int argc, char *argv[] )
{
	int prc_cnt = 0;

	prc_cnt = iscnt_process( APP_PNAME );

#ifdef USE_WEBSOCK 
	print_dbg( DBG_SAVE,1, "Use of WebSocket");
#else
	print_dbg( DBG_SAVE,1, "Not Use of WebSocket");
#endif
	//printf("proc cnt : ========================================%d.......\r\n", prc_cnt );
	if( prc_cnt > 1 )
	{
		print_dbg( DBG_ERR,1 , "RTU Reboot Because of Two loggers are running(%d).", prc_cnt );
		system_cmd("reboot");
		sys_sleep( 3000 );
	}
	
	check_sys();

	resize_stack();
	sys_sleep( 100 );

	set_keep_alive( );

	make_verify_file();

	init_util();

	print_dbg( DBG_LINE, 1, NULL );
	print_dbg(DBG_NONE, 1,  "Start of RTU Logger" );
	print_dbg(DBG_SAVE, 1,  "Start of RTU Logger" );
	print_dbg( DBG_LINE, 1, NULL );


	main_pre_init();

	init_com_conf( CTRL_SEM_KEY );

	init_shm( SHM_SEM_KEY );
	init_sighandler();

#if 0
	/* set temp time : 2014-04-22 02:05:59 */
	set_rtc_time( 1398132359, 0 );
#endif

	/* main init */
	if (main_init() == ERR_SUCCESS )
	{
		main_loop();
	}
	else
	{
		print_dbg( DBG_LINE, 1, NULL );
		print_dbg(DBG_ERR, 1,  "Shutdown RTU Because of RTU initialize Fail !! " );
		print_dbg( DBG_LINE, 1, NULL );
	}

	main_release();
	release_util();

	print_box( "Exit of RTU Logger" );

	return ERR_SUCCESS;
}

