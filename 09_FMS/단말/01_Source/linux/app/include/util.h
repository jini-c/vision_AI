#ifndef _UTIL_H_
#define _UTIL_H_

#include <netinet/in.h>
#include <arpa/inet.h>
#include "com_def.h"

//#include <pthread.h>

/*------------------------------------------------------------------------------
 * definition
 */
#define MAXBUF              512
#define INTERFACE_UP        1
#define INTERFACE_DOWN      2

#define A_ALLOC(type, count) (type *)calloc( count, sizeof(type) )
#define A_ALLOC_NO_TYPE(type, count) (type )calloc( count, 1 )

#define A_FREE(pointer) { if ( NULL != pointer ) { free( pointer ); pointer = NULL; } }

/*------------------------------------------------------------------------------
 * util funtions
 */
// string & bit operation

void init_util( void );
void release_util( void );
int set_debug_send_func_util( send_debug_func debug_func );

void ltrim( char * ptrstr );
void rtrim( char * ptrstr );
void trim ( char * ptrstr );

int isAlnumStr( const char * ptrstr );
int isNumStr  ( const char * ptrstr );

void toupper_str   ( char * ptrstr );
void tolower_str( char * ptrstr );

int isvalidchar( int size, char * ptrdata );
char * get_macaddr ( const char * ptrifname );
char * get_ipaddr  ( const char * ptrifname );
int    set_ipaddr  ( const char * ptrifname,
                     const char * ptripaddr,
                     const char * ptrnetmask );
char * get_netmask ( const char * ptrifname );
int    set_ifconfig( const char * ptrifname, int status );
char* get_gateway(const char* ptrname );

int change_ipaddr ( char * ptripaddr );
int change_hwaddr ( char * ptrhwaddr );
int change_netmask( char * ptrifname,  char * ptripaddr, char * ptrnetmask );
int change_gateway( char * ptripaddr );

// process
int killall( const char * ptrpidname,  int signo );
int isexist_process( char * ptrname );
int iscnt_process( char * ptrname );
long * find_pid_by_name( const char * ptrpidname );

// time
int run_with_timeout (long timeout, void (* ptrfun) (void *), void * ptrarg);
int connect_with_timeout_interrupt( int fd, const struct sockaddr *ptraddr, socklen_t addrlen,long timeout );
int connect_with_timeout_poll( int sockfd, struct sockaddr *saddr, int addrsize, int sec );
void cnvt_year_to_time( unsigned char * ptryear , time_t * ptrtimes );
void cnvt_time_to_year( unsigned char * ptryear , time_t   times    );

int set_rtc_time( unsigned int sec , unsigned int msec, int clock_sync );
int get_cur_time( time_t t_time, char * ptrbuf, char * ptrbuf2 );
int get_cur_day( time_t t_time , char * ptrbuf , char * ptrbuf2 );
int get_cur_day_total( time_t t_time , char * ptrbuf, char * ptrbuf2 );

long long    get_tickcount_us( void );
unsigned int get_realtickcount( void );
unsigned int get_nowtickcount( struct timespec *now );
unsigned int get_sectick( void );

// system operation
void sys_sleep( int ms );
void sys_usleep( int us );
int system_cmd( char * ptrcmd );
int get_reset_type( void );

int logger_reset     ( char * ptrdesp );

//int add_default_gw( const char *ptrifname, const char *gateway );
int copy_binfile( char * ptrorgpath, char * ptrdestpath );
int save_binfile( char * ptrpath, unsigned char * ptrdata, unsigned int size );
int save_binfile_append( char * ptrpath, unsigned char * ptrdata, unsigned int size );
int read_binfile( char * ptrpath, unsigned char * ptrdata, unsigned int size );

int isexist_file( char * ptrpath );
int delete_file( char * ptrpath );

// configuratio file operation
int init_session_conf_file( const char *            ptrfilename,
                            const char *            ptrdelim   ,
                            session_default_value_t tbl[]      );
// debug print
int  print_dbg    ( unsigned char debug_type, int print_consol, char * fmt, ...);
void print_dbg_buf( int size, unsigned char * ptrdata );
void print_box    ( char * ptrdata ,... );
void print_line   ( void );
void print_buf( int size, unsigned char * ptrdata );
void print_step( int thd_id, int step, int val );

int get_status_mnt( char * path, int * total, int *used, int * free );
int get_free_ram( int * per );

int get_daemon_status( char * ptrservice );
int get_driver_status( char * ptrname );


int detect_directory( char * ptrpath );
int save_log_dat( char * ptrpath, char * ptrdata );

int cnvt_fclt_tid( tid_t * ptrtid );

int get_stringtover( char * ptrstr, unsigned char * ptrver );

int cnv_addr_to_strip( unsigned int s_addr, char * ptrip );

float read_file( char * ptrpath );

#endif
