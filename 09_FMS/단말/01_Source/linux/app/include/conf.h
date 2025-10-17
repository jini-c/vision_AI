#ifndef _CONF_H__
#define _CONF_H__

#include "type.h"
#include "com_def.h"

/* ====== init/release ========================*/
int init_config( void );
int init_com_conf( int key  );
int release_com_conf( void );

/* ====== configuration data ==================*/
char * get_conf_main_confpath( void );
char * get_app_main_path( void );

int get_confpath( char * ptrpath, char * ptrfile );

conf_data_t * get_config( void );
int set_update_conf( unsigned int  val );
int get_update_conf( void  );

#endif

