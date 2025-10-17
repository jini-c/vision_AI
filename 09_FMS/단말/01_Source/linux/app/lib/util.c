#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <termios.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <linux/if_ether.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <sys/types.h>
#include <signal.h>

#include <setjmp.h>
#include <assert.h>
#include <fcntl.h> 
#include <errno.h> 

#include "sys/time.h"
#include <time.h>
#include <errno.h>
#include <linux/rtc.h>
#include <fcntl.h>
#include <sys/vfs.h>
#include <stdarg.h>
#include <pthread.h>

#include "util.h"
#include "type.h"
//#include "linux_def.h"

#include "core_conf.h"

#define MAX_READ_BUF_SIZE  ( 1024 * 10 )

static sigset_t s_sigmask;
static struct sigaction s_sig_act;
static send_debug_func  g_send_debug_func = NULL;

const char *MMOUNT = "/proc/mounts";

struct f_size
{
	long blocks;
	long avail;
};

typedef struct _mountinfo
{
	FILE *fp;                // 파일 스트림 포인터
	char devname[80];        // 장치 이름
	char mountdir[80];        // 마운트 디렉토리 이름
	char fstype[12];        // 파일 시스템 타입
	struct f_size size;        // 파일 시스템의 총크기/사용율
} MOUNTP;



/*=========================================================================================*/
static int write_err( char * ptrdata, int size )
{
	FILE *wfp = NULL;
	char szbuf[ MAX_PRINT_SIZE ];
	char sztime[ MAX_TIME_SIZE ];
	char filename[128];

	time_t cur_time = time( NULL );
	struct tm cur_st;
	
	if( ptrdata == NULL )
		return -1;

	memset( &cur_st, 0, sizeof( struct tm ));
	memset( szbuf,  0, sizeof( szbuf ));
	memset( sztime, 0, sizeof( sztime ));
	memset( filename, 0, sizeof( filename ));

	localtime_r( &cur_time, &cur_st );

	sprintf(sztime, "%04d/%02d/%02d %02d:%02d:%02d",
			cur_st.tm_year + 1900, cur_st.tm_mon + 1, cur_st.tm_mday, cur_st.tm_hour, cur_st.tm_min, cur_st.tm_sec);

	sprintf(szbuf, "%s%s\r", sztime, ptrdata);
	sprintf(filename, "/home/fms/log/%04d_%02d_%02d.log", cur_st.tm_year + 1900, cur_st.tm_mon + 1, cur_st.tm_mday);

	wfp = fopen(filename, "a+" );

	if( wfp != NULL )
	{
		fwrite(szbuf, 1, strlen(szbuf), wfp);
		fclose(wfp);
	}
	
	return 0;
}


/* -------------------------------------------------------------------------- */
// Local functions

static void sig_handler( int signo )
{
#if 0
	if ( ( SIGTERM == signo ) || ( SIGINT == signo ) )
	{
		*s_Main_Done = 1;

		fw_dbg( D_INFO, "\n  << User request to stop service.... >>\n\n");
	}
	else
	{
		//      fw_dbg( D_INFO, "Signal NO. = (%d) \n", signo );
	}
#endif
}

/* -------------------------------------------------------------------------- */
// init : for sys_sleep, sys_usleep
void init_util( void )
{

	// for block
	sigfillset( &s_sigmask);           // all init
	sigdelset ( &s_sigmask, SIGTERM ); // except SIGTERM
	sigdelset ( &s_sigmask, SIGINT  ); // except SIGINT
	sigdelset ( &s_sigmask, SIGQUIT ); // except SIGQUIT
	sigdelset ( &s_sigmask, SIGKILL ); // except SIGKILL
	sigdelset ( &s_sigmask, SIGUSR1 ); // except SIGUSR1
	sigdelset ( &s_sigmask, SIGTSTP ); // except SIGTSTP    

	s_sig_act.sa_handler = (void *)sig_handler;
	s_sig_act.sa_mask  = s_sigmask;
	s_sig_act.sa_flags = SA_NOMASK;

	sigaction( SIGTERM, &s_sig_act, 0 );
	sigaction( SIGINT , &s_sig_act, 0 );
	sigaction( SIGQUIT, &s_sig_act, 0 );
	sigaction( SIGKILL, &s_sig_act, 0 );
	sigaction( SIGUSR1, &s_sig_act, 0 );
	sigaction( SIGTSTP, &s_sig_act, 0 );

}


void release_util( void )
{


}

int set_debug_send_func_util( send_debug_func debug_func )
{
	g_send_debug_func = debug_func;
	return ERR_SUCCESS;
}

void ltrim( char * ptrstr )
{
	char *s = ptrstr;
	while( isspace( *s ))
		s++;
	while(( *ptrstr++ = *s++ ));
}

void rtrim( char * ptrstr )
{
	char * s = ptrstr;
	register int i;

	for( i = strlen( s ) -1; isspace( *( s+i ) ); i-- )
		*( s+i ) = '\0';
}

void trim( char * ptrstr )
{
	rtrim( ptrstr );
	ltrim( ptrstr );
}

int isAlnumStr( const char * ptrstr )
{
	const char *ptr = ptrstr;
	int i;

	for( i = 0; i < strlen( ptrstr ); i++, ptr++ )
	{
		if( !isalnum( *ptr ) )
			return 0;
	}
	
	return 1;
}

int isNumStr( const char * ptrstr )
{
	const char *ptr = ptrstr;
	int i;

	for( i = 0; i < strlen( ptrstr ); i++, ptr++ )
	{
		if( !isdigit( *ptr ) )
			return 0;
	}
	
	return 1;
}

void toupper_str( char * ptrstr )
{
	char *ptr = ptrstr;

	while( *ptr )
	{
		if( islower( *ptr ) )
			*ptr = toupper( *ptr );
		ptr++;
	}
}
void tolower_str( char * ptrstr )
{
	char *ptr = ptrstr;

	while( *ptr )
	{
		if( isupper( *ptr ) )
			*ptr = tolower( *ptr );
		ptr++;
	}
}


static void *xrealloc( void * ptr, size_t size )
{
	ptr = realloc(ptr, size);
	if (ptr == NULL && size != 0)
		return NULL;
	return ptr;
}

long *find_pid_by_name( const char * ptridname)
{
	DIR *dir;
	struct dirent *next;
	long* pidList=NULL;
	int i=0;

	dir = opendir("/proc");
	if (!dir)
		return NULL;

	while ((next = readdir(dir)) != NULL) {
		FILE *status;
		char filename[MAXBUF];
		char buffer[MAXBUF];
		char name[MAXBUF];

		/* Must skip ".." since that is outside /proc */
		if (strcmp(next->d_name, "..") == 0)
			continue;

		/* If it isn't a number, we don't want it */
		if (!isdigit(*next->d_name))
			continue;

		sprintf(filename, "/proc/%s/status", next->d_name);
		if (! (status = fopen(filename, "r")) ) {
			continue;
		}
		if (fgets(buffer, MAXBUF-1, status) == NULL) {
			fclose(status);
			continue;
		}
		fclose(status);

		/* Buffer should contain a string like "Name: binary_name" */
		sscanf(buffer, "%*s %s", name);

			//printf("pname %s, org name %s..\n",name, ptridname );
		if (strcmp(name, ptridname) == 0) {
			//printf("pname %s, org name %s..\n",name, ptridname );
			pidList=xrealloc( pidList, sizeof(long) * (i+2));
			pidList[i++]=strtol(next->d_name, NULL, 0);
			break;
		}
	}

	if (pidList) 
	{
		pidList[i]=0;
	} 
	else 
	{
		pidList=xrealloc( pidList, sizeof(long));
		pidList[0]=-1;
	}
	return pidList;
}

char *get_macaddr( const char *ptrifname )
{
	int skfd;
	struct ifreq ifr;
	char temp[10];
	static char hwaddr[32];

	skfd = socket( AF_INET, SOCK_DGRAM, 0 );
	if( skfd < 0 )
		return NULL;

	memset( &ifr, 0, sizeof( ifr ) );

	strcpy( ifr.ifr_name, ptrifname );
	if( ioctl( skfd, SIOCGIFHWADDR, &ifr ) < 0 )
	{
		close( skfd );
		return NULL;
	}

	memcpy( temp, ifr.ifr_hwaddr.sa_data, ETH_ALEN );
	snprintf( hwaddr, sizeof( hwaddr ), "%02X:%02X:%02X:%02X:%02X:%02X",
			((int)temp[0]) & 0xFF, ((int)temp[1]) & 0xFF,
			((int)temp[2]) & 0xFF, ((int)temp[3]) & 0xFF,
			((int)temp[4]) & 0xFF, ((int)temp[5]) & 0xFF );

	close( skfd );

	return hwaddr;
}

char *get_ipaddr( const char *ptrifname )
{
	struct ifreq ifr;
	struct sockaddr_in addr;
	int skfd;
	static char ipaddr[16];

	skfd = socket( AF_INET, SOCK_DGRAM, 0 );
	if ( skfd < 0 )
		return NULL;

	strcpy( ifr.ifr_name, ptrifname );

	/* Check if IP address is configured. */
	if (ioctl( skfd, SIOCGIFADDR, &ifr ) < 0 )
	{
		close( skfd );
		return NULL;
	}

	memcpy( &addr, &ifr.ifr_addr, sizeof( addr ));

	memset( ipaddr, 0, sizeof( ipaddr ));
	strcpy( ipaddr, inet_ntoa( addr.sin_addr ));

	close( skfd );

	return ipaddr;
}

int set_ipaddr( const char *ptrifname, const char *ptripaddr, const char *ptrnetmask )
{
	struct ifreq    ifr;
	struct sockaddr_in *p = (struct sockaddr_in *)&(ifr.ifr_addr);
	int skfd;

	memset( &ifr, 0, sizeof( struct ifreq ));
	strcpy( ifr.ifr_name, ptrifname );

	skfd = socket( AF_PACKET, SOCK_PACKET, 0x0003 );

	if ( skfd < 0 )
		return ERR_RETURN;

	p->sin_family = AF_INET;
	p->sin_addr.s_addr = inet_addr( ptripaddr );

	if ( ptripaddr != NULL )
	{
		if ( ioctl( skfd, SIOCSIFADDR, &ifr ) == -1 )  /* setting IP address */
		{
			close( skfd );
			return ERR_RETURN;
		}
	}

	if ( ptrnetmask != NULL )
	{
		p->sin_addr.s_addr = inet_addr( ptrnetmask );
		if ( ioctl( skfd, SIOCSIFNETMASK, &ifr ) == -1 )  /* setting ptrnetmask */
		{
			close( skfd );
			return ERR_RETURN;
		}

		p->sin_addr.s_addr = inet_addr( ptripaddr ) | ~inet_addr( ptrnetmask );
		if ( ioctl( skfd, SIOCSIFBRDADDR, &ifr ) == -1 ) /* setting broadcast address */
		{
			close( skfd );
			return ERR_RETURN;
		}
	}

	return ERR_SUCCESS;
}

char *get_netmask( const char *ptrname )
{
	struct ifreq ifr;
	struct sockaddr_in saddr;
	int skfd;
	static char netmask[16];

	skfd = socket( AF_INET, SOCK_DGRAM, 0 );
	if( skfd < 0 )
		return NULL;

	strcpy(ifr.ifr_name, ptrname);

	if( ioctl( skfd, SIOCGIFNETMASK, &ifr ) < 0 ) 
	{
		close( skfd );
		return NULL;
	}

	memcpy(&saddr, &ifr.ifr_netmask, sizeof(saddr));

	strcpy( netmask, inet_ntoa( saddr.sin_addr ));

	close( skfd );

	return netmask;
}

char* get_gateway(const char* ptrname ) 
{
	static char gateway[ 20 ];
	char cmd [100] ;
	FILE* fp = NULL;
	char line[256];
	int len =0;

	memset( gateway, 0, sizeof( gateway ));
	memset( cmd, 0, sizeof( cmd ));
	memset( line, 0, sizeof( line ));

	sprintf(cmd,"route -n | grep %s  | grep 'UG[ \t]' | awk '{print $2}'", ptrname );
	fp = popen( cmd, "r");
	
	if ( fp != NULL )
	{
		if(fgets(line, sizeof(line), fp) != NULL)
		{
			len = strlen( line );
			line[ len ] = 0x00;
			memcpy( gateway, line, len -1);

		}
		pclose(fp);
	}

	return gateway;
}

int set_ifconfig( const char *ptrifname, int status )
{
	int skfd;
	struct ifreq ifr;

	skfd = socket( AF_INET, SOCK_DGRAM, 0 );
	if( skfd < 0 )
		return ERR_RETURN;

	strncpy( ifr.ifr_name, ptrifname, IFNAMSIZ );
	if( ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0 )
	{
		close( skfd );
		return ERR_RETURN;
	}

	strncpy(ifr.ifr_name, ptrifname, IFNAMSIZ);

	if( status == INTERFACE_UP )
		ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
	else if( status == INTERFACE_DOWN )
		ifr.ifr_flags &= ~IFF_UP;
	else
	{
		close( skfd );
		return ERR_RETURN;
	}

	if( ioctl( skfd, SIOCSIFFLAGS, &ifr ) < 0 )
	{
		close( skfd );
		return ERR_RETURN;
	}

	close( skfd );

	return ERR_SUCCESS;
}

#if 0
int add_default_gw( const char *ptrifname, const char *gateway )
{
	struct ifreq    ifr;
	struct sockaddr_in *p = (struct sockaddr_in *)&(ifr.ifr_addr);
	struct rtentry	rtent;
	int skfd;
	unsigned int gateway_ip;

	memset(&rtent,0,sizeof(struct rtentry));

	gateway_ip = inet_addr( gateway );

	skfd = socket( AF_PACKET, SOCK_PACKET, 0x0003 );
	if ( skfd < 0 )
		return ERR_RETURN;

	p               	= (struct sockaddr_in *)&rtent.rt_dst;
	p->sin_family   	= AF_INET;
	p->sin_addr.s_addr 	= INADDR_ANY;

	p               	= (struct sockaddr_in *)&rtent.rt_gateway;
	p->sin_family   	= AF_INET;
	memcpy( &p->sin_addr.s_addr, &gateway_ip, 4 );

	p               	= (struct sockaddr_in *)&rtent.rt_genmask;
	p->sin_family   	= AF_INET;
	p->sin_addr.s_addr  = 0;
	rtent.rt_dev        = (char *)ptrifname;
	rtent.rt_metric     = 1;
	rtent.rt_flags      = RTF_UP|RTF_GATEWAY;
	if ( ioctl( skfd, SIOCADDRT, &rtent ) )
	{
		close( skfd );
		return ERR_RETURN;
	}


	return ERR_SUCCESS;
}

#endif

int isexist_process( char * ptrname )
{
	int result ;
	long *pidList = NULL ;

	pidList = ( long * )find_pid_by_name( ptrname );

	if ( !pidList || *pidList <= 0 )
	{
		result =  0;
	}
	else
	{
		result =  1;
	}

	if( pidList != NULL ) free( pidList );

	return result;

}

int iscnt_process( char * ptrname )
{
	FILE  *fPipe;
	int		iRet = -1;
	char	szcmd[256];

	memset( szcmd, 0, sizeof( szcmd ));

	sprintf(szcmd, "ps -ef | grep %s | wc -l", ptrname );
	//sprintf(szcmd, "ps -ef | grep %s", ptrname );

	fPipe = popen(szcmd, "r");
	if( fPipe != NULL )
	{
		memset(szcmd, 0x00, sizeof(szcmd));
#if 0
		while( fgets(szcmd, sizeof(szcmd)-1, fPipe) != NULL)
		{
			printf("==========================%s.....\r\n", szcmd );
		}
#else
		if( fgets(szcmd, sizeof(szcmd)-1, fPipe) == NULL )
		{
			fprintf(stderr, "\nError System Call[%s]\n", szcmd);
			iRet = -1;
		}
		else
		{
			iRet = atoi(szcmd);
			iRet -= 2;	/* grep( 2 ) 으로 나타난 ptrname의 개수는 뺌 */
		}
		pclose(fPipe);
#endif
	}

	//printf(".........iRet:%d................................\r\n", iRet );

	return iRet;	
}

int killall( const char *ptridname,  int signo )
{
	//pid_t myPid = getpid();
	long *pidList;
	int sig = signo;

	pidList = ( long * )find_pid_by_name( ptridname );

	if ( !pidList || *pidList <= 0 )
	{
		printf("piList is null...\n");
		return ERR_RETURN;
	}
	else
	{
		for (; *pidList != 0; pidList++ )
		{
#if 0
			if ( *pidList == myPid )
			{
				printf("same pid \n"); 
				continue;
			}
#endif
			if ( kill( *pidList, sig ) != 0 )
			{
				free( pidList );
				print_dbg(DBG_ERR,1, "Dont Kill %s", ptridname ); 
				return ERR_RETURN;
			}
			else
			{
				//printf("killed name..%s\n", ptridname ); 
			}
		}
	}
	
	if ( pidList != NULL )
	{
		//free( pidList );
	}
	//printf("kill end..\n"); 

	return ERR_SUCCESS;
}

struct cwt_context {
	int fd;
	const struct sockaddr *addr;
	socklen_t addrlen;
	int result;
};

#  define SETJMP(env) sigsetjmp (env, 1)
static sigjmp_buf run_with_timeout_env;

static void
abort_run_with_timeout (int sig)
{
	print_dbg( DBG_ERR, 1, "Sig timeout assert");
	//assert (sig == SIGALRM);
	if ( sig == SIGALRM )
	{
		siglongjmp (run_with_timeout_env, -1);
	}
}

static void
alarm_set (long timeout)
{
	/* Use the modern itimer interface. */
	struct itimerval itv;
	memset (&itv, 0, sizeof (itv));
	itv.it_value.tv_sec = (long) timeout;
	itv.it_value.tv_usec = 1000000L * (timeout - (long)timeout);
	if (itv.it_value.tv_sec == 0 && itv.it_value.tv_usec == 0)
		/* Ensure that we wait for at least the minimum interval.
		 *        Specifying zero would mean "wait forever".  */
		itv.it_value.tv_usec = 1;
	setitimer (ITIMER_REAL, &itv, NULL);
}

static void
alarm_cancel (void)
{
	struct itimerval disable;
	memset (&disable, 0, sizeof (disable));
	setitimer (ITIMER_REAL, &disable, NULL);
}

int
run_with_timeout (long timeout, void (*ptrfun) (void *), void *ptrarg)
{
	int saved_errno;

	if (timeout == 0)
	{
		ptrfun (ptrarg);
		return ERR_SUCCESS;
	}

	signal (SIGALRM, abort_run_with_timeout);
	if (SETJMP (run_with_timeout_env) != 0)
	{
		/* Longjumped out of FUN with a timeout. */
		signal (SIGALRM, SIG_DFL);
		return 1;
	}
	alarm_set (timeout);
	ptrfun (ptrarg);

	/* Preserve errno in case alarm() or signal() modifies it. */
	saved_errno = errno;
	alarm_cancel ();
	signal (SIGALRM, SIG_DFL);
	errno = saved_errno;

	return ERR_SUCCESS;
}

static void
connect_with_timeout_callback (void *ptrarg)
{
	struct cwt_context *ctx = (struct cwt_context *)ptrarg;
	ctx->result = connect (ctx->fd, ctx->addr, ctx->addrlen);
}

int connect_with_timeout_interrupt( int fd, const struct sockaddr *ptraddr, socklen_t addrlen, long timeout )
{
	struct cwt_context ctx;
	ctx.fd = fd;
	ctx.addr = ptraddr;
	ctx.addrlen = addrlen;

	if (run_with_timeout (timeout, connect_with_timeout_callback, &ctx))
	{
		errno = ETIMEDOUT;
		return -1;
	}
	if (ctx.result == -1 && errno == EINTR)
		errno = ETIMEDOUT;

	return ctx.result;
}

int connect_with_timeout_poll( int sockfd, struct sockaddr *saddr, int addrsize, int sec )
{
	int newSockStat; 
	int orgSockStat; 
	int res, n; 
	fd_set  rset, wset; 
	struct timeval tval; 

	int error = 0; 
	int esize; 

	if ( (newSockStat = fcntl(sockfd, F_GETFL, NULL)) < 0 )  
	{ 
		print_dbg(DBG_ERR, 1, "F_GETFL error[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ ); 
		return -1; 
	} 

	orgSockStat = newSockStat; 
	newSockStat |= O_NONBLOCK; 

	// Non blocking 상태로 만든다.  
	if(fcntl(sockfd, F_SETFL, newSockStat) < 0) 
	{ 
		print_dbg(DBG_ERR, 1, "F_SETLF error[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ ); 
		return -1; 
	} 

	// 연결을 기다린다. 
	// Non blocking 상태이므로 바로 리턴한다. 
	if((res = connect(sockfd, saddr, addrsize)) < 0) 
	{ 
		if (errno != EINPROGRESS) 
		{
			print_dbg(DBG_ERR, 1, "EINPROGRESS"); 
			return -1; 
		}
	} 

	//print_dbg(DBG_INFO, 1, "RES : %d", res); 
	// 즉시 연결이 성공했을 경우 소켓을 원래 상태로 되돌리고 리턴한다.  
	if (res == 0) 
	{ 
		//print_dbg(DBG_INFO,1,"Connect Success"); 
		fcntl(sockfd, F_SETFL, orgSockStat); 
		return 1; 
	} 

	FD_ZERO(&rset); 
	FD_SET(sockfd, &rset); 
	wset = rset; 

	tval.tv_sec     = sec;     
	tval.tv_usec    = 0; 

	if ( (n = select(sockfd+1, &rset, &wset, NULL, &tval)) == 0) 
	{ 
		// timeout 
		errno = ETIMEDOUT;     
		//print_dbg(DBG_ERR, 1, "Socket Timeout"); 
		return -1; 
	} 

	// 읽거나 쓴 데이터가 있는지 검사한다.  
	if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset) ) 
	{ 
		//print_dbg(DBG_INFO,1, "Read data"); 
		esize = sizeof(int); 
		if ((n = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&esize)) < 0) 
		{
			print_dbg(DBG_ERR, 1, "Read Data Err[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__); 
			return -1; 
		}
	} 
	else 
	{ 
		print_dbg(DBG_INFO,1, "Socket Not Set[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__); 
		return -1; 
	} 

	fcntl(sockfd, F_SETFL, orgSockStat); 
	if(error) 
	{ 
		errno = error; 
		//print_dbg(DBG_INFO,1,"Socket Error[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__); 
		return -1; 
	} 

	return 1; 
}

void cnvt_year_to_time( unsigned char * ptryear ,time_t *ptrtimes )
{
	struct tm t_time;
	time_t cnv_time = 0;

	t_time.tm_year = ( int )(( ptryear[ 0 ] + 2000  ) - 1900 );
	t_time.tm_mon  = ( int )( ptryear[ 1 ] - 1 );
	t_time.tm_mday = ( int )( ptryear[ 2 ] );
	t_time.tm_hour = ( int )( ptryear[ 3 ] );
	t_time.tm_min  = ( int )( ptryear[ 4 ] );
	t_time.tm_sec  = ( int )( ptryear[ 5 ] );

	cnv_time = mktime( &t_time );
	//cnv_time -= ( 600 * 6 * 9 ) ;
	*ptrtimes= cnv_time;
}

void cnvt_time_to_year( unsigned char * ptryear, time_t times )
{
	time_t cur_time;
	struct tm cur_st;

	if ( times == 0 )
	{
		cur_time = time( NULL );
	}
	else
	{
		cur_time = times;
	}
	
	memset( &cur_st, 0, sizeof( struct tm ));
	localtime_r( &cur_time, &cur_st );
	
	*ptryear++ = ( unsigned char )( ( cur_st.tm_year + 1900 ) - 2000 );
	*ptryear++ = ( unsigned char )(cur_st.tm_mon + 1 );
	*ptryear++ = ( unsigned char )cur_st.tm_mday;
	*ptryear++ = ( unsigned char )cur_st.tm_hour;
	*ptryear++ = ( unsigned char )cur_st.tm_min;
	*ptryear++ = ( unsigned char )cur_st.tm_sec;
}

int set_rtc_time( unsigned int sec, unsigned int msec,int clock_sync   )
{
    struct timeval tv;
	//char buf[ 50 ];
	char cmd[ 50 ];
	int ret = 0;

	if ( sec == 0 )
	{
		print_dbg( DBG_ERR,1,"Not have RTC Sec");
		return ERR_RETURN;
	}

	memset( &tv, 0, sizeof( struct timeval ));
	memset( cmd, 0, sizeof( cmd ));

	/* set system time */
    tv.tv_sec = sec;
	tv.tv_usec = msec * 1000;
    ret = settimeofday( &tv, NULL );
	
	if ( ret == 0 )
	{
		print_dbg( DBG_INFO,1,"Success of Time Set with settimeofday");
	}
	else
	{

		print_dbg( DBG_ERR,1,"Fail of Time Set with settimeofday:%d", errno );
		if ( errno == EFAULT)
		{
			printf(".......1\r\n");
		}
		else if ( errno == EINVAL )
		{
			printf(".......2\r\n");

		}
		else if ( errno == EPERM )
		{
			printf(".......3\r\n");

		}
		else
		{
			printf(".......4\r\n");

		}
	}

	if( clock_sync )
	{
		sprintf( cmd,"%s"," sudo hwclock -w -u");
		system_cmd( cmd );
	}

	//get_cur_time( ( sec + ADD_9H ), cmd, NULL );
	//print_dbg(DBG_INFO,1,"######### Set RTC time(clock_sync:%d) : %s", clock_sync,cmd );

#if 0
    int fd;
    time_t cur;
    struct rtc_time rtc;
    struct tm *tm;

	memset( &rtc,0, sizeof( struct rtc_time ));
	/* set rtc time */
    fd = open( LOGGER_DEV_RTC, O_RDWR );
    if( fd > 0 )
    {
		cur = sec;

		rtc.tm_sec = tm->tm_sec;
		rtc.tm_min = tm->tm_min;
		rtc.tm_hour = tm->tm_hour;
		rtc.tm_mday = tm->tm_mday;
		rtc.tm_mon = tm->tm_mon;
		rtc.tm_year = tm->tm_year;
		rtc.tm_wday = tm->tm_wday;
		rtc.tm_yday = tm->tm_yday;
		rtc.tm_isdst = tm->tm_isdst;

		if ( ioctl( fd, RTC_SET_TIME, &rtc ) == 0 )
		{
			//memset( buf, 0, sizeof( buf ));
			//memset( temp , 0, sizeof( temp ));

			//get_cur_time( 0, buf );
			//print_line();
			//sprintf(temp,"Set RTC Time %s\n", buf );
			//printf( temp );
		}
		else
		{
			print_dbg(DBG_ERR, 1, "Fail to Set RTC Time");
		}

		close( fd );
		/* some delay..*/
	
		sprintf( cmd, "year:%d, month:%d, day:%d hh:%d, mm:%d, ss:%d\n",
				tm->tm_year+1900, tm->tm_mon+ 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec );
		print_dbg(DBG_INFO,1,"Set RTC time: %s", cmd );
	}
	else
	{
		print_dbg(DBG_ERR, 1 , "Fail to Set RTC Drive Open");
	}
#endif

    return ERR_SUCCESS;
}

int change_ipaddr( char * ptripaddr )
{
	char * porgipaddr = NULL;
	
	porgipaddr = get_ipaddr( IFNAME );
	//printf("read  org ip : %s , new ip : %s..\n", porgipaddr, ptripaddr );

	//if ( porgipaddr != NULL && pipaddr != NULL )
	if ( ptripaddr != NULL )
	{
		if ( strcmp( porgipaddr, ptripaddr ) != 0 )
		{
			if ( set_ipaddr( IFNAME, ptripaddr, NULL ) == 0 )
			{
				//printf("changed  ip orgip : %s , new ip : %s..\n", porgipaddr, ptripaddr );
				return ERR_SUCCESS;
			}
		}
	}

	return ERR_RETURN;
}

int change_netmask( char * ptrifname, char * ptripaddr, char * ptrnetmask )
{
	
	char cmd[ 150 ];

	memset( cmd, 0, sizeof( cmd ));

	if ( ptripaddr != NULL && ptrnetmask != NULL && ptrifname )
	{
		//sprintf(cmd , "ifconfig %s %s netmask %s", IFNAME, ptripaddr, ptrnetmask );
		sprintf(cmd , "sudo ifconfig %s %s netmask %s", ptrifname , ptripaddr, ptrnetmask );
#if 0
	system( cmd );
#else
	system_cmd( cmd );
#endif
		sys_sleep( 100 );
	}

	return ERR_RETURN;
}

int change_gateway( char * ptripaddr )
{
	
	char cmd[ 150 ];

	memset( cmd, 0, sizeof( cmd ));

	if ( ptripaddr != NULL )
	{
		sprintf(cmd , "sudo route add default gw %s" , ptripaddr );
#if 0
	system( cmd );
#else
	system_cmd( cmd );
#endif
		sys_sleep( 100 );
	}

	return ERR_RETURN;
}

int change_hwaddr( char * ptrhwaddr )
{
	char * porghwaddr = NULL;
	char cmd1[ 100 ];

	porghwaddr = get_macaddr( IFNAME );
	toupper_str( porghwaddr );
	toupper_str( ptrhwaddr );

	//printf("read  org mac : %s , new mac : %s..\n", porghwaddr, ptrhwaddr );

	memset( cmd1, 0, sizeof( cmd1 ));

	if ( porghwaddr != NULL && ptrhwaddr != NULL )
	{
		if( strcmp( porghwaddr, ptrhwaddr ) != 0 )
		{
			sprintf( cmd1, "ifconfig %s hw ether %s", IFNAME, ptrhwaddr ); 

			set_ifconfig( IFNAME, INTERFACE_DOWN );
			set_ifconfig( IFNAME, INTERFACE_DOWN );
#if 0
	system( cmd1 );
#else
	system_cmd( cmd1 );
#endif
			sys_sleep( 100 );
			set_ifconfig( IFNAME, INTERFACE_UP );
			set_ifconfig( IFNAME, INTERFACE_UP );

			//printf("changed  mac orgmac : %s , new mac : %s..\n", porghwaddr, ptrhwaddr );
		}
	}

	return ERR_RETURN;
}

long long get_tickcount_us( void )
{
    struct timespec now; 
	long long tick=0;

	clock_gettime(CLOCK_MONOTONIC, &now);
    tick =  now.tv_sec*1000000 + now.tv_nsec/1000;

	/* return micro sec */
	return tick;
}

unsigned int get_realtickcount( void )
{
	UINT32 tick = 0;
	struct timespec now;

	/* overflow of about 49day */
	clock_gettime(CLOCK_MONOTONIC, &now);
	tick =  now.tv_sec*1000 + now.tv_nsec/1000000;

	/* return milesec */

	return tick;
}

unsigned int get_sectick( void )
{
	UINT32 tick = 0;
	struct timespec now;

	/* overflow of about 136 year */
	clock_gettime(CLOCK_MONOTONIC, &now);
	tick =  now.tv_sec;

	/* return milesec */

	return tick;

}

unsigned int get_nowtickcount( struct timespec *now )
{
	clock_gettime(CLOCK_REALTIME, now);
	return ERR_SUCCESS;
}

void sys_sleep( int msec )
{
#if 1
	struct timespec timeout;

	timeout.tv_sec = msec / 1000;
	timeout.tv_nsec = (msec % 1000) * (1000 * 1000);

	pselect( 0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &timeout, &s_sigmask );   
#else
	// this is not safe for thread.

	struct timespec timeout0;
	struct timespec timeout1;
	struct timespec* tmp;
	struct timespec* t0 = &timeout0;
	struct timespec* t1 = &timeout1;

	t0->tv_sec = msec / 1000;
	t0->tv_nsec = (msec % 1000) * (1000 * 1000);

	while ((nanosleep(t0, t1) == (-1)) && (errno == EINTR))
	{
		tmp = t0;
		t0 = t1;
		t1 = tmp;
	}
#endif

}

void sys_usleep( int usec )
{
	struct timespec timeout0;
	struct timespec timeout1;
	struct timespec* tmp;
	struct timespec* t0 = &timeout0;
	struct timespec* t1 = &timeout1;

	t0->tv_sec = usec / 1000000;
	t0->tv_nsec = ( usec % 1000000 ) * ( 1000 );

	while ((nanosleep(t0, t1) == (-1)) && (errno == EINTR))
	{
		tmp = t0;
		t0 = t1;
		t1 = tmp;
	}
}


int copy_binfile( char * ptrorgpath, char * ptrdestpath )
{
	int org_fd, dest_fd ;

	int ret = ERR_SUCCESS;
	unsigned char buf[ MAX_READ_BUF_SIZE + 1 ];
	int read_size , write_size ;
	int flag = 0;

	read_size = write_size =0;

	if ( ptrorgpath == NULL || ptrdestpath == NULL )
	{
		return -1;
	}
	
	org_fd = dest_fd = 0;
	org_fd = open( ptrorgpath , O_RDONLY  );
	dest_fd = open( ptrdestpath , O_WRONLY | O_CREAT | O_TRUNC , 0755 );

	if ( org_fd < 0  || dest_fd < 0 )
	{
		//printf( "org_fd( %s, %d ), dest_fd( %s, %d )..\n", ptrorgpath, org_fd, ptrdestpath,  dest_fd );
		ret = -2;
	}

	if ( ret == -2 )
	{
		if ( org_fd >= 0 ) close( org_fd );
		if ( dest_fd >= 0 ) close( dest_fd );

		return ret;
	}

	while (  flag == 0  )
	{
		read_size = read( org_fd, buf, MAX_READ_BUF_SIZE );

		if ( read_size > 0 )
		{
			write_size = write ( dest_fd, buf, read_size );
			if ( read_size != write_size )
			{
				ret = -3;
				break;
			}
		}
		else
		{
			flag = -1;
		}
	}
	
	if ( org_fd >=0 ) close( org_fd );
	if ( dest_fd >= 0 ) close( dest_fd );

	return ret;
}

int save_binfile_append( char * ptrpath, unsigned char * ptrdata, unsigned int size )
{
	FILE * fp = NULL;
	unsigned int write_size =0;
	int result = ERR_SUCCESS;
	
	if ( ptrpath == NULL || ptrdata == NULL || size <= 0 )
		return ERR_RETURN;

	fp = fopen( ptrpath, "ab+");
	if ( fp == NULL )
	{
		result = errno;
		//print_dbg(DBG_ERR,1, "File Open Err:%d===========",result );
		/* error read only file system */
		if ( result == 30 )
		{
			return -3 ;
		}
		else
			return -1;
	}

	//print_buf( size, ptrdata );
	write_size = fwrite( ptrdata, sizeof( char ), size, fp );
	if ( write_size != size )
	{
		printf("err save bin file( org size( %u ), write_size( %u )..\n", size, write_size );
		result = ERR_RETURN;
	}
	
	fflush( fp );

	if ( fp != NULL ) 
	{
		fclose( fp );
	}

	sys_sleep( 10 );

	return result;

}


int save_binfile( char * ptrpath, unsigned char * ptrdata, unsigned int size )
{
	FILE * fp = NULL;
	unsigned int write_size =0;
	int result = ERR_SUCCESS;

	if ( ptrpath == NULL || ptrdata == NULL || size <= 0 )
		return ERR_RETURN;

	fp = fopen( ptrpath, "w+b");
	if ( fp == NULL )
	{
		print_dbg(DBG_ERR, 1, "Err of fp open..errno:%s %d\n", ptrpath , errno );
		return ERR_RETURN;
	}

	//print_buf( size, ptrdata );
	write_size = fwrite( ptrdata, sizeof( char ), size, fp );
	if ( write_size != size )
	{
		print_dbg(DBG_ERR, 1, "The saved file size is different ( org size( %u ), write_size( %u )..\n", size, write_size );
		result = ERR_RETURN;
	}
	
	fflush( fp );
	if ( fp != NULL ) fclose( fp );

	sys_sleep( 10 );
	return result;
}

int read_binfile( char * ptrpath, unsigned char * ptrdata, unsigned int size )
{
	FILE * fp = NULL;
	unsigned int read_size =0;
	unsigned int total_size =0;
	//int result = ERR_SUCCESS;
	
	if ( ptrpath == NULL || ptrdata == NULL )
		return ERR_RETURN;

	fp = fopen( ptrpath, "rb");
	if ( fp == NULL )
	{
		//printf("err fp open..%s\n", ptrpath );
		return ERR_RETURN;
	}

	while( feof( fp ) ==  0 )
	{
		read_size = fread( ptrdata, 1, 1024, fp );
		total_size += read_size;
		ptrdata += read_size;
	}

	/*
	if ( total_size != size )
	{
		printf("not same read size read size file size( %u ), read_size ( %u )\n", total_size, size  );
		result = ERR_RETURN;
	}
	*/

	if ( fp != NULL ) fclose( fp );
	return total_size;

}

int isexist_file( char * ptrpath )
{
	FILE * ptrfp = NULL;

	if ( ptrpath == NULL )
		return 0;

	ptrfp = fopen( ptrpath ,  "r" );
	if( ptrfp == NULL )
		return 0;

	fclose( ptrfp );

	return 1;

}

int delete_file( char * ptrpath )
{
	FILE * ptrfp = NULL;

	if ( ptrpath == NULL )
		return ERR_RETURN;

	ptrfp = fopen( ptrpath ,  "r" );
	if( ptrfp == NULL )
		return ERR_RETURN;

	fclose( ptrfp );

	unlink( ptrpath );
	sys_sleep(2);

	return ERR_SUCCESS;
}

int logger_reset( char * ptrdesp )
{
	char sztemp[ MAX_PATH_SIZE ];

	//com_ioctl_get_reset_func( 1 );

	memset( sztemp, 0, sizeof( sztemp ));
#if 0
	if ( ptrdesp != NULL )
	{
		sprintf( sztemp,"LOGGER will be Reset. Because of %s Command", ptrdesp );
	}
	else
	{
		sprintf( sztemp,"LOGGER will be Reset. Becaseu of ETC Command" );
	}
#endif
	print_dbg( DBG_INFO, 1, sztemp );

	sys_sleep( 200 );
	system_cmd( "sync" );
	sys_sleep( 200 );
	system_cmd( "reboot" );
	sys_sleep( 300 );

	return ERR_SUCCESS;
}

#define BOOTCOUNT_WRITE  0
#define BOOTCOUNT_READ  1

int get_cur_time( time_t t_time  , char * ptrbuf , char * ptrbuf2 )
{
    time_t cur;
    struct tm tm;

	if ( t_time == 0 )
    	cur = time( NULL );
	else
		cur = t_time;
#if 0
	cur += ( 600 * 6 * 9 ); /* add 9 hour */
#endif

	memset( &tm, 0, sizeof( struct tm ));
    localtime_r( &cur, &tm );
#if 1
	if ( ptrbuf != NULL )
	{
		sprintf( ptrbuf,  "%04d-%02d-%02d %02d:%02d:%02d",
					tm.tm_year + 1900,tm.tm_mon + 1,tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	}

	if ( ptrbuf2 != NULL )
	{
		sprintf( ptrbuf2,  "%04d%02d%02d%02d%02d%02d",
					tm.tm_year + 1900,tm.tm_mon + 1,tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	}

#else
	
	strftime(ptrbuf, 50, "%a %Y-%m-%d %H:%M:%S %Z", tm);
#endif
	return ERR_SUCCESS;
}

int get_cur_day( time_t t_time , char * ptrbuf, char * ptrbuf2 )
{
    time_t cur;
    struct tm tm;

	if ( t_time == 0 )
    	cur = time( NULL );
	else
		cur = t_time;
#if 0
	cur += ( 600 * 6 * 9 ); /* add 9 hour */
#endif
	memset( &tm, 0, sizeof( struct tm ));
    localtime_r( &cur, &tm );
#if 1
	if ( ptrbuf != NULL )
	{
		sprintf( ptrbuf,  "%04d-%02d-%02d", tm.tm_year + 1900,tm.tm_mon + 1,tm.tm_mday);
	}

	if ( ptrbuf2 != NULL )
	{
		sprintf( ptrbuf2,  "%04d%02d%02d_%02d", tm.tm_year + 1900,tm.tm_mon + 1,tm.tm_mday, tm.tm_hour );
	}
#else
	strftime(ptrbuf, 50, "%a %Y-%m-%d %H:%M:%S %Z", tm);
#endif
	return ERR_SUCCESS;
}

int get_cur_day_total( time_t t_time , char * ptrbuf, char * ptrbuf2 )
{
    time_t cur;
    struct tm tm;

	if ( t_time == 0 )
    	cur = time( NULL );
	else
		cur = t_time;
#if 0
	cur += ( 600 * 6 * 9 ); /* add 9 hour */
#endif
	memset( &tm, 0, sizeof( struct tm ));
    localtime_r( &cur, &tm );
#if 1
	if ( ptrbuf != NULL )
	{
		sprintf( ptrbuf,  "%04d-%02d-%02d", tm.tm_year + 1900,tm.tm_mon + 1,tm.tm_mday);
	}

	if ( ptrbuf2 != NULL )
	{
		sprintf( ptrbuf2,  "%04d%02d%02d_%02d%02d%02d", 
				tm.tm_year + 1900,tm.tm_mon + 1,tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec );
	}
#else
	strftime(ptrbuf, 50, "%a %Y-%m-%d %H:%M:%S %Z", tm);
#endif
	return ERR_SUCCESS;
}

int init_session_conf_file( const char *ptrfilename, const char *ptrdelim, session_default_value_t tbl[] )
{
	conf_t *conf = NULL ;
	int i;
	char *val;
	int flag = 0;

	conf = load_conf( ptrfilename, ptrdelim );
	if( !conf )
	{
		//printf("Cannot load %s...\n", ptrfilename );
		return ERR_RETURN ;
	}

	for( i = 0; tbl[i].key != NULL; i++ )
	{
		val = conf_session_get_value( conf, tbl[ i ].session, tbl[i].key );
		if( !val )
		{
			flag = 2;
			//printf("Init Session Conf : file[ %s ], session[ %s ] , key[ %s ], val[ %s ]....\n",ptrfilename,  tbl[ i ].session, tbl[i].key, tbl[i].val  );
			conf_session_set_value( conf, tbl[ i ].session, tbl[i].key, tbl[i].val );
		}
	}

	if ( flag == 2 )
	{
		save_release_conf( conf );
	}
	else
	{
		release_conf( conf );
	}

	return flag;

}

static int isexceptchar( char ch )
{

	if ( ( ch != 0x20  ) && ( ch != 0x21  ) && ( ch !=  0x23  ) && ( ch != 0x24  ) && ( ch != 0x25 ) && ( ch != 0x26 ) && 
		 ( ch != 0x2A  ) && ( ch != 0x2B  ) && ( ch != 0x2D ) && ( ch != 0x2E ) && ( ch != 0x2F )   &&
		 ( ch != 0x3D  ) && ( ch != 0x3F  ) && ( ch !=  0x40 ) && ( ch != 0x0A ) )
	{
		return ERR_RETURN;
	}

	return 1;
}

/* mortem =================================================================================================*/

static MOUNTP *dfopen()
{
	MOUNTP *MP;

	MP = (MOUNTP *)malloc(sizeof(MOUNTP));
	if(!(MP->fp = fopen(MMOUNT, "r")))
	{
		return NULL;
	}
	else
		return MP;
}

static int dfclose(MOUNTP *MP)
{
	if ( MP == NULL )
		return 0;

	if ( MP->fp != NULL )
		fclose(MP->fp);

	free( MP );

	return 0;
}

static MOUNTP *dfget(MOUNTP *MP)
{
	char buf[256];
	struct statfs lstatfs;
	struct stat lstat;
	int is_root = 0;

	while(fgets(buf, 255, MP->fp))
	{
		is_root = 0;
		sscanf(buf, "%s%s%s",MP->devname, MP->mountdir, MP->fstype);
		if (strcmp(MP->mountdir,"/") == 0) is_root=1;
		if ( (stat(MP->devname, &lstat) == 0) || is_root)
		{
			if ( ( strstr( buf, MP->mountdir) && S_ISBLK(lstat.st_mode) ) || is_root)
			{
				statfs(MP->mountdir, &lstatfs);
				MP->size.blocks = lstatfs.f_blocks * (lstatfs.f_bsize/1024);//KB 단위
				MP->size.avail  = lstatfs.f_bavail * (lstatfs.f_bsize/1024);//KB 단위
				return MP;
			}
		}
	}
	rewind(MP->fp);
	return NULL;
}
int get_status_mnt( char * ptrpath, int * ptrtotal, int * ptrused, int * ptrfree )
{
	unsigned long used=0;
	MOUNTP *MP = NULL;
	int val= -1 ;
	int len = 0;
	//char * path = "/mnt/CONFIG";
	char szpath[ MAX_PATH_SIZE ];

	if ( ptrpath == NULL || (MP=dfopen()) == NULL )
	{
		perror("error");
		*ptrfree = -1;
		return val;
	}

	if ( ptrtotal == NULL || ptrused == NULL || ptrfree == NULL )
	{
		return val;
	}

	memset( szpath, 0, sizeof( szpath ));
	memcpy( szpath, ptrpath, strlen( ptrpath ));

	len = strlen( szpath );
	if ( szpath[ len  -1 ] == 0x2f )
	{
		szpath[ len -1 ] = 0x00;
	}

	while(dfget(MP))
	{
		if( strcmp( szpath ,MP->devname)==0 || strcmp( szpath,MP->mountdir)==0)
		{
			used = (MP->size.blocks-MP->size.avail);
			//printf("%-14s%-20s%10luKB%10luKB\t%ld\t%d%%\r\n", MP->mountdir, MP->devname,
			//              MP->size.blocks,
			//              MP->size.avail,used,used/(MP->size.blocks/100) );

			*ptrused = used;
			*ptrtotal = MP->size.blocks;
			*ptrfree  = 100 - ( ( (float)*ptrused / *ptrtotal ) * 100 );
		}
		//printf("===================%s, =%s...%s.\r\n",ptrpath, MP->devname,MP->mountdir );
	}

	if ( MP != NULL ) dfclose( MP );
	return val;
}
#define MEM_SIZE	65536

int get_free_ram( int * per )
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	char find = 0;

	char seps[] = "()|{}, \t.;&-[]\"\':`/";
	char *tr=NULL;

	char *type = "MemFree";
	char value[ 50 ]; 
	int val = 0;
	int used =0;

	fp = fopen("/proc/meminfo", "r");
	if (fp == NULL)
		return val ;

	while ((read = getline(&line, &len, fp)) > 0 )
	{
		tr = strtok(line, seps);
		while (tr != NULL && find==0)
		{
			//printf("token: %s\r\n", tr);
			if(strcmp(tr,type)==0)
			{
				tr = strtok(NULL, seps);
				if ( tr != NULL )
				{
					//printf("token2: %s\r\n", tr);
					memset( value, 0, sizeof( value ));
					strcpy(value,tr);
					val = atoi( value );
					used = MEM_SIZE - val;

					if ( per != NULL )
					{
						*per = 100 - (int)( used /( MEM_SIZE / 100 ));
					}
					find = 1;
					break;
				}
			}

			tr = strtok(NULL, seps);
		}
		if(find)
			break;
	}

	free(line);
	fclose(fp);

	return val;
}

int get_daemon_status( char* ptrservice )
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int find = 1;
	char szcmd[ 100 ];

	char seps[] = "()|{}, \t.;&-[]\"\':`/*0";
	char *tr=NULL;

	memset( szcmd, 0, sizeof( szcmd ));
	sprintf( szcmd, "netstat -lt | grep %s > /tmp/netstatinfo",ptrservice);
#if 0
	system( szcmd );
#else
	system_cmd( szcmd );
#endif

	fp = fopen("/tmp/netstatinfo", "r");
	if (fp == NULL)
	{
		return 1;
	}

	while ((read = getline(&line, &len, fp)) > 0)
	{
		find = -1;

		tr = strtok(line, seps);
		while (tr != NULL && find < 0)
		{   
			//printf("token: %s\r\n", tr);
			if(strcmp(tr,ptrservice)==0)
			{   
				find = 1;
				break;
			}   
			tr = strtok(NULL, seps);
		}   

		if(find == 1 )
			break;
	}   

	free(line);
	fclose(fp);

	return find;
}

int get_driver_status( char* ptrname)
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int find = -1;

	char seps[] = "()|{}, \t.;&-[]\"\':`/\n";
	char *tr=NULL;

	fp = fopen("/proc/misc", "r");
	if (fp == NULL)
		return -1; 

	while ((read = getline(&line, &len, fp)) >0 ) 
	{   
		tr = strtok(line, seps);
		while (tr != NULL)
		{   
			//printf("token: %s\r\n", tr);
			if(strcmp(tr,ptrname)==0)
			{   
				find = 1;
				break;
			}   
			tr = strtok(NULL, seps);
		}   
		if(find == 1 )
			break;
	}   
	free(line);
	fclose(fp);

	return find;
}

int isvalidchar( int size, char * ptrdata )
{
	int i;
	int result = 1;
	char * ptr;

	ptr = ptrdata;

	for( i = 0 ; i < strlen( ptrdata ) ; i++ , ptr++ )
	{
		if (  *ptr == 0x0A )
		{
			break;
		}
		else
		{
			if ( !isalnum( *ptr )  )
			{
				if ( isexceptchar( *ptr ) != 1 )
				 {
					result = 0;
					break;
				 }
			}
		}
	}


	return result;
}

int system_cmd( char * ptrcmd )
{
	int ret = ERR_SUCCESS;
	if ( ptrcmd == NULL )
		return ERR_RETURN;
#if 1
	ret = system( ptrcmd );

	if ( 127 == ret || -1 == ret )
	{
		print_dbg( DBG_ERR, 1, "Fail to sytem command [%s,%d]", ptrcmd, ret ); 
	}

	if ( WIFSIGNALED(ret) && ( WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT ) ) 
	{   
		print_dbg( DBG_ERR, 1, "Fail to sytem command [%s,%d]", ptrcmd, ret ); 
	}
#else
    int run = 1;
    while ( run ) 
    {
	    ret = system( ptrcmd );

        if (WIFSIGNALED(ret) && (WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
        {
            run = 0;
            break;
        }
	    sys_sleep( 10 );
    }
#endif
	sys_sleep( 10 );
	ret = ERR_SUCCESS;
	
	return ret;
}

void print_line( void )
{
	printf("\n========================================================================\n");
}

void print_box( char * ptrdata ,... )
{
	char ptrbuf[ MAX_PRINT_SIZE ] ;
	char sztemp[ 220 ];

	va_list args;
	int size = 0;

	if ( ptrdata != NULL )
	{
		memset(ptrbuf, 0, sizeof( ptrbuf ));
		memset( sztemp , 0, sizeof( sztemp ));

		va_start(args, ptrdata );
		vsprintf(ptrbuf, ptrdata , args);
		va_end(args);

		if ( ptrbuf != NULL )
			size = strlen( ptrbuf );

		if ( size  > 200)
		{
			ptrbuf[ 198 ] = 0x00;
			ptrbuf[ 199 ] = 0x00;
		}

		if ( ptrbuf != NULL )
			strcat( sztemp, ptrbuf );

		strcat( sztemp, "\r\n");
		print_line();
		printf( "%s", sztemp );
		print_line();
	}
}

void print_buf( int size, unsigned char * ptrdata )
{
	int i;

	print_line();
	for( i = 0 ; i < size ; i++ )
	{
		printf("%02x ", ptrdata[ i ] & 0xFF);

		if ( ( i != 0 ) && ( i % 100 == 0 ))
		{
			printf("\n");
		}
	}
	print_line();
}

static void print_debug( int size, char * ptrdata, int print_consol, BYTE debug_type )
{
	if ( ptrdata != NULL  && print_consol == 1 )
	{
		printf( "%s", ptrdata );
		if ( g_send_debug_func != NULL )
		{
			g_send_debug_func( ptrdata, size );
		}

		if ( debug_type == DBG_ERR || debug_type == DBG_SAVE )
		{
			write_err( ptrdata, size );
		}
	}
}

int print_dbg( unsigned char debug_type, int print_consol, char * fmt, ...)
{
	va_list args;
	char ptrbuf[ MAX_PRINT_SIZE ];
	char sztemp[ 220 ];
	int size = 0;

	memset( sztemp, 0, sizeof( sztemp ));
	
	if ( debug_type == DBG_INIT )
	{
		sprintf( sztemp, "%s", "[Init] ");
	}
	else if ( debug_type  == DBG_INFO )
	{
		sprintf( sztemp, "%s", "[Info] ");
	}
	else if ( debug_type == DBG_ERR )
	{
		sprintf( sztemp, "%s", "[Error] ");
	}
	else if ( debug_type == DBG_SAVE  )
	{
		sprintf( sztemp, "%s", "[Save] ");
	}
	else if ( debug_type == DBG_LINE )
	{
		sprintf( sztemp, "%s", "========================================================================");
	}
	else if ( debug_type == DBG_NONE )
	{
		sprintf( sztemp, "%s", STR_SPACE);
	}
	else
	{
		sprintf( sztemp, "%s", "[***] ");
	}

	if ( fmt != NULL )
	{
		memset(ptrbuf, 0, sizeof( ptrbuf ));
		va_start(args, fmt);
		vsprintf(ptrbuf, fmt, args);
		va_end(args);
		if ( ptrbuf != NULL )
			size = strlen( ptrbuf );

		if ( size  > 200)
		{
			ptrbuf[ 198 ] = 0x00;
			ptrbuf[ 199 ] = 0x00;
		}

		if ( ptrbuf != NULL )
			strcat( sztemp, ptrbuf );

		strcat( sztemp, "\r\n");
		print_debug( strlen( sztemp ), sztemp, print_consol, debug_type );
	}
	else
	{
		strcat( sztemp, "\r\n");
		print_debug( strlen( sztemp ), sztemp, print_consol, debug_type );
	}

	return ERR_SUCCESS;
}

void print_dbg_buf( int size, unsigned char * ptrdata )
{
	int i;
	char sztemp[ 10 ];
	int cnt =0;

	for( i = 0 ; i < size ; i++ )
	{
		cnt++;
		memset( sztemp, 0, sizeof( sztemp ));
		sprintf( sztemp, "%02x ", ptrdata[ i ] & 0xFF );
		
		print_debug( strlen( sztemp ) ,sztemp , 1 , DBG_NONE );

		if ( ( i != 0 ) && ( cnt % 16 == 0 ))
		{
			memset( sztemp, 0, sizeof( sztemp ));
			sprintf(sztemp, "%s", "\r\n");
			print_debug( strlen( sztemp ) ,sztemp, 1, DBG_NONE );
		}
	}

	print_dbg( DBG_NONE, 1, "\r\n");
	print_dbg( DBG_LINE, 1, NULL );
}

int get_reset_type( void )
{
	int ret = -1;
#if 0
	int sel = 0;
	int fd = -1;

	fd = open( LOGGER_DEV_PIO , O_RDWR);
	if ( fd >=0 )
	{
		ioctl(fd, IOCTL_GPIO_RESET_SR, (unsigned long)&sel); 
		ret = sel;

		if(sel == 0)
			printf("Reset : Genenral\rn");
		else if(sel== 1)
			printf("Reset : Wakeup \rn");
		else if(sel== 2)
			printf("Reset : Wachdog \rn");
		else if(sel== 3)
			printf("Reset : software \rn");
		else if(sel== 4)
			printf("Reset : Reset Switch \rn");

	}

	if ( fd >=0 ) close( fd );
#endif

	return ret;
}

int detect_directory( char * ptrpath )
{
	DIR *dir_info = NULL;
	int detect = 0;

	if( ptrpath == NULL )
	{
		print_dbg(DBG_INFO,1,"isnull detect dir");
		return 0;
	}

	dir_info = opendir( ptrpath );
	if ( NULL != dir_info)
	{    
		detect =1;
		closedir( dir_info);
	}
	//print_dbg( DBG_INFO, 1, "path:%s, det:%d", ptrpath, detect );


	return detect;
}

int save_log_dat( char * ptrpath, char * ptrdata )
{
	FILE * fp = NULL;
	unsigned int write_size =0;
	int result = ERR_SUCCESS;
	int len = 0;

	if ( ptrpath  == NULL || ptrdata == NULL )
	{
		return ERR_RETURN ;
	}

	len = strlen( ptrdata );

	fp = fopen( ptrpath , "a+");
	if ( fp == NULL )
	{
		result = errno;
		//print_dbg(DBG_ERR,1, "File Open Err:%d===========",result );
		/* error read only file system */
		if ( result == 30 )
		{
			return -3 ;
		}
		else
			return -1;
	}

	//print_buf( size, ptrdata );
	write_size = fwrite( ptrdata, sizeof( char ), len, fp );
	if ( write_size != len)
	{
		printf("err %s: file( org size( %u ), write_size( %u )..\n", ptrpath, len, write_size );
		result = ERR_RETURN;
	}

	fflush( fp );

	if ( fp != NULL ) 
	{
		fclose( fp );
	}

	return result;

}

int cnvt_fclt_tid( tid_t * ptrtid )
{
	time_t cur;
	struct tm tm;

	if ( ptrtid == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrid is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	cur = ptrtid->cur_time;

	memset( &tm, 0, sizeof( struct tm ));
	localtime_r( &cur, &tm );
	// 년도(4) + 날짜(4) + 시간(초)(6) + 국소ID(4) + SNSR_TYPE(2) + 시퀀스(4)
	sprintf( ptrtid->szdata,  "%04d%02d%02d%02d%02d%02d%04d%02d%04d",
			tm.tm_year + 1900,
			tm.tm_mon + 1,
			tm.tm_mday, 
			tm.tm_hour, 
			tm.tm_min, 
			tm.tm_sec,
			ptrtid->siteid,
			ptrtid->sens_type,
			ptrtid->seq );

	return ERR_SUCCESS;
}

int get_stringtover( char * ptrstr, unsigned char * ptrver )   
{                           
	char * t, temp[ 100 ];
	int t_len, total_len;

	total_len = 0;

	if ( ptrstr == NULL )
	{   
		ptrver[ 0 ] =0;
		ptrver[ 1 ] =0; 
	}   
	else
	{   
		total_len = strlen( ptrstr );
		/* t = (,1) */
		t = strstr( ptrstr , "." );

		if ( t == NULL || total_len == 0 )
		{   
			ptrver[ 0 ] =0;
			ptrver[ 1 ] =0;
		}
		else
		{   
			t_len = strlen( t );
			memset( temp, 0, sizeof( temp ));
			memcpy( temp, ptrstr , ( total_len - t_len) );
			ptrver[ 0 ] = ( unsigned char )atoi( temp );

			memset( temp, 0, sizeof( temp ));
			memcpy( temp, t +1 , ( t_len - 1 )); 
			ptrver[ 1 ] = ( unsigned char )atoi( temp );
		}
	}    
	return ERR_SUCCESS;
}

int cnv_addr_to_strip( unsigned int s_addr, char * ptrip )
{
	struct sockaddr_in addr;
	char * ptr = NULL;

	if ( ptrip == NULL )
	{
		print_dbg( DBG_ERR,1, "Isnull Data ptrip");
		return ERR_RETURN;
	}
	
	addr.sin_addr.s_addr = ( s_addr );
	ptr = inet_ntoa( addr.sin_addr );
	
	if ( ptr != NULL )
	{
		memcpy( ptrip, ptr, strlen( ptr ));
	}

	return ERR_SUCCESS;
}

float read_file( char * ptrpath )
{
	FILE *fp = NULL;
	char line[ 100 ] ;
	float fval = -1;
	int i, len = 0;

	if ( ptrpath == NULL )
	{
		print_dbg( DBG_ERR,1, "Is null read_file name");
		return ERR_SUCCESS;
	}
	
	memset( line, 0, sizeof( line ));

	fp = fopen( ptrpath, "r");
	if (fp == NULL)
		return ERR_RETURN ;

	fgets( line, 100, fp );
	len = strlen( line );

	/* Replace % -> NULL */
	for( i = 0 ; i < len ; i++ )
	{
		if ( line[ i ] == 0x25 )
			line[ i ] = 0x00;
	}
	
	//printf("-----------------------------lien:%s......................\r\n", line );
	fval = atof( line );
	if ( fval == 0.0 )
		fval = 0.1;
	//printf("-----------------------------fval:%f......................\r\n", fval );

	if ( fp != NULL )
	{
		fclose(fp);
	}

	return fval;
}
