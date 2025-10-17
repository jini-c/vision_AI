#ifndef _SHM_H__
#define _SHM_H__

#include "type.h"
#include "com_def.h"
#if 0
#include "protocol.h"
#endif

int init_shm( int key );
int release_shm( void );

int set_rtu_info_db_shm( void * ptrdata );
int set_rtu_net_shm( net_info_t * ptrnet );
int save_rtu_net_shm( net_info_t * ptrnet  );
net_info_t * get_rtu_net_shm( void );

int set_fms_net_shm( char * ptrip, unsigned short port );
int set_op_net_shm( unsigned short port );

int set_fms_server_net_sts( int val );
int get_fms_server_net_sts( void );

int get_my_siteid_shm( void );
int set_my_siteid_shm( int val );

int get_my_rtu_fcltid_shm( void );

int get_time_cycle_shm( void );

int get_send_cycle_shm( void );

int get_op_listen_port_shm( void );

char * get_fms_server_ip_shm( void );
int get_fms_server_port_shm( void );

int set_rtu_initialize_shm( int val );
int get_rtu_initialize_shm( void );

int set_rtu_version_shm( ver_info_t * ptrver );
ver_info_t * get_rtu_version_shm( void );
char * get_ifname_shm( void );

int get_db_info_shm( db_info_t * ptrdb );

int set_update_server_net_shm( char * ptrip, unsigned short port );
int get_update_server_net_shm( char * ptrip, unsigned short * ptrport );

int is_ver_checked_shm( void );

snsr_col_mng_t *  get_db_col_nm_shm( void );
int get_db_col_info_shm( int snsr_type, int snsr_sub_type, int snsr_idx, snsr_info_t * ptr_snsr_info );

int get_web_port_shm( void );
int set_web_port_shm( int port );

int set_print_debug_shm( int val );
int get_print_debug_shm( void );

int set_ntp_addr_shm( char * ptraddr );
int get_ntp_addr_shm( char * ptraddr );

int set_rtu_run_mode_shm( int mode );
int get_rtu_run_mode_shm( void );

int read_shm_sys_info( void );
int get_shm_sys_info( sys_info_t * ptrsys );


int get_rtu_web_shm( void );

#endif
