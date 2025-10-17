#include "com_include.h"
#include "core_conf.h"
#include "conf.h"

#include "sem.h"

/*========================================================================================
	KOR : utf-8로 설정할것. 
  	선언부
  =======================================================================================*/
static volatile int g_sem_key = -1;
static volatile int g_sem_id = 0;
static volatile int g_partition_flag= 0;
static volatile int g_partition_app= 0;

static char g_szmainpart[ MAX_PATH_SIZE ];
static char g_szmainapp[ MAX_PATH_SIZE ];
static conf_data_t g_my_conf;

/*========================================================================================
 	기본 설정 값 
  =======================================================================================*/
session_default_value_t conf_tbl[] = {
	{ SESSION_DEF, 	KEY_VERIFY, 		VAL_VERIFY },	
	{ SESSION_DEF, 	KEY_SAVE_LOG, 		"0" },	
	{ NULL, NULL, NULL }
};

/* ========================== internal Functions ==============================================
 *
 * ===========================================================================================*/
static int check_partition( void )
{
	if ( g_partition_flag == 0 )
	{
		memset( g_szmainpart, 0, sizeof( g_szmainpart ));
		g_partition_flag = 1;
	}

	if ( g_partition_app == 0 )
	{
		memset( g_szmainapp, 0, sizeof( g_szmainapp ));
		g_partition_app = 1;
	}
	return ERR_SUCCESS;
}


static int internal_conf_load( conf_data_t * ptrconf_data )
{
	FILE *ptrfp;
	conf_t *ptrconf;
	char * ptrval= NULL;
	char szpath[ MAX_PATH_SIZE ];
	
	BYTE * ptr = NULL;
	conf_data_t * temp_ptrconf_data = NULL;

	if ( get_confpath( szpath, CONF_FILE ) != ERR_SUCCESS )
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

	ptr = ( BYTE *)malloc( sizeof(conf_data_t));
	temp_ptrconf_data = ( conf_data_t  *)ptr;
	
	if( temp_ptrconf_data == NULL )
	{
		return ERR_RETURN;
	}

	memset( temp_ptrconf_data, 0, sizeof( conf_data_t ));

	ptrval = conf_session_get_value( ptrconf, SESSION_DEF, KEY_VERIFY );
	if ( ptrval != NULL )
	{
		//print_dbg(DBG_NONE, 1, "CONF Verify code :%s", ptrval);
	}
	
	/* save log  */
	ptrval = conf_session_get_value( ptrconf, SESSION_DEF, KEY_SAVE_LOG );
	if ( ptrval != NULL )
	{
		temp_ptrconf_data->save_log= ( UINT32 )atoi( ptrval );
	}
	else
	{
		temp_ptrconf_data->save_log= 0;
	}

	lock_sem( g_sem_id, g_sem_key );
	memcpy(ptrconf_data, temp_ptrconf_data, sizeof(conf_data_t));
	unlock_sem( g_sem_id , g_sem_key );

	if (NULL != temp_ptrconf_data) { free(temp_ptrconf_data); temp_ptrconf_data = NULL; }

	release_conf( ptrconf );
	return ERR_SUCCESS;
}

static int save_conf_file( UINT32  val )
{
	conf_t * ptrconf = NULL;
	char szpath[ MAX_PATH_SIZE ];
	char szval[ MAX_PATH_SIZE ];
	int ret = ERR_SUCCESS;
	FILE * ptrfp = NULL;

	if ( get_confpath( szpath, CONF_FILE ) != ERR_SUCCESS )
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

	/* save log  code */
	memset( szval, 0, sizeof( szval ));
	sprintf( szval, "%u",val );

	conf_session_set_value( ptrconf, SESSION_DEF, KEY_SAVE_LOG, szval );
	ret = save_release_conf( ptrconf );

	if ( ret != ERR_SUCCESS )
	{
		print_dbg( DBG_ERR, 1, "Store Configuration File");
	}
	else
	{
		g_my_conf.save_log = val ;
	}

	return ERR_SUCCESS;
}

/* ========================== External Functions ==============================================
 *
 * ===========================================================================================*/
int init_com_conf( int key  )
{
	char szpath[ MAX_PATH_SIZE ];

	/* default config info */
	if ( get_confpath( szpath, CONF_FILE ) != ERR_SUCCESS )
	{
		return ERR_RETURN;
	}
	init_session_conf_file( szpath ,"=", conf_tbl );


	/* network info */
	memset( szpath, 0, sizeof(szpath));
	if ( get_confpath( szpath, NET_FILE ) != ERR_SUCCESS )
	{
		return ERR_RETURN;
	}

	if ( key > 0  && key != g_sem_key )
	{
		g_sem_key = key;
		g_sem_id = create_sem( g_sem_key );
		print_dbg( DBG_INFO, 1, "CONF SEMA : %d", g_sem_id );
	}

	init_config();
	
	return ERR_SUCCESS;
}

int release_com_conf( void )
{
	if ( g_sem_key > 0 )
	{
		destroy_sem( g_sem_id, g_sem_key );
		g_sem_key = -1;
	}
	print_dbg( DBG_INFO, 1, "Release of COM CONF");

	return ERR_SUCCESS;
}

int init_config( void )
{
	internal_conf_load( &g_my_conf );
#if 0 
	print_dbg( DBG_INFO, 1, "debug save-adc:%d", g_my_debug.save_adc );
	print_dbg( DBG_INFO, 1, "debug save-100sps:%d", g_my_debug.save_sps100 );
	print_dbg( DBG_INFO, 1, "debug save-20sps:%d", g_my_debug.save_sps20 );
	print_dbg( DBG_INFO, 1, "debug input vlts test :%d", g_my_debug.max_input );
#endif
	return ERR_SUCCESS;
}

conf_data_t * get_config( void )
{
	conf_data_t * ptr = NULL;
	ptr = &g_my_conf;
	return ptr;
}

int get_confpath( char * ptrpath, char * ptrfile )
{
	char * ptr = NULL;

	ptr = get_conf_main_confpath();
	if ( ptrpath == NULL || ptrfile == NULL  )
	{
		return ERR_RETURN;
	}

	if ( ptr == NULL || strlen( ptr ) <= 0 )
	{
		print_dbg( DBG_ERR, 1, "Fail to read Storage File(%s)", ptrfile );
		return ERR_RETURN;
	}

	memset( ptrpath , 0, MAX_PATH_SIZE );
	sprintf( ptrpath, "%s%s", ptr,ptrfile);
	return ERR_SUCCESS;
}

char * get_app_main_path( void )
{
	char *ptrpath, szpath[ MAX_PATH_SIZE ];
	FILE * fp = NULL;
	
	check_partition();

	ptrpath = NULL;
	ptrpath = g_szmainpart;

	if ( strlen( ptrpath ) > 0 )
	{
		return ptrpath ;
	}

	memset( szpath, 0, sizeof( szpath ));
	sprintf( szpath, "%s%s", APP_PATH_1, APP_PNAME );
	fp = fopen( szpath , "r");
	if ( fp != NULL )
	{
		memcpy( g_szmainpart, APP_PATH_1, strlen( APP_PATH_1 )); 
		fclose( fp );
		fp = NULL;
	}

	if ( strlen( g_szmainpart ) <= 0 )
	{
		/* PART2 */
		memset( szpath, 0, sizeof( szpath ));
		sprintf( szpath, "%s%s", APP_PATH_2, APP_PNAME );
		fp = fopen( szpath, "r");
		if ( fp != NULL )
		{
			memcpy( g_szmainpart, APP_PATH_2, strlen( APP_PNAME ) ); 
			fclose( fp );
			fp = NULL;
		}
	}

	if ( strlen( g_szmainpart ) > 0 )
		return g_szmainpart ;

	return NULL;
}

char * get_conf_main_confpath( void )
{
	char *ptrpath, szpath[ MAX_PATH_SIZE ];
	FILE * fp = NULL;
	
	check_partition();

	ptrpath = NULL;
	ptrpath = g_szmainapp;

	if ( strlen( ptrpath ) > 0 )
	{
		return ptrpath ;
	}

	memset( szpath, 0, sizeof( szpath ));
	sprintf( szpath, "%s%s",CONF_PART_1, VERIFY_FILE );
	fp = fopen( szpath , "r");
	if ( fp != NULL )
	{
		memcpy( g_szmainapp, CONF_PART_1, strlen( CONF_PART_1 )); 
		fclose( fp );
		fp = NULL;
	}

	if ( strlen( g_szmainapp ) <= 0 )
	{
		/* PART2 */
		memset( szpath, 0, sizeof( szpath ));
		sprintf( szpath, "%s%s", CONF_PART_2, VERIFY_FILE );
		fp = fopen( szpath, "r");
		if ( fp != NULL )
		{
			memcpy( g_szmainapp, CONF_PART_2, strlen( CONF_PART_2) ); 
			fclose( fp );
			fp = NULL;
		}
	}

	if ( strlen( g_szmainapp ) > 0 )
		return g_szmainapp ;

	return NULL;
}

int set_update_conf( UINT32 val )
{
    save_conf_file( val );

    return ERR_SUCCESS;
}

int get_update_conf( void  )
{
    int val = 0;

    conf_data_t conf;

    memset( &conf, 0, sizeof( conf));

    internal_conf_load( &conf );

    val = conf.save_log;

    return val;
}


