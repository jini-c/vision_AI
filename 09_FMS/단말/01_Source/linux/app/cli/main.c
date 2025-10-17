
#include <sys/wait.h>
#include <dirent.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "com_include.h"
#include "shm.h"

#include "core_conf.h"
#include "conf.h"
#include "util.h"

#include <my_global.h>
#include <mysql.h>

typedef struct menu_
{
	int ret;
	int pos;
	char *str_index;
	char *str_title;
	char *str_input;
	int (*get_func)(void * );
	int (*set_func)(char * );
}__attribute__ ((packed)) menu_t;

/* PID */
static pthread_t debug_pthread;
static void * pthread_debug_idle( void * ptrdata );
static volatile int g_control_run_cnt = 0;
static volatile int start_debug_pthread = 0;
static volatile int end_debug_pthread = 1;

static int set_debug_fd( void );
static volatile int g_debug_fd = -1;
static char * g_recv_buf= NULL;
static struct sockaddr_in g_client;

static volatile int g_done 	= 0;
static volatile int g_menu_list_length =0;
static volatile int g_debug_prt	= 0;

static int exit_nonsave(char *ptr);

static int get_rtu_network( void * ptr );
static int set_rtu_network( char * ptr );

static int get_rtu_siteid( void * ptr );
static int set_rtu_siteid( char * ptr );

static int set_rtu_debug( char * ptr );
static int set_rtu_arp( char * ptr );

static int set_rtu_do( char * ptr );
static int set_rtu_light( char * ptr );
static int set_rtu_doe( char * ptr );
static int set_rtu_pwr( char * ptr );
static int set_rtu_air( char * ptr );
static int set_rtu_di_read( char * ptr );
static int set_rtu_version( char * ptr );
static int set_rtu_restart( char * ptr );
static int set_rtu_shutdown( char * ptr );
static int set_rtu_reserved( char * ptr );
static int set_rtu_webport( char * ptr );

static int func_NULL(void * ptr);

#define POS_NET 	0
#define POS_SITE 	1
#define POS_DEBUG   2
#define POS_ARP		3
#define POS_DO		4
#define POS_LIGHT	5
#define POS_DOE		6
#define POS_PWR		7
#define POS_AIR		8
#define POS_DI		9
#define POS_VER     10	
#define POS_KILL    11	
#define POS_SDOWN   12	
#define POS_RESERVED   13	

static menu_t g_menus[] = {
	{ 0, POS_SITE, 	"1", "Set RTU Site ID( Max Numeric 4 Digits)   "					, NULL	, get_rtu_siteid	, set_rtu_siteid},
	{ 0, POS_NET, 	"2", "Set RTU Test Network(Ip,Netmask,Gateway) "					, NULL	, get_rtu_network	, set_rtu_network},
	{ 0, POS_ARP, 	"3", "Set RTU ARPing (Traget IP,Repeat Count ) "    			    , NULL	, func_NULL			, set_rtu_arp	},
	{ 0, POS_DEBUG, "4", "Read RTU All Debug (t: Test Mode, r: Real Mode, e: Stop Debug )", NULL, func_NULL			, set_rtu_debug	},
	{ 0, POS_DO,    "5", "Test DO ( 8 Digits[ 0 or 1 ] Ex:0,1,1,1,1,1,1,1,1 )"		    , NULL	, func_NULL			, set_rtu_do	},
	{ 0, POS_LIGHT, "6", "Test Light ( 1 Digits[ 0 or 1 ] Ex:0 )" 		                , NULL	, func_NULL			, set_rtu_light	},
	{ 0, POS_DOE,   "7", "Test Inside Power ( 2 Digits[ 0 or 1 ] Ex:0,1	)" 		        , NULL	, func_NULL			, set_rtu_doe	},
	{ 0, POS_PWR,   "8", "Test Outside Power ( 6 Digits[ 0 or 1 ] Ex:0,1,1,1,1,1 )"  	, NULL	, func_NULL			, set_rtu_pwr	},
	{ 0, POS_AIR,   "9", "Test Aircon ( 1 Digits[ 0 or 1 ] Ex:1 )"		                , NULL	, func_NULL			, set_rtu_air	},
	{ 0, POS_DI,    "a", "Read DO, DI,AI Data (s: Start, e: Stop) "						, NULL	, func_NULL			, set_rtu_di_read	},
	{ 0, POS_VER,   "b", "Read Version & Protocol Network(s :Display, e: Not Display) "  , NULL	, func_NULL			, set_rtu_version	},
	{ 0, POS_KILL,  "c", "RTU Restart ( r : RTU Restart )"                              , NULL	, func_NULL			, set_rtu_restart	},
	{ 0, POS_SDOWN, "d", "RTU Shutdown ( s : RTU Shutdown )"                            , NULL	, func_NULL			, set_rtu_shutdown	},
	{ 0, POS_SDOWN, "e", "Reserved Command( Numeric 2 Digits : First cmd, Second cmd)"  , NULL	, func_NULL			, set_rtu_reserved	},
	{ 0, POS_SDOWN, "f", "Set RTU WebSocket Port( Numeric Data 1 ~ 60000 )"  ,           NULL	, func_NULL			, set_rtu_webport	},
	{ 0, 101, 		"x", "Exit "														, NULL	, NULL				, exit_nonsave	},
};

/* ================================== DEBUG FUNTION =================================================*/
static db_info_t g_db_info;

static int close_db( MYSQL * ptrconn )
{
	if ( ptrconn != NULL )
	{
		mysql_close( ptrconn );
		ptrconn = NULL;
	}

	return ERR_SUCCESS;
}

static int db_connect ( MYSQL * ptrconn )
{
	int ret = ERR_SUCCESS;
    
    if ( strlen( g_db_info.szdbuser ) == 0 ||
         strlen( g_db_info.szdbpwr) == 0 ||
         strlen( g_db_info.szdbname ) == 0  )
    {
        print_dbg( DBG_ERR, 1, "Invalid DB Info User:%s, Pwr:%s, Name:%s",
                                g_db_info.szdbuser, g_db_info.szdbpwr, g_db_info.szdbname );
        return ERR_RETURN;
    }
#if 0
    print_dbg( DBG_INFO, 1, "DB Info User:%s, Pwr:%s, Name:%s",
            g_db_info.szdbuser, g_db_info.szdbpwr, g_db_info.szdbname );
#endif

	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "is null ptrconn [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	if (mysql_real_connect( ptrconn, "127.0.0.1", g_db_info.szdbuser, g_db_info.szdbpwr, g_db_info.szdbname, 3306, NULL, 0) == NULL)
	//if (mysql_real_connect( ptrconn, "127.0.0.1", "fmsuser", "fms123", "FMSDB", 3306, NULL, 0) == NULL)
	{
		print_dbg( DBG_ERR,1, "Cannot Connect LocalDB [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	return ret ;
}

int update_rtu_site_id( int siteid )
{
	int ret = ERR_SUCCESS;
	MYSQL * ptrconn = NULL;
	char szsql[ 500 ]; 
	int org_siteid = 0;

	if ( siteid == 0 )
	{
		print_dbg( DBG_ERR,1, "siteid is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	if (get_rtu_run_mode_shm() != RTU_RUN_ALONE )
	{
		print_dbg( DBG_SAVE,1, "is not RTU RUN ALONE MODE [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}

	org_siteid= get_my_siteid_shm();

	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* 해당 부분 변경 시 반드시 CTRL DB 도 수정할 것 */
	/* TFMS_AITHRLD_DTL */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_AITHRLD_DTL set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_INFO,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* TFMS_DIDOTHRLD_DTL */
	sys_sleep( 100 );
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_DIDOTHRLD_DTL set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );
	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_SAVE,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* TFMS_FCLT_MST */
	sys_sleep( 100 );
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_FCLT_MST set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );
	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_SAVE,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* TFMS_SITE_MST */
	sys_sleep( 100 );
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_SITE_MST set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );
	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_SAVE,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* TFMS_SNSRMETHOD_MST */
	sys_sleep( 100 );
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_SNSRMETHOD_MST set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );
	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_SAVE,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* TFMS_SNSR_MST */
	sys_sleep( 100 );
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_SNSR_MST set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );
	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_SAVE,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* TFMS_USER_MST */
	sys_sleep( 100 );
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_USER_MST set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );
	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_SAVE,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	mysql_close( ptrconn );

	return ret ;

}

static int run_arp( char *ptrip  )
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
	int send_cnt	= 4;
	char * ptrifname = NULL;

	if ( ptrip == NULL )
		return -1;

	memset( output, '\0', sizeof( output ) );
	memset( cmdstr, '\0', sizeof( cmdstr ) );

	ptrifname = get_ifname_shm();
	sprintf( cmdstr, "sudo arping -c %d %s -i %s | grep 'received' ", send_cnt, ptrip , ptrifname );

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
						if( recv_cnt >= 2 )
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
		print_dbg(DBG_NONE,1, "######## ARP Status:: Alived of Target Device(ARP Recv Cnt:%d, Sts:%d, Target IP:%s)", recv_cnt , net_sts, ptrip );
	}
	else
	{
		print_dbg(DBG_NONE,1,"######## ARP Status:: Not Alive of Target Device(ARP Recv Cnt:%d, Sts:%d, Target IP:%s)", recv_cnt , net_sts, ptrip );
	}

	return net_sts;
}

static int create_debug_thread( void )
{
	/* msgqueue thread */
	if ( start_debug_pthread == 0 && end_debug_pthread == 1 )
	{
		if ( pthread_create( &debug_pthread, NULL, pthread_debug_idle, NULL ) < 0 )
		{
			//printf("err create network  pop queue thread....\n");
		}
		else
		{
			start_debug_pthread = 1;
		}
	}
	return ERR_SUCCESS;
}

static int set_debug_fd( void )
{
	int fd =-1; 
	struct sockaddr_in sockaddr;
	int so_reuseaddr;
	int err=ERR_SUCCESS;

	if ( g_debug_fd  > 0 )
		return 0;

	fd = socket( PF_INET, SOCK_DGRAM, 0 ); 
	if ( fd < ERR_SUCCESS )
	{    
		print_dbg(DBG_ERR, 1, "Fail to Create Debug socket");
		return ERR_RETURN;
	}    

	so_reuseaddr = 1; 
	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof (so_reuseaddr));

	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = inet_addr( DEBUG_IP );
	//sockaddr.sin_addr.s_addr = htonl( INADDR_ANY );
	sockaddr.sin_port = htons( DEBUG_PORT );

	err = bind( fd, (struct sockaddr *) &sockaddr, sizeof( sockaddr ));
	if ( err != ERR_SUCCESS )
	{    
		close( fd );
		print_dbg(DBG_ERR, 1, "Fail to create debuger bind port( %d )", DEBUG_PORT );
		return ERR_RETURN ;
	}    

	g_debug_fd = fd;

	return ERR_SUCCESS;
}

static int send_test_mode( char * ptr, int size )
{

	if ( g_debug_fd > 0  )
	{
		sendto( g_debug_fd,  ptr, size, 0, ( struct sockaddr * )&g_client, sizeof( g_client ) );	
	}

	return ERR_SUCCESS;
}

static int read_debug( void )
{
	int n;
	struct sockaddr_in clientaddr;
	socklen_t len = 0;
	
	if ( g_debug_fd < 0 )
		return ERR_RETURN;

	if ( g_recv_buf == NULL )
	{
		print_dbg( DBG_ERR,1, "Can not find recvbuf");
		sys_sleep( 2000 );
		return ERR_SUCCESS;
	}

	memset(&clientaddr, 0, sizeof( clientaddr ));
	len = sizeof( clientaddr );

	n = recvfrom( g_debug_fd, g_recv_buf, 1000, 0, ( struct sockaddr *)&clientaddr, &len );

#if 0
	char * ip = NULL;
	int port = 0;

	ip  = inet_ntoa( clientaddr.sin_addr );
	port = ntohs( clientaddr.sin_port );

	printf("ip:%s, port:%d..size:%d..\r\n",ip,port , n );
#endif
	
	if ( n > 0 )
	{
		g_recv_buf[ n ] = 0x00;

		if ( g_debug_prt == 1 )
		{
			printf("%s", g_recv_buf );
		}
		memcpy( &g_client, &clientaddr, len );
	}

	return ERR_SUCCESS;
}

static void * pthread_debug_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;

	end_debug_pthread = 0;

	while ( g_done == 0  )
	{
		read_debug( );
	}

	pthread_join( debug_pthread , ( void **)&status );
	start_debug_pthread =0;
	end_debug_pthread = 1;

	return ERR_SUCCESS;
}

static int create_buf( void )
{
	if (g_recv_buf == NULL )
	{
		g_recv_buf = ( char *)malloc( sizeof( BYTE ) * MAX_DBG_BUF_SIZE );
	}

	return ERR_SUCCESS;
}

/* ================================== CLI FUNTION =================================================*/
static int func_NULL(void * ptr )
{
	return 0;
}

static int exit_nonsave(char *ptr)
{
	g_done = 1;
	return 0;
}

static int get_rtu_network( void * ptr )
{
	char * ptrip = NULL;
	char * ptrnet = NULL;
	char * ptrgate = NULL;
	char * ptrifname = NULL;

	net_info_t *ptrinfo = NULL;


	if ( ptr == NULL )
		return -1;
	
	ptrinfo = ( net_info_t *)ptr;
	ptrifname = get_ifname_shm();

	if ( ptrifname != NULL )
	{
		ptrip  = get_ipaddr( ptrifname );
		ptrnet = get_netmask( ptrifname );
		ptrgate= get_gateway( ptrifname );
	}

	if ( ptrip != NULL && ptrnet != NULL && ptrgate != NULL )
	{
		memcpy( ptrinfo->ip, ptrip, strlen( ptrip ));
		memcpy( ptrinfo->netmask, ptrnet, strlen( ptrnet ));
		memcpy( ptrinfo->gateway, ptrgate, strlen( ptrgate ));
	}

	return 0;
}

static int set_rtu_network( char * ptr )
{
	char *tr = NULL;
	int find = 0;
	int pos =0;
	
	char szip[ MAX_IP_SIZE ];
	char sznet[ MAX_IP_SIZE ];
	char szgate[ MAX_IP_SIZE ];
	char * ptrifname = NULL;
	net_info_t net;

	if ( ptr == NULL)
		return -1;
	
	trim( ptr );
	ptrifname = get_ifname_shm();
	
	if ( ptrifname == NULL )
		return -1;

	memset(szip,0, sizeof( szip ));
	memset(sznet,0, sizeof( sznet ));
	memset(szgate,0, sizeof( szgate ));
	memset( &net, 0, sizeof( net ));

	tr = strtok(ptr, "," );
	while (tr != NULL && find==0)
	{
		//printf("token:pos:%d...%s..\r\n",pos, tr);
		if( pos == 0 )
		{
			memcpy( szip, tr, strlen( tr ));
		}
		else if ( pos ==1  )
		{
			memcpy( sznet, tr, strlen( tr ));
		}
		else if ( pos ==2 )
		{
			memcpy( szgate, tr, strlen( tr ));
		}
		tr = strtok(NULL, ",");
		pos++;
	}
	
	if ( strlen( szip ) > 0 && strlen( sznet ) > 0 && strlen( szgate ) > 0 )
	{

		memcpy( net.ip 		, szip, strlen( szip ));
		memcpy( net.gateway , szgate, strlen( szgate ));
		memcpy( net.netmask , sznet, strlen( sznet ));

		if( save_rtu_net_shm( &net  ) == ERR_SUCCESS )
		{
			sys_sleep( 1000 );
			system_cmd("reboot");
			//change_netmask( ptrifname, szip, sznet );
			//change_gateway( szgate );
		}
		else
		{
			print_dbg(DBG_ERR, 1, "Can not Change RTU IP ADDR:IP:%s, GATE:%s, NET:%s", net.ip, net.gateway, net.netmask);
			return -1;

		}

	}
	else
	{
		printf("input All Infomation:: IP,Netmaks,Gateway");
		return -1;
	}
	return 0;
}

static int get_rtu_siteid( void * ptr )
{
	int siteid ;

	if ( ptr == NULL )
		return -1;

	siteid = get_my_siteid_shm();
	sprintf(ptr,"%04d", siteid );

	return 0;
}

static int set_rtu_siteid( char * ptr )
{
	int ret = 0;
    char sztemp[ 2 ];
	int siteid = 0;

    memset( sztemp, 0, sizeof( sztemp ));
    sprintf(sztemp,"%s","r");

	if ( ptr == NULL )
		return -1;

	if ( isNumStr( ptr ) != 1 )
	{
		printf("Is not Numeric :%s", ptr );
		return -1;
	}

	if ( strlen( ptr ) > 4 )
	{
		printf("Over langth Data size :%d\r\n", (int)strlen(ptr) );
		return -1;
	}
	
	siteid = atoi( ptr );

	if ( update_rtu_site_id( siteid ) == ERR_SUCCESS )
	{
		ret = set_my_siteid_shm( siteid );
		if ( ret == 0 )
		{
			//killall( APP_PNAME, 9);
			set_rtu_restart(sztemp);
		}
	}
	return ret;
}

static int set_rtu_debug( char * ptr )
{
	if ( ptr == NULL )
		return -1;

	toupper_str( ptr );

	if ( strcmp( ptr, "E") == 0 )
	{
		g_debug_prt = 0;
	}
	else if ( strcmp( ptr, "T") == 0  || strcmp( ptr, "R") == 0 )
	{
		send_test_mode( ptr, 1 );
		g_debug_prt = 1;
	}

	return 0;
}

static int set_rtu_arp( char * ptr )
{
	char *tr = NULL;
	int find = 0;
	int pos =0;
	
	char szip[ MAX_IP_SIZE ];
	char szcnt[ MAX_IP_SIZE ];
	int i, cnt = 0;

	if ( ptr == NULL)
		return -1;
	
	trim( ptr );
	
	memset(szip,0, sizeof( szip ));
	memset(szcnt,0, sizeof( szcnt ));

	tr = strtok(ptr, "," );
	while (tr != NULL && find==0)
	{
		//printf("token:pos:%d...%s..\r\n",pos, tr);
		if( pos == 0 )
		{
			memcpy( szip, tr, strlen( tr ));
		}
		else if ( pos ==1  )
		{
			memcpy( szcnt, tr, strlen( tr ));
		}
		tr = strtok(NULL, ",");
		pos++;
	}
	
	if ( isNumStr( szcnt ) == 0 )
	{
		printf("input the number of repet count\r\n" );
		return -1;
	}

	if ( strlen( szip ) > 0 && strlen( szcnt ) > 0 )
	{
		cnt = atoi( szcnt );

		printf("Start of Arping test, Please Wait....\r\n");
		for( i = 0 ; i < cnt ; i++ )
		{
			printf("Send Arping packet repeat Count:%d\r\n", i+1 );
			run_arp( szip );
		}
	}
	else
	{
		printf("input All Infomation:: Target IP,Repeat Count\r\n");
		return -1;
	}

	return 0;
}
static int set_data( int pos , char *ptrdata , BYTE * ptrbuf )
{
	if( isNumStr( ptrdata ) == 0)
	{
		printf("is not numeric data:%s", ptrdata);
		return -1;
	}

	ptrbuf[ pos + 1 ] = atoi( ptrdata );
	return 0;
}

static int set_rtu_do( char * ptr )
{
	BYTE data[ 10 ];
	int pos = 0;
	int cnt = 8;
	char * tr = NULL;
	int find = 0;

	if ( ptr == NULL)
		return -1;
	
	trim( ptr );
	
	memset(data,0, sizeof( data ));

	data[ 0 ] = DO_TYPE;

	tr = strtok(ptr, "," );

	while (tr != NULL && find==0)
	{
		//printf("token:pos:%d...%s..\r\n",pos, tr);
		if( pos == 0 || pos == 1 ||pos == 2 ||pos == 3 ||pos == 4 ||pos == 5 ||pos == 6 ||pos == 7 )
		{
			if (set_data( pos, tr, data ) != 0)
				return -1;
		}

		tr = strtok(NULL, ",");
		pos++;
	}
	
	if ( pos != cnt )
	{
		printf("input 8 digits with 0 or 1(ex:1,0,1,1,1,1,1,1) \r\n" );
		return -1;
	}

	send_test_mode( (char*)&data, cnt+1 );

	return 0;
}

static int set_rtu_light( char * ptr )
{
	BYTE data[ 10 ];
	int pos = 0;
	int cnt = 1;
	char * tr = NULL;
	int find = 0;


	if ( ptr == NULL)
		return -1;
	
	trim( ptr );
	
	memset(data,0, sizeof( data ));

	data[ 0 ] = LIGHT_TYPE;

	tr = strtok(ptr, "," );

	while (tr != NULL && find==0)
	{
		//printf("token:pos:%d...%s..\r\n",pos, tr);
		if( pos == 0 )
		{
			if (set_data( pos, tr, data ) != 0)
				return -1;
		}
		tr = strtok(NULL, ",");
		pos++;
	}
	
	if ( pos != cnt )
	{
		printf("input 1 digits with 0 or 1 \r\n" );
		return -1;
	}

	send_test_mode( (char*)&data, cnt+1 );
	return 0;
}

static int set_rtu_doe( char * ptr )
{
	BYTE data[ 10 ];
	int pos = 0;
	int cnt = 2;
	char * tr = NULL;
	int find = 0;


	if ( ptr == NULL)
		return -1;
	
	trim( ptr );
	
	memset(data,0, sizeof( data ));

	data[ 0 ] = DOE_TYPE;

	tr = strtok(ptr, "," );

	while (tr != NULL && find==0)
	{
		//printf("token:pos:%d...%s..\r\n",pos, tr);
		if( pos == 0 || pos == 1 )
		{
			if (set_data( pos, tr, data ) != 0)
				return -1;
		}
		tr = strtok(NULL, ",");
		pos++;
	}
	
	if ( pos != cnt )
	{
		printf("input 2 digits with 0 or 1(ex:1,0) \r\n" );
		return -1;
	}

	send_test_mode( (char*)&data, cnt+1 );
	return 0;
}

static int set_rtu_pwr( char * ptr )
{
	BYTE data[ 10 ];
	int pos = 0;
	int cnt = 6;
	char * tr = NULL;
	int find = 0;


	if ( ptr == NULL)
		return -1;
	
	trim( ptr );
	
	memset(data,0, sizeof( data ));

	data[ 0 ] = PWR_TYPE;

	tr = strtok(ptr, "," );

	while (tr != NULL && find==0)
	{
		//printf("token:pos:%d...%s..\r\n",pos, tr);
		if( pos == 0 || pos == 1 || pos == 2 || pos == 3 || pos == 4 || pos == 5 )
		{
			if (set_data( pos, tr, data ) != 0)
				return -1;
		}
		tr = strtok(NULL, ",");
		pos++;
	}
	
	if ( pos != cnt )
	{
		printf("input 6 digits with 0 or 1(ex:1,0,1,1,1,1) \r\n" );
		return -1;
	}

	send_test_mode( (char*)&data, cnt+1 );
	return 0;
}
static int set_rtu_air( char * ptr )
{
	BYTE data[ 10 ];
	int pos = 0;
	int cnt = 1;
	char * tr = NULL;
	int find = 0;

	if ( ptr == NULL)
		return -1;
	
	trim( ptr );
	
	memset(data,0, sizeof( data ));

	data[ 0 ] = AIR_TYPE;

	tr = strtok(ptr, "," );

	while (tr != NULL && find==0)
	{
		//printf("token:pos:%d...%s..\r\n",pos, tr);
		if( pos == 0 )
		{
			if (set_data( pos, tr, data ) != 0)
				return -1;
		}
		tr = strtok(NULL, ",");
		pos++;
	}
	
	if ( pos != cnt )
	{
		printf("input 1 digits with 1 \r\n" );
		return -1;
	}
	send_test_mode( (char*)&data, cnt+1 );
	return 0;
}

static int set_rtu_di_read( char * ptr )
{
	BYTE data[ 10 ];
	int cnt = 1;

	if ( ptr == NULL)
		return -1;
	
	trim( ptr );
	
	memset(data,0, sizeof( data ));

	data[ 0 ] = DI_TYPE;
	toupper_str( ptr );

	if ( strcmp( ptr, "S") == 0 )
	{
		g_debug_prt = 1;
	}
	else if ( strcmp( ptr, "E") == 0 )
	{
		g_debug_prt = 0;
	}

	data[ 1 ] = ptr[ 0 ];
	
	send_test_mode( (char*)&data, cnt+1 );
	return 0;
}

static int set_rtu_version( char * ptr )
{
	BYTE data[ 10 ];
	int cnt = 1;

	if ( ptr == NULL)
		return -1;
	
	trim( ptr );
	
	memset(data,0, sizeof( data ));

	data[ 0 ] = VER_TYPE;
	toupper_str( ptr );

	if ( strcmp( ptr, "S") == 0 )
	{
		g_debug_prt = 1;
        //printf("version.........start\r\n");
	}
	else if ( strcmp( ptr, "E") == 0 )
	{
		g_debug_prt = 0;
	}

	data[ 1 ] = ptr[ 0 ];
	
	send_test_mode( (char*)&data, cnt+1 );
    sys_sleep( 2000 );
	g_debug_prt = 0;
	//printf("version.........end\r\n");

	return 0;
}

static int set_rtu_restart( char * ptr )
{
	BYTE data[ 10 ];
	int cnt = 1;

	if ( ptr == NULL)
		return -1;
	
	trim( ptr );
	
	memset(data,0, sizeof( data ));

	data[ 0 ] = REBOOT_TYPE;
	toupper_str( ptr );

	if ( strcmp( ptr, "R") == 0 )
	{
        data[ 1 ] = ptr[ 0 ];
        send_test_mode( (char*)&data, cnt+1 );
	}

	return 0;

}

static int set_rtu_shutdown( char * ptr )
{
	if ( ptr == NULL)
		return -1;
	
	trim( ptr );
	toupper_str( ptr );

	if ( strcmp( ptr, "S") == 0 )
	{
        print_dbg(DBG_NONE, 1, "Shutdown of RTU Logger");
        system_cmd("reboot");
	}

	return 0;

}

static int set_rtu_webport( char * ptr )
{
	int ret = 0;
    char sztemp[ 2 ];
	int port = 0;

    memset( sztemp, 0, sizeof( sztemp ));
    sprintf(sztemp,"%s","r");

	if ( ptr == NULL )
		return -1;

	if ( isNumStr( ptr ) != 1 )
	{
		printf("Is not Numeric :%s", ptr );
		return -1;
	}

	port = atoi( ptr );
	
	if ( port < 1 || port > 60000 )
	{
		printf("Over Port Range( 1 ~ 60000 )  :%d\r\n", port );
		return -1;
	}

	ret = set_web_port_shm( port );
	if ( ret == 0 )
	{
        print_dbg(DBG_NONE, 1, "Shutdown of RTU Logger");
        system_cmd("reboot");
	}

    return 0;
}

static int set_rtu_reserved( char * ptr )
{
	BYTE data[ 10 ];
	int pos = 0;
	int cnt = 2;
	char * tr = NULL;
	int find = 0;


	if ( ptr == NULL)
		return -1;
	
	trim( ptr );
	
	memset(data,0, sizeof( data ));

	data[ 0 ] = RESERVED_TYPE;

	tr = strtok(ptr, "," );

	while (tr != NULL && find==0)
	{
		//printf("token:pos:%d...%s..\r\n",pos, tr);
		if( pos == 0 || pos == 1 )
		{
			if (set_data( pos, tr, data ) != 0)
				return -1;
		}
		tr = strtok(NULL, ",");
		pos++;
	}
	
	if ( pos != cnt )
	{
		printf("input 2 command (ex : a,b ) \r\n" );
		return -1;
	}

	send_test_mode( (char*)&data, cnt+1 );
    return 0;
}

static int stdin_string(char *ptr)
{
	char input_message[32];
	char stdinput = 0;
	int i = 0;

	memset(input_message, 0, sizeof(input_message));
	while(1)
	{
		if(stdinput == 10 || stdinput == 'r' || stdinput == 'R' || stdinput == 's' || stdinput == 'S')
		{
			if(stdinput != 10 && stdinput < 96)
			{
				input_message[i-1] = stdinput + 32; 
			}

			break;
		}
		if(i > sizeof(input_message))
		{ break; }

		stdinput = getchar();
		fflush(stdin);

		if((stdinput >= 0x30 && stdinput <= 0x39) || stdinput == 0x10 || stdinput == 46 || stdinput == 44 || stdinput == 'e' || stdinput == 'a' || 
			stdinput == 'b' || stdinput == 's' || stdinput == 'c'  || stdinput == 'd'  || stdinput == 'E' || stdinput == 'S' || stdinput == 'C' ||
			stdinput == 'f' || 
			stdinput == 0x58 || stdinput == 0x78 ) // 0 ~ 9, enter, dot,쉼표
		{
			input_message[i] = stdinput;
			i++;
		}
		else if(stdinput == 0x08) // back space
		{
			input_message[i] = 0;
			if(i > 0)
			{
				i--;
			}
		}
		else
		{

		}
	}

	memcpy(ptr, input_message, sizeof(input_message));
	return 0;
}

static int print_text_menu(void)
{
	char ibuf[MAX_IP_SIZE ];
	char obuf[MAX_IP_SIZE * 5 ];
	char tbuf[MAX_IP_SIZE * 10 ];
	char sbuf[MAX_IP_SIZE * 4 ];
	int i = 0; 
	net_info_t net_info;
	char szsiteid[ 10 ];
	int ret =0;

	memset( ibuf, 0, sizeof( ibuf ));
	memset( obuf, 0, sizeof( obuf ));
	memset( tbuf, 0, sizeof( tbuf ));
	memset( sbuf, 0, sizeof( sbuf ));
	
	memset( szsiteid, 0, sizeof( szsiteid ));
	memset( &net_info, 0, sizeof( net_info));

	print_box("FMS RTU Configuration, Please Input The Number And Press Enter !!");
	for(i = 0; i<g_menu_list_length; i++)
	{
		memset(obuf, 0, sizeof(obuf));
		memset(tbuf, 0, sizeof(tbuf));
	
		if ( g_menus[i].pos  == POS_NET )
		{
			g_menus[i].ret =  g_menus[i].get_func( &net_info );
		}
		else if (g_menus[i].pos  == POS_SITE  )
		{
			g_menus[i].ret =  g_menus[i].get_func( szsiteid );
		}

		if(g_menus[i].ret != 0)
		{
			sprintf(tbuf, "(%-15s)", "Can not Find Information");
			g_menus[i].str_input = tbuf;
		}
		else
		{
			if ( g_menus[i].pos  == POS_NET )
			{
				sprintf(tbuf, "IP:( %s ) Netmask:( %s ) Gateway:( %s ) ", net_info.ip, net_info.netmask, net_info.gateway);
				g_menus[i].str_input = tbuf;
			}
			else if ( g_menus[i].pos == POS_SITE )
			{
				sprintf(tbuf, "SiteID( %-5s ) ", szsiteid );
				g_menus[i].str_input = tbuf;
			}
		}

		sprintf(obuf, "%s. %-35s %-15s", g_menus[i].str_index, g_menus[i].str_title,  tbuf);
		printf("%s\n", obuf);
	}
	printf("\n========================================================================\n");
	printf("select number >> ");

	// 매뉴를 선택한다.
	memset(sbuf, 0, sizeof(sbuf));
	memset(ibuf, 0, sizeof(ibuf));
	stdin_string(ibuf);
	trim(ibuf);

	for(i = 0; i<g_menu_list_length; i++)
	{
		if(strcmp(g_menus[i].str_index, ibuf) == 0)
		{
			printf("selected the menu is [%d. %s], input the value [press 'Enter' - Reload Menu]\n", i+1, g_menus[i].str_title);
			printf(">> ");

			if(i < g_menu_list_length - 1)
			{
				fgets(sbuf, sizeof(sbuf), stdin) ;

				// 메뉴에 해당하는 값을 넣는다
				//stdin_string(sbuf);
				trim(sbuf);
				//printf("[%s]\n", sbuf);
				if(sbuf == NULL)
				{
					printf("err\n");
					break;
				}
				else if(strcmp(sbuf, "c") == 0)
				{
					printf("return to menu \n");
					break;
				}

				//printf("sbuf %s\n", sbuf);
				//입력된 값을 저장
				ret = g_menus[i].set_func( sbuf );
				printf("%s\n", ret == 0 ? "Sucess of Setting" : "Fail of Setting");
				break;
			}
			else
			{
				exit_nonsave( NULL );
			}
		}
	}

	return 0;
}

static int init_config_menu(void)
{
	init_shm( 0 );

	g_menu_list_length = sizeof(g_menus) / sizeof(menu_t);
	return 0;
}

static int release_config_menu(void)
{
	print_box("FMS RTU Configuration Exit");
	release_shm();
	return 0;
}

static int init_conf_file(void)
{
	int fd;
	char filePath[1024];

	// 경로 생성
	memset(filePath, 0, sizeof(filePath));
	sprintf(filePath, "%s%s", CONF_PART_1, VERIFY_FILE);

	if (access(filePath, F_OK) == 0)
	{
		return 0;
	}

	printf("Conf file is not existed.\n");

	fd = creat(filePath, 0644);
	if (fd < 0)
		return -1;
	printf("Conf file is created.\n");
	close(fd);
	return 0;
}

/* ================================== System FUNTION =================================================*/
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
			//print_dbg(DBG_INFO, 1, "SIGINIT Ctrl + C");
			//g_done = 1;
			print_text_menu();
		}
	}
}

static void init_sighandler( void )
{
	struct sigaction act;

	act.sa_handler = sighandler;
	sigemptyset( &act.sa_mask );
	act.sa_flags = 0;
	sigaction( SIGTERM, &act, 0 ); /* Terminate application */
	sigaction( SIGINT, &act, 0 ); /* control tty input ^C */
}

int main(int argc, char *argv[])
{
	print_dbg( DBG_LINE, 1, NULL );
	print_dbg( DBG_NONE , 1, "Start of RTU CLI" );
	print_dbg( DBG_LINE, 1, NULL );
	
	if (init_conf_file() < 0)
	{
		print_dbg( DBG_ERR , 1, "Shutdown CLI Becasue of Could not CLI Initialize !!" );
		return -1;
	}

	/* seg message 등록 */
	init_sighandler();

    memset( &g_db_info, 0, sizeof( g_db_info ));
  
	init_config_menu();

    if ( get_db_info_shm( &g_db_info ) != ERR_SUCCESS )
    {
        print_dbg( DBG_ERR,1,"Can not Read DB Info");
    }

	create_buf();

	set_debug_fd();

	/* debug wathchdog thread 생성 */
	create_debug_thread();

	while( g_done == 0)
	{
		print_text_menu();
	}

	send_test_mode("R", 1);
	sys_sleep( 30 );

	if( g_debug_fd > 0 )
		close( g_debug_fd );
	
	if ( g_recv_buf != NULL )
		free( g_recv_buf );

	release_config_menu();
	print_dbg( DBG_NONE , 1, "Release of RTU CLI" );

	return 0;
}
