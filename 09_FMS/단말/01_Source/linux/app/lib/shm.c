#include "com_include.h"
#include "core_conf.h"
#include "sem.h"
#include "shm.h"

static char g_ifname[ 10 ];


typedef struct net_manager_
{
	int cnt;
	net_info_t net_info[ MAX_FCLT_CNT ];
}net_manager_t;

/*================================================================================================
 전역 변수 
================================================================================================*/
static volatile int g_sem_key 						= -1;
static volatile int g_sem_id 						= 0;

static volatile int g_print_debug					= 0;
static volatile int g_rtu_webport					= 6006;
static char g_rtu_szntp_addr[ MAX_NTP_ADDR_SIZE ];
static volatile int g_rtu_run_mode					= RTU_RUN_SVR_LINK;

static rtu_info_t g_rtu_info;
static ver_info_t g_rtu_version;
static db_info_t g_db_info;
static update_net_t g_update_net;
static volatile int g_ver_checked               = 0;
static snsr_col_mng_t g_snsr_col_mng;
static sys_info_t   g_sys_info;
static volatile int g_rtu_web = 0;

/*================================================================================================
  내부 함수선언
================================================================================================*/
static int init_data( void );

static session_default_value_t run_tbl[] =
{
	{ SESSION_BASE,  KEY_VERIFY, VAL_VERIFY },
	{ SESSION_BASE,  KEY_IFNAME,  IFNAME },
	{ SESSION_BASE,  KEY_SITEID,  "9999" },

	{ SESSION_BASE,  KEY_DB_USER,	"fmsuser" },
	{ SESSION_BASE,  KEY_DB_PWR, 	"fms123" },
	{ SESSION_BASE,  KEY_DB_NAME, 	"FMSDB" },
	{ SESSION_BASE,  KEY_WEB_PORT, 	"6006" },
	{ SESSION_BASE,  KEY_NTP_ADDR, 	"192.168.0.121" },
	{ SESSION_BASE,  KEY_RTU_RUN_MODE, 	"0" },
	{ SESSION_BASE,  KEY_RTU_WEB, 	"0" },
	{ SESSION_BASE,  KEY_RTU_VER, 	"0.1" },
	{ NULL, NULL, NULL }
};

static session_default_value_t update_tbl[] =
{
	{ SESSION_BASE,  KEY_VERIFY, VAL_VERIFY },
	{ SESSION_BASE,  KEY_UPDATE_IP,  "1.1.1.1" },
	{ SESSION_BASE,  KEY_UPDATE_PORT,  "99" },

	{ NULL, NULL, NULL }
};

static default_value_t net_tbl[] =
{
	{ KEY_IFNAME,  IFNAME },
	{ KEY_IP,    "192.168.0.228" },
	{ KEY_GATE,  "192.168.0.1" },
	{ KEY_SUB,   "255.255.255.0" },
	{ NULL, NULL }
};

/*================================================================================================
내부 함수  정의 
================================================================================================*/

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
            //printf("...key:%s.....val:%s....\r\n",tbl[i].key, tbl[i].val );
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

static int save_siteid( int val )
{
	conf_t * ptrconf = NULL;
	char szpath[ MAX_PATH_SIZE ];
	char szval[ MAX_PATH_SIZE ];
	int ret = ERR_SUCCESS;
	FILE * ptrfp = NULL;

	if ( get_confpath( szpath, RUN_FILE ) != ERR_SUCCESS )
	{    
		return ERR_RETURN;
	}    

	ptrfp = fopen( szpath , "r" );
	if( ptrfp == NULL )
	{    
		return ERR_RETURN;
	}    

	if ( ptrfp != NULL ) fclose( ptrfp );

	ptrconf = load_conf( szpath , "=");
	if ( !ptrconf )
	{    
		print_dbg(DBG_ERR, 1, "Fail to read Common Configuration File");
		return ERR_RETURN;
	}    

	/* verify code */
	memset( szval, 0, sizeof( szval ));
	sprintf( szval, "%04d",val );

	conf_session_set_value( ptrconf, SESSION_BASE, KEY_SITEID, szval );

	ret = save_release_conf( ptrconf );
	if ( ret != ERR_SUCCESS )
	{    
		print_dbg( DBG_ERR, 1, "Store Configuration File");
	}    

	return ERR_SUCCESS;
}

static int save_webport( int val )
{
	conf_t * ptrconf = NULL;
	char szpath[ MAX_PATH_SIZE ];
	char szval[ MAX_PATH_SIZE ];
	int ret = ERR_SUCCESS;
	FILE * ptrfp = NULL;

	if ( get_confpath( szpath, RUN_FILE ) != ERR_SUCCESS )
	{    
		return ERR_RETURN;
	}    

	ptrfp = fopen( szpath , "r" );
	if( ptrfp == NULL )
	{    
		return ERR_RETURN;
	}    

	if ( ptrfp != NULL ) fclose( ptrfp );

	ptrconf = load_conf( szpath , "=");
	if ( !ptrconf )
	{    
		print_dbg(DBG_ERR, 1, "Fail to read Common Configuration File");
		return ERR_RETURN;
	}    

	/* verify code */
	memset( szval, 0, sizeof( szval ));
	sprintf( szval, "%d",val );

	conf_session_set_value( ptrconf, SESSION_BASE, KEY_WEB_PORT, szval );
	ret = save_release_conf( ptrconf );

	if ( ret != ERR_SUCCESS )
	{    
		print_dbg( DBG_ERR, 1, "Store Configuration File");
	}    

	return ret;
}

static int save_rtu_version( void )
{
	conf_t * ptrconf = NULL;
	char szpath[ MAX_PATH_SIZE ];
	int ret = ERR_SUCCESS;
	FILE * ptrfp = NULL;
	char szver[ MAX_PATH_SIZE ];

	if ( get_confpath( szpath, RUN_FILE ) != ERR_SUCCESS )
	{    
		return ERR_RETURN;
	}    

	ptrfp = fopen( szpath , "r" );
	if( ptrfp == NULL )
	{    
		return ERR_RETURN;
	}    


	if ( ptrfp != NULL ) fclose( ptrfp );

	ptrconf = load_conf( szpath , "=");
	if ( !ptrconf )
	{    
		print_dbg(DBG_ERR, 1, "Fail to read Common Configuration File");
		return ERR_RETURN;
	}    
	
	memset(&szver, 0, sizeof( szver ));
	sprintf( szver, "%d.%d", g_rtu_version.app_ver[ 0 ], 	g_rtu_version.app_ver[ 1 ] );
	conf_session_set_value( ptrconf, SESSION_BASE, KEY_RTU_VER , szver );

	ret = save_release_conf( ptrconf );
	if ( ret != ERR_SUCCESS )
	{    
		print_dbg( DBG_ERR, 1, "Store Configuration File");
	}    

	return ret;
}

static int save_ntp_addr( char * ptraddr )
{
	conf_t * ptrconf = NULL;
	char szpath[ MAX_PATH_SIZE ];
	int ret = ERR_SUCCESS;
	FILE * ptrfp = NULL;

	if ( get_confpath( szpath, RUN_FILE ) != ERR_SUCCESS )
	{    
		return ERR_RETURN;
	}    

	ptrfp = fopen( szpath , "r" );
	if( ptrfp == NULL )
	{    
		return ERR_RETURN;
	}    

	if ( ptrfp != NULL ) fclose( ptrfp );

	ptrconf = load_conf( szpath , "=");
	if ( !ptrconf )
	{    
		print_dbg(DBG_ERR, 1, "Fail to read Common Configuration File");
		return ERR_RETURN;
	}    
	
	conf_session_set_value( ptrconf, SESSION_BASE, KEY_NTP_ADDR , ptraddr );

	ret = save_release_conf( ptrconf );
	if ( ret != ERR_SUCCESS )
	{    
		print_dbg( DBG_ERR, 1, "Store Configuration File");
	}    

	return ret;
}

static int save_rtu_run_mode( int val )
{
	conf_t * ptrconf = NULL;
	char szpath[ MAX_PATH_SIZE ];
	char szval[ MAX_PATH_SIZE ];
	int ret = ERR_SUCCESS;
	FILE * ptrfp = NULL;

	if ( get_confpath( szpath, RUN_FILE ) != ERR_SUCCESS )
	{    
		return ERR_RETURN;
	}    

	ptrfp = fopen( szpath , "r" );
	if( ptrfp == NULL )
	{    
		return ERR_RETURN;
	}    

	if ( ptrfp != NULL ) fclose( ptrfp );

	ptrconf = load_conf( szpath , "=");
	if ( !ptrconf )
	{    
		print_dbg(DBG_ERR, 1, "Fail to read Common Configuration File");
		return ERR_RETURN;
	}    

	/* verify code */
	memset( szval, 0, sizeof( szval ));
	sprintf( szval, "%d",val );

	conf_session_set_value( ptrconf, SESSION_BASE, KEY_RTU_RUN_MODE, szval );
	ret = save_release_conf( ptrconf );

	if ( ret != ERR_SUCCESS )
	{    
		print_dbg( DBG_ERR, 1, "Store Configuration File");
	}    

	return ret;
}
static int load_config( rtu_info_t * ptrrtu_info, db_info_t * ptrdb_info  )
{
	FILE *ptrfp;
	conf_t *ptrconf;
	char * ptrval= NULL;
	char szpath[ MAX_PATH_SIZE ];

	BYTE * ptr = NULL;
	BYTE * ptr2 = NULL;

	rtu_info_t * temp_ptrconf_data = NULL;
	db_info_t * temp_ptrdb_data = NULL;

	if ( get_confpath( szpath, RUN_FILE ) != ERR_SUCCESS )
	{
		return ERR_RETURN;
	}

	ptrfp = fopen( szpath , "r" );
	if( ptrfp == NULL )
	{
		return ERR_RETURN;
	}

	if ( ptrfp != NULL ) fclose( ptrfp );

	ptrconf = load_conf( szpath, "=");
	if ( !ptrconf )
	{
		print_dbg(DBG_ERR, 1, "Fail to read Common Configuration File");
		return ERR_RETURN;
	}

	ptr     = ( BYTE *)malloc( sizeof(rtu_info_t ));
	ptr2    = ( BYTE *)malloc( sizeof(db_info_t ));

	temp_ptrconf_data = ( rtu_info_t  *)ptr;
	temp_ptrdb_data = ( db_info_t  *)ptr2;

	if( temp_ptrconf_data == NULL || temp_ptrdb_data == NULL )
	{
		return ERR_RETURN;
	}

	memset( temp_ptrconf_data, 0, sizeof( rtu_info_t ));
	memset( temp_ptrdb_data, 0, sizeof( db_info_t ));

	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_VERIFY );
	if ( ptrval != NULL )
	{
		//print_dbg(DBG_NONE, 1, "CONF Verify code :%s", ptrval);
	}

	/* site id */
	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_SITEID);
	if ( ptrval != NULL )
	{
		temp_ptrconf_data->siteid= atoi( ptrval );
	}
	else
	{
		temp_ptrconf_data->siteid= 0;
	}
	
	/* IFNAME */
	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_IFNAME);
	if ( ptrval != NULL )
	{
		memcpy( g_ifname, ptrval, strlen( ptrval ));
	}
	else
	{
		memcpy( g_ifname, IFNAME, strlen(IFNAME ));
	}
	
	/* dbuser  name */
	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_DB_USER);
	if ( ptrval != NULL )
	{
		memcpy(temp_ptrdb_data->szdbuser, ptrval, strlen( ptrval ) );
	}
	else
	{
		memcpy(temp_ptrdb_data->szdbuser, "fmsuser", strlen( "fmsuser" ) );
	}
	
	/* dbuser  pwr */
	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_DB_PWR );
	if ( ptrval != NULL )
	{
		memcpy(temp_ptrdb_data->szdbpwr, ptrval, strlen( ptrval ) );
	}
	else
	{
		memcpy(temp_ptrdb_data->szdbpwr, "fms123", strlen( "fms123" ) );
	}

	/* db  name */
	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_DB_NAME);
	if ( ptrval != NULL )
	{
		memcpy(temp_ptrdb_data->szdbname, ptrval, strlen( ptrval ) );
	}
	else
	{
		memcpy(temp_ptrdb_data->szdbname, "FMSDB", strlen( "FMSDB" ) );
	}
	
	/* web port */
	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_WEB_PORT );
	if ( ptrval != NULL )
	{
		g_rtu_webport = atoi( ptrval );
	}
	else
	{
		g_rtu_webport = 6006 ;
	}

	/* ntp addr  */
	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_NTP_ADDR );
	if ( ptrval != NULL )
	{
		memcpy( g_rtu_szntp_addr, ptrval,strlen( ptrval )) ;
	}
	else
	{
		memcpy( g_rtu_szntp_addr, "192.168.0.121",strlen( "192.168.0.121" )) ;
	}

	/* rtu run mode  */
	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_RTU_RUN_MODE );
	if ( ptrval != NULL )
	{
		g_rtu_run_mode = atoi( ptrval );
	}
	else
	{
		g_rtu_run_mode = RTU_RUN_SVR_LINK;
	}

	/* rtu web  */
	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_RTU_WEB );
	if ( ptrval != NULL )
	{
		g_rtu_web = atoi( ptrval );
	}
	else
	{
		g_rtu_web = 0;
	}

	lock_sem( g_sem_id, g_sem_key );
	memcpy(ptrrtu_info, temp_ptrconf_data, sizeof(rtu_info_t));
	memcpy(ptrdb_info, temp_ptrdb_data, sizeof(db_info_t));
	unlock_sem( g_sem_id , g_sem_key );

	if (NULL != temp_ptrconf_data) { free(temp_ptrconf_data); temp_ptrconf_data = NULL; }
	if (NULL != temp_ptrdb_data) { free(temp_ptrdb_data); temp_ptrdb_data = NULL; }

	release_conf( ptrconf );

	return ERR_SUCCESS;
}

static int load_rtu_net_config( void  )
{
	FILE *ptrfp;
	conf_t *ptrconf;
	char * ptrval= NULL;
	char szpath[ MAX_PATH_SIZE ];

	BYTE * ptr = NULL;
	net_info_t * temp_net = NULL;

	if ( get_confpath( szpath, NET_FILE ) != ERR_SUCCESS )
	{
		return ERR_RETURN;
	}

	ptrfp = fopen( szpath , "r" );
	if( ptrfp == NULL )
	{
		return ERR_RETURN;
	}

	if ( ptrfp != NULL ) fclose( ptrfp );

	ptrconf = load_conf( szpath, "=");
	if ( !ptrconf )
	{
		print_dbg(DBG_ERR, 1, "Fail to read Common Configuration File");
		return ERR_RETURN;
	}

	ptr     = ( BYTE *)malloc( sizeof(net_info_t));

	temp_net = ( net_info_t*)ptr;

	if( temp_net == NULL )
	{
		return ERR_RETURN;
	}

	memset( temp_net, 0, sizeof( net_info_t));
	
	/* rtu ip  */
	ptrval = conf_get_value( ptrconf , KEY_IP );
	if ( ptrval != NULL )
	{
		memcpy( temp_net->ip , ptrval, strlen( ptrval ));
	}
	else
	{
		memcpy( temp_net->ip , "192.168.0.228", strlen( "192.168.0.228" ));
	}
	
	/* rtu gateway  */
	ptrval = conf_get_value( ptrconf , KEY_GATE );
	if ( ptrval != NULL )
	{
		memcpy( temp_net->gateway , ptrval, strlen( ptrval ));
	}
	else
	{
		memcpy( temp_net->gateway , "192.168.0.1", strlen( "192.168.0.1" ));
	}
	
	/* rtu sub  */
	ptrval = conf_get_value( ptrconf , KEY_SUB );
	if ( ptrval != NULL )
	{
		memcpy( temp_net->netmask, ptrval, strlen( ptrval ));
	}
	else
	{
		memcpy( temp_net->netmask , "255,255,255,0", strlen( "255.255.255.0" ));
	}
	
	
	/* dbuser  name */
	lock_sem( g_sem_id, g_sem_key );
	memcpy( &g_rtu_info.net, temp_net, sizeof( net_info_t ));
	unlock_sem( g_sem_id , g_sem_key );

	if (NULL != temp_net ) 
    { 
        free(temp_net ); 
        temp_net = NULL; 
    }

	release_conf( ptrconf );

	return ERR_SUCCESS;
}

static int load_update_config( update_net_t * ptrupdate  )
{
	FILE *ptrfp;
	conf_t *ptrconf;
	char * ptrval= NULL;
	char szpath[ MAX_PATH_SIZE ];

	BYTE * ptr = NULL;
	update_net_t * temp_up_data = NULL;

	if ( get_confpath( szpath, UPDATE_FILE ) != ERR_SUCCESS )
	{
		return ERR_RETURN;
	}

	ptrfp = fopen( szpath , "r" );
	if( ptrfp == NULL )
	{
		return ERR_RETURN;
	}

	if ( ptrfp != NULL ) fclose( ptrfp );

	ptrconf = load_conf( szpath, "=");
	if ( !ptrconf )
	{
		print_dbg(DBG_ERR, 1, "Fail to read Common Configuration File");
		return ERR_RETURN;
	}

	ptr     = ( BYTE *)malloc( sizeof(update_net_t));

	temp_up_data = ( update_net_t *)ptr;

	if( temp_up_data == NULL )
	{
		return ERR_RETURN;
	}

	memset( temp_up_data, 0, sizeof( update_net_t ));

	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_VERIFY );
	if ( ptrval != NULL )
	{
		//print_dbg(DBG_NONE, 1, "CONF Verify code :%s", ptrval);
	}

	/* update ip  */
	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_UPDATE_IP );
	if ( ptrval != NULL )
	{
		memcpy( temp_up_data->szip , ptrval, strlen( ptrval ));
	}
	else
	{
		memcpy( temp_up_data->szip , "1.1.1.1", strlen( "1.1.1.1" ));
	}
	
	/* update port  */
	ptrval = conf_session_get_value( ptrconf, SESSION_BASE, KEY_UPDATE_PORT );
	if ( ptrval != NULL )
	{
	    temp_up_data->port = (unsigned short )atoi( ptrval );
    }
	else
	{
	    temp_up_data->port = 99;
	}
	
	/* dbuser  name */
	lock_sem( g_sem_id, g_sem_key );
	memcpy(ptrupdate , temp_up_data, sizeof(update_net_t));
	unlock_sem( g_sem_id , g_sem_key );

	if (NULL != temp_up_data) 
    { 
        free(temp_up_data); 
        temp_up_data = NULL; 
    }

	release_conf( ptrconf );

	return ERR_SUCCESS;
}

static int save_update_net( void )
{
	conf_t * ptrconf = NULL;
	char szpath[ MAX_PATH_SIZE ];
	char szval[ MAX_PATH_SIZE ];
	int ret = ERR_SUCCESS;
	FILE * ptrfp = NULL;

	if ( get_confpath( szpath, UPDATE_FILE ) != ERR_SUCCESS )
	{    
		return ERR_RETURN;
	}    

	ptrfp = fopen( szpath , "r" );
	if( ptrfp == NULL )
	{    
		return ERR_RETURN;
	}    

	if ( ptrfp != NULL ) fclose( ptrfp );

	ptrconf = load_conf( szpath , "=");
	if ( !ptrconf )
	{    
		print_dbg(DBG_ERR, 1, "Fail to read Update Configuration File");
		return ERR_RETURN;
	}    

	/* verify code */
	conf_session_set_value( ptrconf, SESSION_BASE, KEY_UPDATE_IP, g_update_net.szip );

	memset( szval, 0, sizeof( szval ));
	sprintf( szval, "%d",g_update_net.port );
	conf_session_set_value( ptrconf, SESSION_BASE, KEY_UPDATE_PORT, szval );

	ret = save_release_conf( ptrconf );
	if ( ret != ERR_SUCCESS )
	{    
		print_dbg( DBG_ERR, 1, "Fail to Store Update Configuration File");
	}    


	return ERR_SUCCESS;
}


static int save_network( void )
{
	conf_t * ptrconf = NULL;
	char szpath[ MAX_PATH_SIZE ];
	char szval[ MAX_PATH_SIZE ];
	int ret = ERR_SUCCESS;
	FILE * ptrfp = NULL;

	if ( get_confpath( szpath, NET_FILE ) != ERR_SUCCESS )
	{    
		return ERR_RETURN;
	}    

	ptrfp = fopen( szpath , "r" );
	if( ptrfp == NULL )
	{    
		return ERR_RETURN;
	}    

	if ( ptrfp != NULL ) fclose( ptrfp );

	ptrconf = load_conf( szpath , "=");
	if ( !ptrconf )
	{    
		print_dbg(DBG_ERR, 1, "Fail to read Common Configuration File");
		return ERR_RETURN;
	}    

	/* verify code */
	//memcpy( g_rtu_info.net.ip, 		ptrrtu_info->net.ip, 		strlen( ptrrtu_info->net.ip ));
	//memcpy( g_rtu_info.net.netmask, ptrrtu_info->net.netmask,  	strlen( ptrrtu_info->net.netmask ));
	//memcpy( g_rtu_info.net.gateway, ptrrtu_info->net.gateway , 	strlen( ptrrtu_info->net.gateway ));


    /* ifname */
	conf_set_value( ptrconf, KEY_IFNAME, g_ifname );

    /* ip */
	memset( szval, 0, sizeof( szval ));
	sprintf( szval, "%s",g_rtu_info.net.ip );
	conf_set_value( ptrconf, KEY_IP, szval );

    /* gate */
	memset( szval, 0, sizeof( szval ));
	sprintf( szval, "%s",g_rtu_info.net.gateway );
	conf_set_value( ptrconf, KEY_GATE, szval );

    /* sub */
	memset( szval, 0, sizeof( szval ));
	sprintf( szval, "%s",g_rtu_info.net.netmask );
	conf_set_value( ptrconf, KEY_SUB, szval );

	ret = save_release_conf( ptrconf );
	if ( ret != ERR_SUCCESS )
	{    
		print_dbg( DBG_ERR, 1, "Store Network Configuration File");
	}    

	return ERR_SUCCESS;
}

static int create_snsr_col_buf( void )
{
	char * ptr = NULL;
	int size = sizeof( snsr_col_nm_t ) * MAX_COL_NAME_CNT ;

	ptr  = ( char  *)malloc( size );

	if ( ptr != NULL )
	{	
		memset( ptr, 0, size );
		g_snsr_col_mng.ptrcol_nm = ( snsr_col_nm_t *)ptr;
	}
	else
	{
		print_dbg( DBG_ERR,1," Cannot Create Snsr Col Name Buffer" );
	}

	return ERR_SUCCESS;
}

static int release_snsr_col_buf ( void )
{
	if ( g_snsr_col_mng.ptrcol_nm != NULL )
	{
		free( g_snsr_col_mng.ptrcol_nm);
		g_snsr_col_mng.ptrcol_nm= NULL;
	}
	return ERR_SUCCESS;

}

static int init_data( void )
{
	int ret = ERR_SUCCESS;

	memset( &g_rtu_info, 0, sizeof( g_rtu_info ));
	memset( &g_rtu_version, 0, sizeof( g_rtu_version ));
	memset( &g_ifname, 0, sizeof( g_ifname ));
    memset( &g_db_info, 0, sizeof( g_db_info ));
    memset( &g_update_net, 0, sizeof( g_update_net ));
	
	//printf("============STEP 1=========================\r\n");

	ret = load_config( &g_rtu_info, &g_db_info );

	if ( ret != ERR_SUCCESS )
	{
		print_dbg( DBG_ERR,1,"Can not load Configuration File");
	}

#if 0
    print_dbg( DBG_INFO, 1, "//DB Info User:%s, Pwr:%s, Name:%s",
            g_db_info.szdbuser, g_db_info.szdbpwr, g_db_info.szdbname );
#endif

	ret = load_rtu_net_config( );
	if ( ret != ERR_SUCCESS )
	{
		print_dbg( DBG_ERR,1,"Can not load RTU NETWORK File");
	}

	create_snsr_col_buf();

	ret = load_update_config( &g_update_net );

	if ( ret != ERR_SUCCESS )
	{
		print_dbg( DBG_ERR,1,"Can not load Configuration File");
	}


	if ( ret != ERR_SUCCESS )
	{
		print_dbg( DBG_ERR,1,"Can not load Configuration File");
	}

	return ret;
}

/*================================================================================================
  외부 함수 정의
================================================================================================*/
int init_shm( int key  )
{
	char szpath[ MAX_PATH_SIZE ];

	int ret = ERR_SUCCESS;

	/* default config info */
	if ( get_confpath( szpath, RUN_FILE ) != ERR_SUCCESS )
	{
		return ERR_RETURN;
	}

	init_session_conf_file( szpath ,"=", run_tbl );

    /* update file */
	if ( get_confpath( szpath, UPDATE_FILE ) != ERR_SUCCESS )
	{
		return ERR_RETURN;
	}

	init_session_conf_file( szpath ,"=",update_tbl );

    /* net file */
	if ( get_confpath( szpath, NET_FILE ) != ERR_SUCCESS )
	{
		return ERR_RETURN;
	}

    init_net_file( szpath ,"=", net_tbl );

	if ( key > 0  && key != g_sem_key )
	{
		g_sem_key = key;
		g_sem_id = create_sem( g_sem_key );
		print_dbg( DBG_INFO, 1, "SHM SEMA : %d", g_sem_id );
	}


	ret = init_data( );

	 memset( &g_sys_info, 0, sizeof( g_sys_info ));

	return ret ;
}

int release_shm( void )
{
	if ( g_sem_key > 0 )
	{
		destroy_sem( g_sem_id, g_sem_key );
		g_sem_key = -1;
	}

	release_snsr_col_buf();

	print_dbg( DBG_INFO, 1, "Release of SHM");

	return ERR_SUCCESS;
}

int set_rtu_info_db_shm( void * ptrdata )
{
	rtu_info_t * ptrrtu_info = NULL;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "is null ptrrtu info [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}
	ptrrtu_info = ( rtu_info_t *) ptrdata;

	/* rtu info */
#if 0
	
	memcpy( g_rtu_info.net.ip, "192.168.0.228", strlen( "192.168.0.228"  ));
	memcpy( g_rtu_info.net.netmask, "255.255.255.0", strlen("255.255.255.0" ));
	memcpy( g_rtu_info.net.gateway, "192.168.0.1", strlen("192.168.0.1" ));
#else

	g_rtu_info.fcltid 		= ptrrtu_info->fcltid;
	g_rtu_info.time_cycle	= ptrrtu_info->time_cycle * 60 ; /*분단위 */
	g_rtu_info.op_port		= ptrrtu_info->op_port; 
	g_rtu_info.send_cycle	= ptrrtu_info->send_cycle * 60; /* 분단위 */
	
	/* RTU IP 정보를 파일에서 읽어 온다 */
#if 0
	memcpy( g_rtu_info.net.ip, 		ptrrtu_info->net.ip, 		strlen( ptrrtu_info->net.ip ));
	memcpy( g_rtu_info.net.netmask, ptrrtu_info->net.netmask,  	strlen( ptrrtu_info->net.netmask ));
	memcpy( g_rtu_info.net.gateway, ptrrtu_info->net.gateway , 	strlen( ptrrtu_info->net.gateway ));
#endif
	
	memset( g_rtu_info.fms_server_ip, 	0, MAX_IP_SIZE );	
	memcpy( g_rtu_info.fms_server_ip, 	ptrrtu_info->fms_server_ip, 	strlen( ptrrtu_info->fms_server_ip ));
	g_rtu_info.fms_server_port = ptrrtu_info->fms_server_port;
    
	/* RTU IP는 CLI에서만 변경, 등록 한다 2016-1030*/
    //save_network();

#endif
	
	printf("============STEP 2=========================\r\n");
	print_dbg( DBG_LINE, 1, NULL );
	print_dbg( DBG_SAVE, 1, "RTU SITE ID:%d, FCLT_ID:%d, Send Cycle:%d, Time Cycle:%d,",
							g_rtu_info.siteid, 
							g_rtu_info.fcltid, 
							g_rtu_info.send_cycle,
							g_rtu_info.time_cycle);

	print_dbg( DBG_SAVE, 1, "RTU IP:%s, Mask:%s, Gateway:%s,",
							g_rtu_info.net.ip,
							g_rtu_info.net.netmask,
							g_rtu_info.net.gateway );

	print_dbg( DBG_SAVE, 1, "FMS Server IP:%s, FMS Server Port:%d,",
							g_rtu_info.fms_server_ip,
							g_rtu_info.fms_server_port);

	print_dbg( DBG_SAVE, 1, "OP Listen Port:%d,",
							g_rtu_info.op_port);

	print_dbg( DBG_LINE, 1, NULL );

	return ERR_SUCCESS;
}

int set_rtu_net_shm( net_info_t * ptrnet )
{
	if ( ptrnet == NULL )
	{
		print_dbg( DBG_ERR,1, "is null ptrnet info [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}

	/* RTU IP는 CLI에서만 변경, 등록 한다 2016-1030*/

#if 0    
    //printf("..........................ip:%s, netmask:%s...........gate:%s...\r\n", ptrnet->ip, ptrnet->netmask,ptrnet->gateway );
    
    if( strlen( ptrnet->ip ) == 0 || strlen( ptrnet->netmask ) == 0 || strlen( ptrnet->gateway ) == 0 )
    {
		print_dbg( DBG_ERR,1, "is null ptrnet info [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;

    }

	memcpy( g_rtu_info.net.ip, 		ptrnet->ip, 		strlen( ptrnet->ip ));
	memcpy( g_rtu_info.net.netmask, ptrnet->netmask,  	strlen( ptrnet->netmask ));
	memcpy( g_rtu_info.net.gateway, ptrnet->gateway , 	strlen( ptrnet->gateway ));

    save_network();

	print_dbg( DBG_LINE, 1, NULL );
	print_dbg( DBG_NONE, 1, "Change Of RTU IP:%s, Mask:%s, Gateway:%s,",
							g_rtu_info.net.ip,
							g_rtu_info.net.netmask,
							g_rtu_info.net.gateway );
#endif

    return ERR_SUCCESS;

}

int save_rtu_net_shm( net_info_t * ptrnet )
{
	if ( ptrnet == NULL )
	{
		print_dbg( DBG_ERR,1, "is null ptrnet info [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}

    //printf("..........................ip:%s, netmask:%s...........gate:%s...\r\n", ptrnet->ip, ptrnet->netmask,ptrnet->gateway );
    
    if( strlen( ptrnet->ip ) == 0 || strlen( ptrnet->netmask ) == 0 || strlen( ptrnet->gateway ) == 0 )
    {
		print_dbg( DBG_ERR,1, "is null ptrnet info [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
    }

	memset( g_rtu_info.net.ip, 0 , MAX_IP_SIZE );
	memset( g_rtu_info.net.netmask, 0 , MAX_IP_SIZE );
	memset( g_rtu_info.net.gateway, 0 , MAX_IP_SIZE );

	memcpy( g_rtu_info.net.ip, 		ptrnet->ip, 		strlen( ptrnet->ip ));
	memcpy( g_rtu_info.net.netmask, ptrnet->netmask,  	strlen( ptrnet->netmask ));
	memcpy( g_rtu_info.net.gateway, ptrnet->gateway , 	strlen( ptrnet->gateway ));

    save_network();

	print_dbg( DBG_LINE, 1, NULL );
	print_dbg( DBG_SAVE, 1, "Change Of RTU IP:%s, Mask:%s, Gateway:%s,",
							g_rtu_info.net.ip,
							g_rtu_info.net.netmask,
							g_rtu_info.net.gateway );

    return ERR_SUCCESS;

}

net_info_t * get_rtu_net_shm( void )
{
	return &g_rtu_info.net;
}

int set_fms_net_shm( char * ptrip, unsigned short port )
{
	if ( ptrip == NULL || strlen( ptrip ) == 0 || port == 0 )
	{
		print_dbg( DBG_ERR,1, "is null ptrnet info [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}

	memcpy( g_rtu_info.fms_server_ip, 		ptrip, 		strlen( ptrip ));
    g_rtu_info.fms_server_port = port;

	print_dbg( DBG_LINE, 1, NULL );
	print_dbg( DBG_NONE, 1, "Change of FMS Server IP:%s, FMS Server Port:%d,",
							g_rtu_info.fms_server_ip,
							g_rtu_info.fms_server_port);

    return ERR_SUCCESS;

}

int set_op_net_shm( unsigned short port )
{
	if ( port == 0 )
	{
		print_dbg( DBG_ERR,1, "is null ptrnet info [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}

    g_rtu_info.op_port = port;

	print_dbg( DBG_NONE, 1, "Change of OP Listen Port:%d,",
							g_rtu_info.op_port);
    return ERR_SUCCESS;
}

int set_fms_server_net_sts( int sts )
{
	int val = 0;


	lock_sem( g_sem_id, g_sem_key );

	val = g_rtu_info.fms_net_sts ;

	if ( val != sts )
	{
		g_rtu_info.fms_net_sts = sts ;
	}

	unlock_sem( g_sem_id, g_sem_key );

	return ERR_SUCCESS;
}

int get_fms_server_net_sts( void )
{
	int val =0;

	lock_sem( g_sem_id, g_sem_key );

	val = g_rtu_info.fms_net_sts ;

	unlock_sem( g_sem_id, g_sem_key );
	
	return val;
}

int get_my_siteid_shm( void )
{
	return g_rtu_info.siteid;
}

int set_my_siteid_shm( int val )
{
	int ret = 0;

	if ( val != g_rtu_info.siteid )
	{
		ret= save_siteid( val );
		if ( ret == ERR_SUCCESS )
		{
			g_rtu_info.siteid = val;
		}
	}

	return ret;
}

int get_my_rtu_fcltid_shm( void )
{
	return g_rtu_info.fcltid;
}

int get_time_cycle_shm( void )
{
	return g_rtu_info.time_cycle;
}

int get_send_cycle_shm( void )
{
    int val = REAL_MOR_TIME_SEC;
#if 0
	val = g_rtu_info.send_cycle;
#endif
	return val;
}

int get_op_listen_port_shm( void )
{
	return g_rtu_info.op_port;
}

char * get_fms_server_ip_shm( void )
{
	return g_rtu_info.fms_server_ip;
}

int get_fms_server_port_shm( void )
{
	return g_rtu_info.fms_server_port;
}

int set_rtu_initialize_shm( int val )
{
	g_rtu_info.init_sts = val;
	return ERR_SUCCESS;
}

int get_rtu_initialize_shm( void )
{
	return g_rtu_info.init_sts;
}

int set_rtu_version_shm( ver_info_t * ptrver )
{
	if( ptrver == NULL )
	{
		print_dbg( DBG_ERR, 1, "pterver is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	memcpy(&g_rtu_version, ptrver, sizeof( g_rtu_version ));

	print_dbg(DBG_NONE, 1, "RTU Observer Version:%d.%d"			, g_rtu_version.ob_ver[ 0 ], 	g_rtu_version.ob_ver[ 1 ] );
	print_dbg(DBG_NONE, 1, "RTU Version:%d.%d"			, g_rtu_version.app_ver[ 0 ], 	g_rtu_version.app_ver[ 1 ] );
	print_dbg(DBG_NONE, 1, "RTU MCU Version:%d.%d"				, g_rtu_version.mcu_ver[ 0 ], 	g_rtu_version.mcu_ver[ 1 ] );
	print_dbg(DBG_NONE, 1, "RTU Aircondition Control Version:%d.%d"	, g_rtu_version.remote_ver[ 0 ],g_rtu_version.remote_ver[ 1 ]);
	print_dbg(DBG_NONE, 1, "RTU Temprature Version:%d.%d"		, g_rtu_version.temp_ver[ 0 ], 	g_rtu_version.temp_ver[ 1 ] );
	print_dbg(DBG_NONE, 1, "RTU Power Control Version:%d.%d"	, g_rtu_version.pwr_c_ver[ 0 ], g_rtu_version.pwr_c_ver[ 1 ]  );
	print_dbg(DBG_LINE, 1, NULL );
    g_ver_checked	= 1;

	save_rtu_version();

	return ERR_SUCCESS;
}

ver_info_t * get_rtu_version_shm( void )
{
	return &g_rtu_version ;
}

char * get_ifname_shm( void )
{
	return g_ifname;
}

int get_db_info_shm( db_info_t * ptrdb )
{
    if ( ptrdb == NULL )
    {
		print_dbg( DBG_ERR, 1, "pterver is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
        return ERR_RETURN;
    }

    memcpy( ptrdb, &g_db_info, sizeof( db_info_t ));

    return ERR_SUCCESS;
}

int set_update_server_net_shm( char * ptrip, unsigned short port )
{
    if ( ptrip == NULL || strlen( ptrip ) == 0 ||port <=0 )
    {
		print_dbg( DBG_ERR, 1, "Update Server Net is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
        return ERR_RETURN;
    }
   
    memset( g_update_net.szip, 0, sizeof(g_update_net) );
    memcpy( g_update_net.szip, ptrip, strlen( ptrip ));
    g_update_net.port = port;

    save_update_net();

    return ERR_SUCCESS;
}

int get_update_server_net_shm( char * ptrip, unsigned short * ptrport )
{
	if ( ptrip == NULL || ptrport  == NULL )
	{
		print_dbg( DBG_ERR, 1, "Update Server Net is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memcpy(  ptrip, g_update_net.szip , strlen( g_update_net.szip ));
	*ptrport = g_update_net.port;

	return ERR_SUCCESS;

}

int is_ver_checked_shm( void )
{
	return g_ver_checked;
}


snsr_col_mng_t *  get_db_col_nm_shm( void )
{
	return &g_snsr_col_mng;
}


int get_db_col_info_shm( int snsr_type, int snsr_sub_type, int snsr_idx, snsr_info_t * ptr_snsr_info )
{
	int i , ret,find;
	int cnt =0;
	snsr_col_nm_t *ptrcol = NULL;
	int l_snsr_id , l_snsr_type, l_snsr_sub_type, l_snsr_idx;

	if ( ptr_snsr_info == NULL )
	{

		print_dbg( DBG_ERR, 1, "ptrcol_mngis Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	cnt = g_snsr_col_mng.cnt;
	find = 0;
	//print_dbg( DBG_INFO,1, "GET CNT:%d", cnt );
	
	ptrcol = g_snsr_col_mng.ptrcol_nm;
	for( i = 0 ; i < cnt ; i++ )
	{
		if ( ptrcol != NULL )
		{
			l_snsr_id		= ptrcol->snsr_id;
			l_snsr_type 	= ptrcol->snsr_type;
			l_snsr_sub_type = ptrcol->snsr_sub_type ;
			l_snsr_idx		= ptrcol->snsr_idx ;

			if ( l_snsr_type == snsr_type && l_snsr_sub_type == snsr_sub_type && snsr_idx == l_snsr_idx )
			{
				ptr_snsr_info->snsr_id =l_snsr_id ;
				if ( ptrcol->szcol != NULL )
				{
					memcpy( ptr_snsr_info->szcol, ptrcol->szcol, strlen( ptrcol->szcol ));
				}
				find = 1;
				break;
			}
			
			ptrcol++;
		}
	}

	if ( find == 1 )
		ret = ERR_SUCCESS;
	else
		ret = ERR_RETURN;

	return ret ;
}

int get_web_port_shm( void )
{
	return g_rtu_webport;
}

int set_web_port_shm( int port )
{
	int ret = ERR_SUCCESS;

	if ( port != g_rtu_webport )
	{
		ret= save_webport( port );
		if ( ret == ERR_SUCCESS )
		{
			g_rtu_webport= port;
		}
	}
	else
		ret = ERR_RETURN;

	return ret;
}


int set_ntp_addr_shm( char * ptraddr )
{
	if ( ptraddr == NULL || strlen( ptraddr ) == 0 || strlen( ptraddr ) > MAX_NTP_ADDR_SIZE )
	{
		print_dbg( DBG_SAVE, 1, "Is null ntp address");
		return ERR_RETURN;
	}
	
	if ( strcmp( ptraddr, g_rtu_szntp_addr ) != 0 )
	{
		if ( save_ntp_addr( ptraddr ) == ERR_SUCCESS )
		{
			memset( g_rtu_szntp_addr, 0, sizeof( g_rtu_szntp_addr ));
			memcpy( g_rtu_szntp_addr, ptraddr, strlen( ptraddr ));
		}
	}

	return ERR_SUCCESS;
}

int get_ntp_addr_shm( char * ptraddr )
{
	if ( ptraddr == NULL )
	{
		print_dbg( DBG_SAVE, 1, "Is null ntp address");
		return ERR_RETURN;
	}
	
	//sprintf( g_rtu_szntp_addr, "%s", "192.168.0.121" );

	if ( strlen( g_rtu_szntp_addr ) <=0 )
		return ERR_RETURN;
	

	memcpy( ptraddr, g_rtu_szntp_addr, strlen( g_rtu_szntp_addr ));

	return ERR_SUCCESS;
}


int set_print_debug_shm( int val )
{
	g_print_debug = val;
	return ERR_SUCCESS;
}

int get_print_debug_shm( void )
{
	return g_print_debug;
}

int set_rtu_run_mode_shm( int mode )
{
	int ret = ERR_SUCCESS;

	if ( mode != g_rtu_run_mode )
	{
		ret= save_rtu_run_mode( mode );
		if ( ret == ERR_SUCCESS )
		{
			g_rtu_run_mode = mode ;
		}
	}
	return ERR_SUCCESS;
}

int get_rtu_run_mode_shm( void )
{
	return g_rtu_run_mode ;
}

int read_shm_sys_info( void )
{
	int ret = ERR_SUCCESS;
	float fval1, fval2, fval3 = 0;
	
	fval1 = fval2 = fval3 = 0;
	fval1 = read_file( SYS_CPU );
	fval2 = read_file( SYS_RAM );
	fval3 = read_file( SYS_HRD );
	
	/*
	float use_cpu;
	float use_mem;
	float use_hard;
	*/
	if ( fval1 > 0.0 )
	{
		g_sys_info.use_cpu = fval1;
	}
	else
		ret = ERR_RETURN;

	if ( fval2 > 0.0 )
	{
		g_sys_info.use_mem = fval2;
	}
	else
		ret = ERR_RETURN;

	if ( fval3 > 0.0 )
	{
		g_sys_info.use_hard = fval3;
	}
	else
		ret = ERR_RETURN;

	return ret;
}

int get_shm_sys_info( sys_info_t * ptrsys )
{
	if ( ptrsys == NULL )
	{
		print_dbg( DBG_ERR,1, "Isvalid ptrsys");
		return ERR_RETURN;
	}

	ptrsys->use_cpu = g_sys_info.use_cpu;
	ptrsys->use_mem = g_sys_info.use_mem;
	ptrsys->use_hard = g_sys_info.use_hard;

	return ERR_SUCCESS;
}

int get_rtu_web_shm( void )
{
	return g_rtu_web;
}


