#include "com_include.h"
#include "core_conf.h"
#include "conf.h"
#include "util.h"
#include "gpio.h"

//#include <linux/watchdog.h>

default_value_t ifcfg_eth0_tbl[] = 
{ 
	{ KEY_MY_IP     , "192.168.0.169"     },
	{ KEY_MY_NETMASK, "255.255.255.0"     },
	{ KEY_MY_GATEWAY, "192.168.0.1"       },
	{ KEY_MY_MAC    , "8C:53:F7:00:09:69" },
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

static int init_variable( void )
{
	char * ptrpath = NULL;
	char szpath[ MAX_PATH_SIZE ];

	ptrpath = get_conf_main_confpath();
	if ( ptrpath != NULL && strlen( ptrpath ) > 0 )
	{
		memset( szpath, 0, sizeof( szpath ));
		sprintf( szpath, "%s%s", ptrpath, NET_CONF_FILE );
		print_dbg( DBG_NONE, 1,"NET PATH:%s", szpath );
		init_net_file( szpath, "=", ifcfg_eth0_tbl );
	}

	return 0;
}

static int check_watchdog( void )
{
	int fd = -1;
#if 0
	int bwatchdog ;
	bwatchdog = 1;

	if ( bwatchdog == 1 )
	{
		fd = open( RU_DEV_WTD , O_RDWR );
		
		if ( fd >= 0 )
		{
			ioctl(fd , WDIOC_KEEPALIVE, NULL);
			close( fd );
		}

		print_dbg( DBG_INFO, 1, "Watchdog is used");
	}
	else
	{
		print_dbg( DBG_INFO, 1, "Watchdog is unused");
	}
#endif
	return fd;
}

int main( int argc, char *argv[] )
{
	
	init_gpio( 0 );
	release_gpio( );

	print_dbg( DBG_LINE, 1, NULL );
	print_dbg( DBG_NONE , 1, "Initialize Ver: %d.%d", APP_VER1, APP_VER2 );
	print_dbg( DBG_LINE, 1, NULL );
	
	make_verify_file();
	init_config();
	init_variable();
	check_watchdog();
	
	return 0;
}
