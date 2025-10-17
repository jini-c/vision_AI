#include <sys/wait.h>
#include <dirent.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "com_include.h"
#include "core_conf.h"
#include "conf.h"
#include "util.h"
#define  CHK_TIME   ( 60 * 30 )

static volatile int wtd_fd = -1;
static volatile int terminated = 0;

/* PID */
static volatile int g_pid_logger= -1;

static pthread_t observer_pthread;
static void * pthread_observer_idle( void * ptrdata );
static volatile int g_control_run_cnt = 0;
static volatile int start_observer_pthread = 0;
static volatile int end_observer_pthread = 1;

static int start_agent( void );
static int check_watchdog_fd( void );
static int reset_flag = 0;

static char g_szfile[ MAX_PATH_SIZE ];
static volatile UINT32 g_time_tick  =0;
static volatile UINT32 g_release_time_tick  =0;

#if 0
default_value_t ifcfg_eth0_tbl[] = 
{ 
	{ KEY_MY_IP     , "192.168.0.169"     },
	{ KEY_MY_NETMASK, "255.255.255.0"     },
	{ KEY_MY_GATEWAY, "192.168.0.1"       },
	{ NULL          , NULL                }
};

static void init_net_file( char *ptrpath, const char *ptrdelim, default_value_t tbl[] )
{
	conf_t *conf = NULL;
	int i;
	char *val;
	int flag = 0;

	if ( ptrpath == NULL )
	{
		print_dbg(DBG_ERR, 1,  "Initialization:Fail to Read Partition Information");
	}

	conf = load_conf( ptrpath , ptrdelim );
	if( conf == NULL )
	{
		print_dbg( DBG_ERR, 1, "Fail to read Network Configuration File");
		return;
	}

	for( i = 0; tbl[i].key != NULL; i++ )
	{
		val = conf_get_value( conf, tbl[i].key );
		if( val == NULL )
		{
			conf_set_value( conf, tbl[i].key, tbl[i].val );
			flag =1;
		}
	}

	if ( conf != NULL )
	{
		if ( flag == 1 )
		{
			print_dbg( DBG_ERR, 1, "Set of Default Network, ( %s )", __FUNCTION__);
			save_release_conf( conf );
		}
		else
		{
			release_conf( conf );
		}
	}
}
#endif

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

static int create_observer_thread( void )
{
	/* msgqueue thread */
	if ( start_observer_pthread == 0 && end_observer_pthread == 1 )
	{
		if ( pthread_create( &observer_pthread, NULL, pthread_observer_idle, NULL ) < 0 )
		{
			//printf("err create network  pop queue thread....\n");
		}
		else
		{
			start_observer_pthread = 1;
		}
	}
	return ERR_SUCCESS;
}

static int set_app_patition( void )
{
	char * ptrpath = NULL ;

	if ( strlen( g_szfile ) == 0 )
	{
		ptrpath = get_app_main_path( );

		if ( ptrpath != NULL )
		{
			sprintf( g_szfile, "%s%s", ptrpath , APP_PNAME );
		}
		else
		{
			print_dbg(DBG_ERR, 1, "Can not find that App");
		}
	}

	return ERR_SUCCESS;
}


static void * pthread_observer_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;

	end_observer_pthread = 0;

	while ( terminated == 0  )
	{
		check_watchdog_fd();
		sys_sleep( 3000 );
	}

	pthread_join( observer_pthread , ( void **)&status );
	start_observer_pthread =0;
	end_observer_pthread = 1;

	return ERR_SUCCESS;
}

static int logger_rollback( void )
{
    char szcmd[ 400 ];

    memset( szcmd, 0, sizeof( szcmd ));
    sprintf( szcmd,"cp -f /home/fms/apps/* /home/fms/appm && chmod +x /home/fms/appm/*");
    print_dbg( DBG_ERR, 1, "############# Rollback Process !! fms_logger Application Copy apps to appm");
	system_cmd( szcmd );

    sys_sleep( 3000 );
    
    killall( APP_PNAME, 9 );
    killall( APP_PNAME, 9 );

    return ERR_SUCCESS;
}

static int check_backup( void )
{
    UINT32 cur_tick = 0;
    UINT32 inter_tick = 0;
    char szcmd[ 400 ];
    volatile int updated = 0;

    cur_tick = get_sectick();

    if (g_time_tick == 0 )
    {
        g_time_tick = cur_tick;
        return ERR_SUCCESS;
    }

    inter_tick = cur_tick - g_time_tick;

    if( ( g_control_run_cnt < 5) &&
         (inter_tick >= CHK_TIME  ))
    {
        updated = get_update_conf();

        if (updated != 0 &&  (updated + CHK_TIME ) <= cur_tick )
        {
            memset( szcmd, 0, sizeof( szcmd ));
            sprintf( szcmd,"cp -f /home/fms/appm/*  /home/fms/apps/" );
            system_cmd( szcmd );
            print_dbg( DBG_ERR, 1, "Backup Process !! fms_logger Application Copy appm to apps");
            g_control_run_cnt   = 0;

            sys_sleep( 1000 );

            /* Update 상태 초기화 */
            set_update_conf( 0 );
        }

        g_time_tick = cur_tick;
    }
     
   return ERR_SUCCESS;
}

static int check_release( void )
{
    UINT32 cur_tick = 0;
    UINT32 inter_tick = 0;

    cur_tick = get_sectick();

    if (g_release_time_tick == 0 )
    {
        g_release_time_tick = cur_tick;
        return ERR_SUCCESS;
    }

    inter_tick = cur_tick - g_release_time_tick;

    if (( g_control_run_cnt < 5) &&
         (inter_tick >=CHK_TIME ))
    {
        g_control_run_cnt   = 0;
        g_release_time_tick = cur_tick;
    }

    return ERR_SUCCESS;
}

static void sig_term(int sig)
{
	printf("SIGTERM");
	terminated = 1;
}

static void sig_hup(int sig)
{
	//printf("SIGHUP");
}

static void sig_usr1(int sig)
{
	//printf("SIGUSR1");
}

static void sig_init(int sig)
{
	print_dbg(DBG_INFO, 1, "SIGINIT");
	terminated = 1;
}

static void sig_child(int sig)
{
	int pid;

	if( sig == SIGCHLD )
	{
		pid = waitpid(-1, NULL, WNOHANG);

		if( pid == g_pid_logger )
		{
			g_pid_logger = -1;
			print_dbg(DBG_ERR, 1, "observe::killed Logger ");

			g_control_run_cnt++;

			if ( g_control_run_cnt >= 5 )
			{
				print_dbg(DBG_ERR, 1,  "############# Becase Re-run 5 more times.Logger are expected to be Rollback And Reboot");
				sys_sleep( 100 );
                g_control_run_cnt   = 0;
                logger_rollback();
			}

			print_dbg(DBG_ERR, 1, "observe::Restart Logger Cnt:%d", g_control_run_cnt );
			if ( reset_flag == 0 )
			{
				sys_sleep( 2000 );
				start_agent();
			}

		}
	}
	else if( sig == SIGINT )
	{
		print_dbg(DBG_INFO, 1, "SIGINIT");
		terminated = 1;
	}
}
static void setup_sighandler(void)
{
	struct sigaction sa;


	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_NOCLDSTOP;

	sigemptyset( &sa.sa_mask );

	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);

	sigaction(SIGINT,  &sa, NULL);
	sa.sa_handler = sig_hup;
	sigaction(SIGHUP, &sa, NULL);

	sa.sa_handler = sig_usr1;
	sigaction(SIGUSR1, &sa, NULL);

	sa.sa_handler = sig_child;
	sigaction(SIGCHLD, &sa, NULL);

	sa.sa_handler = sig_init;
	sigaction(SIGINT, &sa, NULL);
}

static int daemonize( void )
{
	pid_t pid;

	pid = fork();
	if( pid > 0 )
	{
		exit( 0 );
	}
	print_dbg(DBG_INFO, 1, "Daemonization OK");

	return ERR_SUCCESS;
}

static int create_process( char * path, int target_pid )
{
	pid_t pid;
	int argc;
	char *argv[ 20 ];

	//printf("target_pid = %d..\n", target_pid );
	if( target_pid < 0 )
	{
		pid = fork();
		if( pid == 0 )
		{
			argc = 0;
			argv[ argc++ ] = path;
			argv[ argc ] = NULL;
			execv( argv[ 0 ], argv );
			exit( -1 );
		}
		if( pid > 0 )
		{
			return pid;
		}
	}
	else if( target_pid > 0 )
	{
		return target_pid ;
	}

	return -1;
}

static int start_agent( void )
{
	int val;

	set_app_patition();
	val = create_process( g_szfile, g_pid_logger );
	g_pid_logger= val;

	return ERR_SUCCESS;
}

static int check_watchdog_fd( void )
{
	int fd = -1;

#if 0
	int bwatchdog = 0;
	if ( bwatchdog == 1 )
	{
		if( wtd_fd <= 0 )
		{
			wtd_fd = open( DEV_WTD, O_RDWR );
		}

		if ( wtd_fd > 0 )
		{
			ioctl(wtd_fd , WDIOC_KEEPALIVE, NULL);
		}
	}
	else
	{
		if ( wtd_fd > 0 )
		{
			write( wtd_fd , "V", 1);
			close( wtd_fd );
			wtd_fd = -1;
		}
	}
#endif

	//printf("watchdog :%d, wtd_fd %d...\n", bwatchdog, wtd_fd );
	return fd;
}

static int main_loop( void )
{

	while( terminated == 0 ) 
	{   
		start_agent();
		sys_sleep( 20000 );

        check_backup();
        check_release();
	}   

	return ERR_SUCCESS;
}
int main( int argc, char *argv[] )
{
	print_dbg( DBG_LINE, 1, NULL );
	print_dbg( DBG_NONE , 1, "Start of RTU Observer" );
	print_dbg( DBG_LINE, 1, NULL );

	/* DB Connect 후 IP 변경 */

	killall( APP_PNAME, 9 );

	/* verify file 생성 */
	make_verify_file();
	memset( g_szfile , 0, sizeof( g_szfile ));

	/* 설정 파일 초기화   */
	init_com_conf( 0 );

	/* seg message 등록 */
	setup_sighandler();
	sys_sleep( 2000 );

	/* Daemon 생성 */
	daemonize();

	/* 설정 파일 초기화  (데몬화 후 한번더 ) */
	init_com_conf( 0 );

	/* db에서 IP 관리 */

	/* Observer wathchdog thread 생성 */
	create_observer_thread();

	main_loop( );

	return ERR_SUCCESS;
}
