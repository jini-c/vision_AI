/*********************************************************************
	file : apc_ups.c
	description :
	product by tory45( tory45@empal.com) ( 2016.08.05 )
	update by ?? ( . . . )

 ********************************************************************/
#include "com_include.h"
#include "apc_ups.h"
#include "control.h"

#define MAX_APC_DATA_SIZE 700	
#define MAX_APC_TCP_RECV_SIZE   ( MAX_APC_DATA_SIZE )	
#define MIN_APC_TCP_QUEUE_SIZE	10
#define MAX_APC_TCP_QUEUE_SIZE	( MAX_APC_TCP_RECV_SIZE * 5 )
#define MAX_APC_SEND_BUF_SIZE	300

#define T_1 1
#define T_2	2
#define T_3 3
#define T_4 4

typedef struct mbap_head_
{
	unsigned short t_id; 		/* Transaction ID */
	unsigned short p_id;		/* Protocol ID */
	unsigned short len;			/* Length  */
	BYTE 		   u_id;		/* Unit ID */
	BYTE * ptrdata;
}__attribute__ ((packed)) mbap_head_t;

typedef struct apc_queue_
{
	volatile int head;
	char * ptrlist_buf;
	char * ptrrecv_buf;
}__attribute__ ((packed)) apc_queue_t; 

typedef struct apc_ups_info_
{
	fclt_info_t fclt_info;
	volatile int fd;
	apc_queue_t queue;
}apc_ups_info_t;

typedef struct apc_packet_info_
{
	volatile unsigned short t_id;
	volatile unsigned short addr;
	volatile unsigned short reg_cnt;
	volatile int snsr_sub_type;
	volatile int last_val;
}apc_packet_info_t;

typedef struct apc_packet_mng_
{
	volatile int next_step;
	volatile int last_step;
	volatile UINT32 last_recv_time;
	apc_packet_info_t list[ MAX_APC_UPS_DATA_CNT ];
}apc_packet_mng_t;

/*================================================================================================
  전역 변수 
  ================================================================================================*/
static send_data_func  g_ptrsend_apc_ups_data_control_func = NULL;		/* CONTROL 모듈에 설정,제어 정보를 넘기는 함수 포인트 */

apc_ups_info_t g_apc_ups_info;
apc_packet_mng_t g_apc_packet_mng;

static volatile int g_release				= 0;

/*================================================================================================
  내부 함수선언
  ================================================================================================*/
static int parse_apc_data( int size, apc_queue_t * ptrapc_queue, proc_func ptrparse_func, mbap_head_t * ptrhead );
static int check_apc_ups_connect_sts( int sts );
static int close_tcp( void );


/* thread  */
static volatile int g_start_apc_ups_pthread 	= 0; 
static volatile int g_end_apc_ups_pthread 		= 1; 
static pthread_t apc_ups_pthread;
static void * apc_ups_pthread_idle( void * ptrdata );

static int send_data_control( UINT16 msg, void * ptrdata );

static int internal_init( void );
static int internal_idle( void * ptr );
static int internal_release( void );

static int parse_ups_low_input( UINT16 t_id, void * ptrdata );
static int parse_ups_bat_sts( UINT16 t_id, void * ptrdata );
static int parse_ups_bat_remain( UINT16 t_id, void * ptrdata );

static int recv_allim_msg(unsigned short inter_msg, void * ptrdata );
/*================================================================================================
  내부 함수  정의 
  ================================================================================================*/
static proc_func_t g_proc_func_list []= 
{
	/*=============================================*/
	{ EFT_ALLIM_MSG	,       recv_allim_msg },

	{ 0, NULL }
};

static proc_func_t g_parse_apc_ups_func_list[ ] = 
{
	{ T_1, 	parse_ups_low_input },
	{ T_2, 	parse_ups_bat_sts },
	{ T_3, 	parse_ups_bat_remain },
	{ 0	, 	NULL }
};

static int reload_apc_ups( void )
{
	fclt_info_t fclt;

	memset( &fclt, 0, sizeof( fclt ));
	if ( get_apc_ups_fclt_info_ctrl( &fclt ) == ERR_SUCCESS )
	{
		memcpy( &g_apc_ups_info.fclt_info, &fclt , sizeof( fclt ));
		g_apc_ups_info.fd = -1;
		print_dbg( DBG_INFO, 1, "GET Reload APC UPS FcltID :%d, FcltCode:%d", g_apc_ups_info.fclt_info.fcltid, g_apc_ups_info.fclt_info.fcltcode );
	}

	return ERR_SUCCESS;
}

static int recv_allim_msg(unsigned short inter_msg, void * ptrdata )
{
	inter_msg_t * ptrmsg =NULL;
	allim_msg_t allim_msg ;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}
	
	memset( &allim_msg, 0, sizeof( allim_msg ));
	ptrmsg = ( inter_msg_t *)ptrdata;

	if ( ptrmsg->ptrbudy == NULL )
	{
		print_dbg( DBG_ERR,1, "is budy [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	memcpy( &allim_msg, ptrmsg->ptrbudy, sizeof( allim_msg ));
	
	/* change of network */
	if ( allim_msg.allim_msg  == ALM_UPS_NET_CHG_NOTI_MSG  )
	{
		print_dbg( DBG_INFO,1, "Recv AllimMsge UPS NET CHG");
		close_tcp( );
		reload_apc_ups( );
	}
	/* add of network */
	else if ( allim_msg.allim_msg  == ALM_UPS_NET_ADD_NOTI_MSG  )
	{
		print_dbg( DBG_INFO,1, "Recv AllimMsge UPS NET ADD");
		close_tcp( );
		reload_apc_ups( );
	}
	/* del of network */
	else if ( allim_msg.allim_msg  == ALM_UPS_NET_DEL_NOTI_MSG  )
	{
		print_dbg( DBG_INFO,1, "Recv AllimMsge UPS NET DEL");
		close_tcp( );
		reload_apc_ups( );
	}

	return ERR_SUCCESS;
}
/* CONTROL module에  보낼 데이터(제어, 설정)가  있을 경우 호출 */
static int send_data_control( UINT16 msg, void * ptrdata  )
{
	if ( g_ptrsend_apc_ups_data_control_func != NULL )
	{
		return g_ptrsend_apc_ups_data_control_func( msg, ptrdata );
	}
	return ERR_RETURN;
}

/* CONTROL Module로 부터 수집 데이터를 받는 내부 함수 */
static int internal_recv_ctrl_data( unsigned short msg, void * ptrdata )
{

	int i;
	int proc_func_cnt = 0;
	
	proc_func  ptrtemp_proc_func = NULL;
	UINT16 list_msg = 0;

	if( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* proc function list에 등록된 개수 파악 */
	proc_func_cnt= ( sizeof( g_proc_func_list ) / sizeof( proc_func_t ));

	/* 등록된 proc function에서 inter_msg가 동일한 function 찾기 */
	for( i = 0 ; i < proc_func_cnt ; i++ )
	{    
		list_msg 			= g_proc_func_list[ i ].inter_msg; 	/* function list에 등록된 msg id */
		ptrtemp_proc_func 	= g_proc_func_list[ i ].ptrproc;	/* function list에 등록된 function point */

		if ( ptrtemp_proc_func != NULL && list_msg == msg )
		{    
			ptrtemp_proc_func( msg, ptrdata );
			break;
		}    
	} 

	return ERR_SUCCESS;
}


static int set_tcp_fd( void )
{
	int s = -1, n;
	int	so_reuseaddr;
	int so_keepalive;
	struct linger so_linger;
	struct sockaddr_in serv_addr;
	apc_ups_info_t * ptrups = NULL;

	ptrups = &g_apc_ups_info;

	if( ptrups == NULL )
	{
		print_dbg( DBG_ERR, 1, "Invalid APC UPSinfo [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	s = socket( PF_INET, SOCK_STREAM, 0 );
	if( s < 0 )
	{
		print_dbg( DBG_ERR, 1, "Cannot Create TCP Socket:IP:%s,Port:%d", ptrups->fclt_info.szip, ptrups->fclt_info.port );
		return ERR_RETURN;
	}


	memset( &serv_addr, 0, sizeof(serv_addr) );
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= inet_addr( ptrups->fclt_info.szip );
	serv_addr.sin_port			= htons( ptrups->fclt_info.port );

	/* connect timeout : 3sec */
	n = connect_with_timeout_poll( s, (struct sockaddr *)&serv_addr, sizeof( serv_addr ), TCP_TIMEOUT );
	if( n < 0 )
	{
		close(s);
		print_dbg( DBG_ERR, 1, "Cannot Connect APC UPS :IP(Timeout):%s,Port:%d",  ptrups->fclt_info.szip, ptrups->fclt_info.port );
		sys_sleep( 2000 );
		return ERR_RETURN;
	}

	so_reuseaddr 		= 1;
	so_keepalive 		= 1;
	so_linger.l_onoff	= 1;
	so_linger.l_linger	= 0;
	setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr) );
	setsockopt( s, SOL_SOCKET, SO_KEEPALIVE, &so_keepalive, sizeof(so_keepalive) );
	setsockopt( s, SOL_SOCKET, SO_LINGER,	 &so_linger,	sizeof(so_linger) );

	ptrups->fd = s;
	print_dbg( DBG_SAVE, 1, "######## Success of TCP Client :IP:%s,Port:%d, fd:%d ", ptrups->fclt_info.szip, ptrups->fclt_info.port, ptrups->fd );

	return ERR_SUCCESS;
}

static int set_tcp_connect( void )
{
	int fd = 0;

	fd = g_apc_ups_info.fd;
	
	if ( fd <=0 )
	{
		if ( set_tcp_fd( ) < 0 )
		{
			check_apc_ups_connect_sts( ESNSRCNNST_ERROR );
		}
		else
		{
			check_apc_ups_connect_sts( ESNSRCNNST_NORMAL );
		}
		sys_sleep( 100 );
	}

	return ERR_SUCCESS;
}

static int close_tcp( void )
{
	int fd = 0;

	fd = g_apc_ups_info.fd;

	if ( fd >0 )
	{
		close( fd );
		print_dbg( DBG_INFO,1,"###### APC UPS TCP Close :%d", fd );
		g_apc_ups_info.fd = -1;
		if ( g_release == 0 )
		{
			check_apc_ups_connect_sts( ESNSRCNNST_ERROR );
		}
	}

	return ERR_SUCCESS;
}

static int send_tcp_data( int len, BYTE * ptrdata )
{
	int ret = ERR_SUCCESS;
	int size = 0;
	BYTE *ptrtemp = NULL;
	int fd  = 0;

	fd = g_apc_ups_info.fd;

	if( fd < 0 || ptrdata == NULL || len <= 0 )
	{
		//print_dbg( DBG_ERR, 1, "Invalid TCP Data fcltcode:%d", fcltcode );
		return ERR_RETURN;
	}

	ptrtemp 	= ptrdata;
	size 		= len;

	ret = write( fd, ptrtemp, size );
	if( ret < 0 )
	{
		print_dbg( DBG_ERR, 1, "Can not Send TCP Data APC UPS fd:%d, size:%d, ret:%d", fd, len, ret);
		close_tcp( );
		return ERR_RETURN;
	}

	while( ret < size )
	{
		//printf("send_tcp_data... size = %d, send size = %d\n", size, ret );
		ret += write( fd, (ptrtemp + ret), size - ret );
	}


	return	 ERR_SUCCESS;
}

static int make_send_packet( apc_packet_info_t * ptrpacket, BYTE * ptrsend_buf, int * ptrsend_size )
{
	unsigned short addr;
	unsigned short reg_cnt;

	int ret = ERR_SUCCESS;
	BYTE * ptrbuf = NULL;
	mbap_head_t head;
	int head_size;
	int t_size = 0;
	int size =0;
	BYTE f_code ; 	
	
	if ( ptrpacket == NULL || ptrsend_buf == NULL || ptrsend_size == NULL )
	{
		print_dbg( DBG_ERR, 1, "Invalid send packet [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	memset(&head, 0, sizeof( head ));
	
	f_code		= 0x03; 				/* Read Holding Registers */ 
	addr 		= ptrpacket->addr;		/* Read Holding Register Address */
	reg_cnt 	= ptrpacket->reg_cnt;	/* Read Holding Register Count */

	head.t_id	= ptrpacket->t_id; 	/* Transaction ID */
	head.p_id	= 0; 				/* Protocol ID ( 0 : Modbus  ) */
	head.len	= 6;				/* U_ID Size( 1byte) + Function Code Size( 1byte ) + Data Size( 4byte ) */
	head.u_id	= 0x01;   			/* Unit ID ( 1 : Modbus Salave ID ) */

	/* convert big endian */
	addr 		= htons( addr );
	reg_cnt 	= htons( reg_cnt ); 
	head.t_id	= htons( head.t_id ); 
	head.p_id	= htons( head.p_id );
	head.len	= htons( head.len );

	head_size 	= sizeof( head ) - PNT_SIZE;
	ptrbuf 		= ptrsend_buf;

	/* head copy */
	size = head_size;
	memcpy( ptrbuf, &head, size );
	ptrbuf += size;	
	t_size += size;
	
	/* function code */
	size = sizeof( BYTE );
	*ptrbuf++ = (f_code & 0xFF );
	t_size += size;

	/* addr */
	size = sizeof( short );
	memcpy( ptrbuf, &addr, size );
	ptrbuf += size;
	t_size += size;

	/* reg count */
	size = sizeof( short );
	memcpy( ptrbuf, &reg_cnt, size );
	ptrbuf += size;
	t_size += size;

	*ptrsend_size = t_size;

	return ret;
}

static int valid_packet_apc( int size, apc_queue_t * ptrapc_queue, proc_func ptrparse_func, mbap_head_t * ptrhead   )
{
	int head =0;
	char * ptrqueue= NULL;
	short sval = 0;
	int l_size = 0;
	int head_size = 0;

	head  = ptrapc_queue->head;
	head_size = ( sizeof( mbap_head_t ) - PNT_SIZE ) ;

	if ( head < MIN_APC_TCP_QUEUE_SIZE )
	{
		//print_dbg( DBG_INFO,1,"Lack of SR data size( step 1 )" );
		return 0;
	}

	ptrqueue = (char*)&ptrapc_queue->ptrlist_buf[ 0 ] ;

	/* head :t_id*/
	l_size = sizeof( short );
	memcpy( &sval, ptrqueue, l_size);
	ptrhead->t_id= ntohs( sval ) ;
	ptrqueue += l_size;

	/* head : p_id */
	l_size = sizeof( short );
	memcpy( &sval, ptrqueue, l_size);
	ptrhead->p_id = ntohs( sval );
	ptrqueue += l_size;

	/* head : len */
	l_size = sizeof( short );
	memcpy( &sval, ptrqueue, l_size);
	ptrhead->len= ntohs( sval );
	ptrqueue += l_size;

	/* head : u_id  */
	l_size = sizeof( BYTE );
	ptrhead->u_id= *ptrqueue;
	ptrqueue += l_size;

	if ( ptrhead->len >= MAX_APC_DATA_SIZE )
	{
		print_dbg( DBG_ERR,1, "Overflow Body Size:%d, APC UPS", ptrhead->len );
		//ptrhead->data_len = MAX_TCP_RECV_SIZE;
		memset( ptrapc_queue->ptrlist_buf, 0, MAX_APC_TCP_QUEUE_SIZE );
		ptrapc_queue->head = 0;
		return 0;
	}

	/* head size + data size : 5byte , -1 : u_id size */
	if ( head < ( (ptrhead->len -1 ) + head_size ) )
	{
		//print_dbg( DBG_INFO,1,"Lack of APC UPS data size( step 2 )" );
		return 0;
	}

	if( ptrhead->ptrdata != NULL && ptrhead->len > 0 )
	{
		memset( ptrhead->ptrdata, 0, MAX_APC_DATA_SIZE );
		memcpy( ptrhead->ptrdata, ptrqueue, ( ptrhead->len  -1 ));
		//print_buf(ptrhead->len-1 ,ptrhead->ptrdata );
	}
	
	/* check varify */
	if ( ptrhead->u_id == 1 && ptrhead->ptrdata[ 0 ]== 0x03 &&  ( ptrhead->ptrdata[ 1 ]== 0x02 || ptrhead->ptrdata[ 1 ]== 0x04 ))
	{
		return 1;
	}
	else
	{
		memset( ptrapc_queue->ptrlist_buf, 0, MAX_APC_TCP_QUEUE_SIZE );
		ptrapc_queue->head = 0;
		print_dbg( DBG_ERR,1,"Invalid head protocol UPS APC Packet data :%02x %02x %02x", 
							 ptrhead->u_id,
							 ptrhead->ptrdata[ 0 ],
							 ptrhead->ptrdata[ 1 ] );
	}

	return ERR_SUCCESS;
}

static int parse_packet_apc( int size, apc_queue_t * ptrapc_queue, proc_func ptrparse_func, mbap_head_t * ptrhead )
{
	int head =0;
	int t_size, r_size = 0;
	char * ptrqueue= NULL;
	int head_size = 0;

	head        = ptrapc_queue->head;
	ptrqueue    = ( char *)&ptrapc_queue->ptrlist_buf[ 0 ] ;
	t_size = r_size =0;

	head_size = sizeof( mbap_head_t ) - PNT_SIZE;

	/* head size*/
	t_size 	+= head_size;
	ptrqueue += head_size;

	/* data size */
	t_size   += ptrhead->len -1 ;
	ptrqueue += ptrhead->len -1;

	if ( head < t_size )
	{
		memset( ptrapc_queue->ptrlist_buf, 0, MAX_APC_TCP_QUEUE_SIZE );
		ptrapc_queue->head = 0;
		print_dbg( DBG_ERR, 1,"Invalid UPS APC Packet data" );
		return -1;
	}

	r_size = head - t_size;

	if ( r_size == 0 )
	{
		memset( ptrapc_queue->ptrlist_buf, 0, MAX_APC_TCP_QUEUE_SIZE );
		ptrapc_queue->head = 0;
	}
	else
	{
		memmove( &ptrqueue[ 0 ], &ptrqueue[ t_size ], r_size );
		ptrapc_queue->head = r_size;
	}
	//print_dbg( DBG_INFO, 1," Cortex RESP ADDR:%04x", ptrsr_data->addr& 0x0000FFFF ); 

	if ( ptrparse_func != NULL )
	{
		ptrparse_func( ptrhead->t_id, ptrhead );
	}

	return r_size;
}

static int parse_apc_data( int size, apc_queue_t * ptrapc_queue, proc_func ptrparse_func, mbap_head_t * ptrhead   )
{
	int head = 0;
	char *ptrqueue = NULL;
	char *ptrrecv = NULL;
	int valid = 0;
	int remain_size = 0;

	if ( ptrapc_queue == NULL || ptrparse_func == NULL || ptrhead == NULL )
	{
		print_dbg( DBG_ERR, 1, "Isnull prase data[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrqueue    = ptrapc_queue->ptrlist_buf;
	ptrrecv     = ptrapc_queue->ptrrecv_buf;
	head        = ptrapc_queue->head;

	if ( head + size >= MAX_APC_TCP_QUEUE_SIZE )
	{
		print_dbg( DBG_ERR,1, "Overflow APC UPS QUEUE BUF Head:%d, Size:%d",head, size );
		memset( ptrqueue, 0, MAX_APC_TCP_QUEUE_SIZE );
		ptrapc_queue->head = 0;

		return ERR_RETURN;
	}

	memcpy( &ptrqueue[ head ], ptrrecv , size );
	ptrapc_queue->head += size;

	valid = valid_packet_apc( size, ptrapc_queue, ptrparse_func,  ptrhead );

	while( valid )
	{
		remain_size = parse_packet_apc( size, ptrapc_queue, ptrparse_func, ptrhead );

		if ( remain_size > 0 )
			valid = valid_packet_apc( remain_size, ptrapc_queue, ptrparse_func, ptrhead  );
		else
			valid = 0;
	}

	return ERR_SUCCESS;
}

static int set_apc_ups_val( int snsr_sub_type, int val )
{
	int i;
	int last_step;

	last_step = g_apc_packet_mng.last_step;
	
	for( i = 0 ; i <= last_step ;i++ )
	{
		if( g_apc_packet_mng.list[ i ].snsr_sub_type == snsr_sub_type )
		{
			g_apc_packet_mng.list[ i ].last_val = val;
#if 0
			print_dbg( DBG_INFO,1, "Set UPS Value  Sub type:%d.. Value:%d........",
									g_apc_packet_mng.list[ i ].snsr_sub_type,
									g_apc_packet_mng.list[ i ].last_val );
#endif
			break;
		}
	}
	
	return ERR_SUCCESS;
}

static int parse_com_data( void * ptrdata, mbap_head_t * ptrhead, UINT32 * ptrval )
{
	UINT16 sval = 0;
	UINT32 uval = 0;
	BYTE * ptr = NULL;
	int reg_cnt = 0;

	if ( ptrdata == NULL || ptrhead == NULL || ptrval == NULL )
	{
		print_dbg( DBG_ERR, 1, "is null ptrdata [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
		return ERR_RETURN;
	}
	
	memcpy( ptrhead, ptrdata, sizeof( mbap_head_t ));

	ptr = ptrhead->ptrdata;
	reg_cnt = (int)ptr[ 1 ];

	if ( reg_cnt == 2 )
	{
		memcpy( &sval, &ptr[ 2 ], sizeof( short ));
		sval = ntohs( sval );
		*ptrval = sval;
	}
	else if ( reg_cnt == 4 )
	{
		memcpy( &uval, &ptr[ 2 ], sizeof( int ));
		uval = ntohl( uval );
		*ptrval = uval;
	}
	else
	{
		print_dbg( DBG_ERR,1, "Invalid Register Count :%d[%s:%d:%s]", reg_cnt, __FILE__, __LINE__,__FUNCTION__   );
	}

	return ERR_SUCCESS;
}

static int parse_ups_low_input( UINT16 t_id, void * ptrdata )
{
	int set_val = 0;
	UINT32 sval = 0;
	mbap_head_t head;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "is null ptrdata [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
		return ERR_RETURN;
	}
	
	memset( &head, 0, sizeof( head ));
	
	if ( parse_com_data( ptrdata, &head, &sval ) != ERR_SUCCESS )
	{
		print_dbg( DBG_ERR, 1, "is null ptrdata [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
		return ERR_RETURN;
	}

	/* Set Low input Voltage : 0012 , 0  
		PowerFailure: Indicates that the input power has failed. Signal will be driven with output on or off. Complement of InputStatus.Acceptable.
	 */
	if ( sval == 0 )
		set_val = 0;
	else 
		set_val = 1;

	set_apc_ups_val( ESNSRSUB_DI_UPS_LOW_INPUT, set_val );

	if ( get_print_debug_shm()== 1 )
	{
		print_dbg( DBG_INFO,1, "Recv APC UPS Low Input Data T_ID:%d Val:%d, %d", 
								head.t_id, sval, set_val );
	}

	return ERR_SUCCESS;
}

static int parse_ups_bat_sts( UINT16 t_id, void * ptrdata )
{
	int set_val = 0;
	UINT32 sval = 0;
	mbap_head_t head;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "is null ptrdata [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
		return ERR_RETURN;
	}
	
	memset( &head, 0, sizeof( head ));
	
	if ( parse_com_data( ptrdata, &head, &sval ) != ERR_SUCCESS )
	{
		print_dbg( DBG_ERR, 1, "is null ptrdata [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
		return ERR_RETURN;
	}

	/* Set State of Bettary Charge  
		0082 : The percent state of charge in the battery. ( Divide 512 )
	 */

	sval = sval/512;
	
	set_val = sval;

	set_apc_ups_val( ESNSRSUB_AI_UPS_BATTERY_CHARGE, set_val );

	if ( get_print_debug_shm()== 1 )
	{
		print_dbg( DBG_INFO,1, "Recv APC UPS Chage of Battery Data T_ID:%d Val:%d, %d", 
								head.t_id, sval, set_val );
	}
	return ERR_SUCCESS;
}

#define ONE_MIN	 	60 
static int parse_ups_bat_remain( UINT16 t_id, void * ptrdata )
{
	int set_val = 0;
	UINT32 sval = 0;
	mbap_head_t head;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "is null ptrdata [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
		return ERR_RETURN;
	}
	
	memset( &head, 0, sizeof( head ));
	
	if ( parse_com_data( ptrdata, &head, &sval ) != ERR_SUCCESS )
	{
		print_dbg( DBG_ERR, 1, "is null ptrdata [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
		return ERR_RETURN;
	}

	/* RunTime Remaind
		0080 : The number of seconds until power will go out, when running on battery. This should
		never be compared as an actual value, but should be compared as "less than or equal
		to." Some UPS's will max out at 65535 seconds (18.2 hours).. 
	 */
	sval = sval / ONE_MIN;
	set_val = sval;

	set_apc_ups_val( ESNSRSUB_AI_UPS_BATTERY_REMAIN_TIME, set_val );

	if ( get_print_debug_shm()== 1 )
	{
		print_dbg( DBG_INFO,1, "Recv APC UPS Runtime Remaind Data T_ID:%d, Val:%d, %d", 
								head.t_id, sval, set_val );
	}

	return ERR_SUCCESS;
}

/* apc ups command 확인 */
static int parse_apc_cmd( UINT16 t_id, void * ptrdata )
{
	int i,cnt = 0;
	proc_func tmp_parse = NULL;
	int lt_id = 0;

	cnt = ( sizeof( g_parse_apc_ups_func_list ) / sizeof( proc_func_t));

	for( i = 0 ; i < cnt ; i++ )
	{
		tmp_parse 	= g_parse_apc_ups_func_list[ i ].ptrproc ;
		lt_id 		= g_parse_apc_ups_func_list[ i ].inter_msg; 

		if ( tmp_parse!= NULL && t_id == lt_id )
		{
			tmp_parse( t_id, ptrdata );
		}
	}

	return ERR_SUCCESS;	
}

static int read_apc_ups( mbap_head_t * ptrhead )
{
	int size ;
	int fd;
	char * ptrrecv= NULL;
	apc_ups_info_t * ptrapc = NULL;

	ptrapc = &g_apc_ups_info;
	if( ptrapc == NULL )
	{
		//print_dbg( DBG_ERR, 1, "Cannot read fd fcltid:%s", FDE_NAME );
		sys_sleep( 2000 );
		return ERR_RETURN;
	}

	fd = ptrapc->fd;

	if ( fd > 0 )
	{
		ptrrecv = ptrapc->queue.ptrrecv_buf;

		if ( ptrrecv != NULL )
		{
			size =0;
			memset( ptrrecv, 0, MAX_APC_TCP_RECV_SIZE );
			size = read( fd , ptrrecv, MAX_APC_TCP_RECV_SIZE  );

			if ( size > 0 )
			{
				memset( ptrhead->ptrdata, 0, MAX_APC_DATA_SIZE );
				//print_buf( size, (BYTE*)ptrrecv );
				parse_apc_data( size, &ptrapc->queue, parse_apc_cmd, ptrhead );
				g_apc_packet_mng.last_recv_time = get_sectick();
			}
			//print_buf(size, (BYTE*)ptrrecv );
			//print_dbg( DBG_INFO,1,"apc read size:%d", size );
		}
		else
		{
			print_dbg( DBG_ERR, 1, "Not have APC UPS recv buffer");
		}
	}

	return size;
}	

static void *  apc_ups_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;

	int status;
	mbap_head_t r_head;
	g_end_apc_ups_pthread = 0;
	fd_set apc_set;
	int fd ;
	int max_fd = -1;
	apc_ups_info_t * ptrapc = NULL;
	struct timeval tv;
	int n;
	int ret = 0;

	r_head.ptrdata = NULL;
	r_head.ptrdata = ( BYTE *)malloc( sizeof( BYTE ) * MAX_APC_TCP_QUEUE_SIZE );

	if( r_head.ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "Can not make head buf APC UPS");
	}

	while( g_release == 0 && g_start_apc_ups_pthread == 1 )
	{
		ptrapc = NULL;
		ptrapc = &g_apc_ups_info;

		if( ptrapc != NULL && ptrapc->fd > 0 )
		{
#if 1
			fd = ptrapc->fd;
			FD_ZERO(&apc_set);
			FD_SET(fd , &apc_set);
			max_fd = fd ;
			tv.tv_sec  = 1;	/* 1sec */
			tv.tv_usec = 0;

			n = select( max_fd + 1 , &apc_set, NULL, NULL , &tv );

			if (n == -1)/*select 함수 오류 발생 */
			{ 
				print_dbg( DBG_ERR, 1, "select function error [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
				sys_sleep( 5000 );
			}
			else if (n == 0) /* time-out에 의한 리턴 */
			{ 
				//print_dbg( DBG_NONE, 1,"===>> 시간이 초과 되었습니다 : sock %d", g_sock);
			}
			else /* 파일 디스크립터 변화에 의한 리턴 */
			{ 
				//print_dbg( DBG_NONE, 1,"===>> fms read event : sock %d", g_sock);
				if (FD_ISSET(fd, &apc_set))
				{
					if ( r_head.ptrdata != NULL )
					{
						ret = read_apc_ups( &r_head );
						if (ret  <= 0 )
						{
							FD_CLR(fd, &apc_set);
							close_tcp(  );
						}
					}
				}

			}
#else
			ret = read_apc_ups( &r_head );
#endif
		}
		else
		{
			sys_sleep( 1000 );
		}
	}

	if ( r_head.ptrdata != NULL )
	{
		free( r_head.ptrdata );
		r_head.ptrdata = NULL;
	}

	if( g_end_apc_ups_pthread == 0 )
	{
		pthread_join( apc_ups_pthread , ( void **)&status );
		g_start_apc_ups_pthread 	= 0;
		g_end_apc_ups_pthread 		= 1;
	}

	return ERR_SUCCESS;
}


static int create_apc_ups_thread( void )
{
	int ret = ERR_SUCCESS;

	if ( g_release == 0 && g_start_apc_ups_pthread == 0 && g_end_apc_ups_pthread == 1 )
	{
		g_start_apc_ups_pthread = 1;

		if ( pthread_create( &apc_ups_pthread, NULL, apc_ups_pthread_idle , NULL ) < 0 )
		{
			g_start_apc_ups_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create APC UPS Thread" );
			ret = ERR_RETURN;
		}
		else
		{
			//print_dbg( DBG_INIT, 1, "mini-SEED Thread Create OK. ");
		}
	}

	return ret;
}

static int init_queue( void )
{
	int er1, er2 = 0;
	int ret = ERR_SUCCESS;

	er1 = er2 = 0;
	g_apc_ups_info.queue.ptrlist_buf = ( char *)malloc( sizeof( BYTE ) * MAX_APC_TCP_QUEUE_SIZE );
	g_apc_ups_info.queue.ptrrecv_buf = ( char *)malloc( sizeof( BYTE ) * MAX_APC_TCP_RECV_SIZE );	

	if( g_apc_ups_info.queue.ptrlist_buf == NULL )
	{
		print_dbg( DBG_ERR, 1, "Can not Create APC UPS List Buffer");	
		er1 = 1;
	}

	if( g_apc_ups_info.queue.ptrrecv_buf == NULL )
	{
		print_dbg( DBG_ERR, 1, "Can not Create APC UPS Recv Buffer");	
		er2 = 1;
	}

	if ( er1 == 0 )
	{
		memset( g_apc_ups_info.queue.ptrlist_buf, 0, MAX_APC_TCP_QUEUE_SIZE );
	}
	if ( er2 == 0 )
	{
		memset( g_apc_ups_info.queue.ptrrecv_buf, 0, MAX_APC_TCP_RECV_SIZE );
	}

	if ( er1 != 0 || er2 != 0 )
		ret = ERR_RETURN;

	return ret;
}

/*
typedef apc_packet_info_
{
	int t_id;
	unsigned short addr;
	int snsr_sub_type;
	int last_val;
}apc_packet_info_t;
*/

static int init_apc_packet_list( void )
{
	int pos =0;

	memset( &g_apc_packet_mng, 0, sizeof( g_apc_packet_mng ));

	g_apc_packet_mng.list[ pos ].t_id = T_1;
	g_apc_packet_mng.list[ pos ].addr = 0x0012;
	g_apc_packet_mng.list[ pos ].reg_cnt = 1;

	g_apc_packet_mng.list[ pos ].snsr_sub_type = ESNSRSUB_DI_UPS_LOW_INPUT;
	g_apc_packet_mng.list[ pos ].last_val = -1;
	
	if ( pos++ >= MAX_APC_UPS_DATA_CNT ) 
	{
		print_dbg( DBG_ERR, 1, "Over APC Packet Cnt:%d[%s:%d:%s]", pos ,__FILE__, __LINE__,__FUNCTION__ );
		return ERR_SUCCESS;
	}

	g_apc_packet_mng.list[ pos ].t_id = T_2;
	g_apc_packet_mng.list[ pos ].addr = 0x0082;
	g_apc_packet_mng.list[ pos ].reg_cnt = 1;

	g_apc_packet_mng.list[ pos ].snsr_sub_type = ESNSRSUB_AI_UPS_BATTERY_CHARGE;
	g_apc_packet_mng.list[ pos ].last_val = -1;
	
	if ( pos++ >= MAX_APC_UPS_DATA_CNT ) 
	{
		print_dbg( DBG_ERR, 1, "Over APC Packet Cnt:%d[%s:%d:%s]", pos ,__FILE__, __LINE__,__FUNCTION__ );
		return ERR_SUCCESS;
	}

	g_apc_packet_mng.list[ pos ].t_id = T_3;
	g_apc_packet_mng.list[ pos ].addr = 0x0080;
	g_apc_packet_mng.list[ pos ].reg_cnt = 2;

	g_apc_packet_mng.list[ pos ].snsr_sub_type = ESNSRSUB_AI_UPS_BATTERY_REMAIN_TIME;
	g_apc_packet_mng.list[ pos ].last_val = -1;

	/* 마지막 단계 등록 */
	g_apc_packet_mng.last_step = pos;

	return ERR_SUCCESS;
}

static int get_apc_ups_info( void )
{
	fclt_info_t fclt;

	memset( &fclt, 0, sizeof( fclt ));
	memset( &g_apc_ups_info, 0, sizeof( g_apc_ups_info ));

	if ( get_apc_ups_fclt_info_ctrl( &fclt ) == ERR_SUCCESS )
	{
		memcpy( &g_apc_ups_info.fclt_info, &fclt , sizeof( fclt ));
		g_apc_ups_info.fd = -1;

		print_dbg( DBG_INFO, 1, "GET APC UPS FcltID :%d, FcltCode:%d", g_apc_ups_info.fclt_info.fcltid, g_apc_ups_info.fclt_info.fcltcode );
	}

	return ERR_SUCCESS;
}

#define MAX_SEND_BUF_SIZE 	300

static int send_apc_ups_req( void )
{
	int ret = ERR_SUCCESS;
	int next_step;
	int last_step ;
	int send_size;
	static BYTE send_buf [ MAX_SEND_BUF_SIZE ];
	apc_packet_info_t * ptrpacket = NULL;

	memset( &send_buf, 0, sizeof( send_buf ));

	next_step 	= g_apc_packet_mng.next_step;
	last_step 	= g_apc_packet_mng.last_step;
	send_size 	= 0;
	
	if ( next_step > last_step )
	{
		g_apc_packet_mng.next_step = 0;
		next_step = 0;
	}
	
	ptrpacket = ( apc_packet_info_t * )&g_apc_packet_mng.list[ next_step ];
	if ( ptrpacket == NULL )
	{
		print_dbg( DBG_ERR, 1, "Is null send packet [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
		return ERR_RETURN;
	}
	
	if ( make_send_packet( ptrpacket, send_buf, &send_size ) == ERR_SUCCESS )
	{
		//print_buf( send_size, send_buf );

		if ( send_tcp_data( send_size, send_buf ) == ERR_SUCCESS )
		{
			g_apc_packet_mng.next_step++;
		}
	}

	return ret;

}

static int check_apc_ups_connect_sts( int sts )
{

	set_apc_ups_trans_sts_ctrl( sts );

	if ( sts == ESNSRCNNST_NORMAL  )
	{
		print_dbg( DBG_INFO,1, "APC UPS Tcp status once the terminal is connected"); 
	}
	else
	{
		print_dbg( DBG_INFO,1, "APC UPS Tcp status once the terminal is disconnect");  	
	}

	return ERR_SUCCESS;
}

static int check_apc_ups_req_time( time_t cur_time )
{
	int ret = 0;

	( void )cur_time;

	ret= send_apc_ups_req(  );

	return ret;
}

static int check_apc_ups_data_noti( time_t cur_time )
{
	int i;
	int last_step;
	int pos =0;
	ups_data_t ups_data;

	last_step = g_apc_packet_mng.last_step;
	pos = 0;
	memset( &ups_data, 0, sizeof( ups_data ));

	for( i = 0 ; i <= last_step ;i++ )
	{
		if( g_apc_packet_mng.list[ i ].snsr_sub_type != 0 )
		{
			ups_data.sub_type[ pos ] = g_apc_packet_mng.list[ i ].snsr_sub_type;
			ups_data.val[ pos ] = g_apc_packet_mng.list[ i ].last_val;
			//printf(" POS : %d, sub_type:%d. val:%d......................\r\n", pos, ups_data.sub_type[ pos ], ups_data.val[ pos ] );
			pos++;
		}
	}
	
	if ( pos > 0 )
	{
		set_apc_ups_data_ctrl( &ups_data );
	}

	return ERR_SUCCESS;
}

static int check_tcp_connect( time_t cur_time )
{
	static volatile time_t pre_check_tcp_time ;
	static volatile int first = 0;
	time_t inter_time;

	if ( pre_check_tcp_time == 0 )
		pre_check_tcp_time = cur_time;

	inter_time = ( cur_time - pre_check_tcp_time );

	/* 3초에 한번 tcp 연결 확인 */
	if ( first == 0 || inter_time >= 3 )
	{
		first =1;
		set_tcp_connect();
		pre_check_tcp_time = cur_time;
	}

	return ERR_SUCCESS;
}

static int check_read_timeout( void )
{
	UINT32 cur_tick;
	UINT32 last_recv_tick;

	cur_tick = get_sectick();
	last_recv_tick = g_apc_packet_mng.last_recv_time;


	//printf("kkkkkkkkkkk cur_tick:%u...last_recv_tick:%u...inter:%d......\r\n", 
	//cur_tick, last_recv_tick, cur_tick - last_recv_tick );

	/* 10초간 read 를 하지 못할 경우 네트워크 단절로 판단 한다 .*/
	if ( last_recv_tick != 0 && ( cur_tick - last_recv_tick >= 10 ))
	{
		close_tcp();
		g_apc_packet_mng.last_recv_time = 0;
	}

	return ERR_SUCCESS;
}

/* apc_ups module 초기화 내부 작업 */
static int internal_init( void )
{
	int ret = ERR_SUCCESS;
	/* warring을 없애기 위한 함수 호출 */
	send_data_control( 0, NULL );

	init_apc_packet_list();

	ret = get_apc_ups_info( );

	if ( ret == ERR_SUCCESS )
	{
		ret = init_queue();
	}

	if ( ret == ERR_SUCCESS )
	{
		ret = create_apc_ups_thread();
	}

	if ( ret == ERR_SUCCESS )
	{
		print_dbg(DBG_INFO, 1, "Success of APC UPS Module Initialize");
	}

	return ret;
}

/* apc_ups module loop idle 내부 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
static int internal_idle( void * ptr )
{
	( void )ptr;

	time_t cur_time;
	apc_ups_info_t * ptrups = NULL;

	( void )ptr;

	cur_time = time( NULL );

	/* 전원 제어 중일 때는 상태 요청 하지 않는다 */
	ptrups = &g_apc_ups_info;

	if ( ptrups != NULL )
	{
		check_apc_ups_req_time( cur_time );
	}
	
	check_apc_ups_data_noti( cur_time );

	check_tcp_connect( cur_time );
	create_apc_ups_thread();
	check_read_timeout();

	return ERR_SUCCESS;
}

/* apc_ups module의 release 내부 작업 */
static int internal_release( void )
{
	g_release = 1;

	close_tcp();


	if ( g_apc_ups_info.queue.ptrlist_buf != NULL )
	{
		free( g_apc_ups_info.queue.ptrlist_buf );
		g_apc_ups_info.queue.ptrlist_buf = NULL;
	}

	if ( g_apc_ups_info.queue.ptrrecv_buf != NULL )
	{
		free( g_apc_ups_info.queue.ptrrecv_buf );
		g_apc_ups_info.queue.ptrrecv_buf = NULL;
	}

	print_dbg(DBG_INFO, 1, "Release of APC UPS Module");
	return ERR_SUCCESS;
}

/*================================================================================================
  외부  함수  정의 
  ================================================================================================*/
int init_apc_ups( send_data_func ptrsend_data )
{
	/* CONTROL module의 함수 포인트 등록 */
	g_ptrsend_apc_ups_data_control_func = ptrsend_data ;	/* CONTROL module로 설정, 제어 데이터를 보낼 수 있는 함수 포인터 */

	/* 기타 초기화 작업 */
	return internal_init();
}

/* apc_ups module loop idle 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
int idle_apc_ups( void * ptr )
{
	return internal_idle( ptr );
}

/* 동적 메모리, fd 등 resouce release함수 */
int release_apc_ups( void )
{
	/* release 작업  */
	return internal_release(); 
}

/* CONTROL Module로 부터 수집 데이터를 받는 외부 함수 */
int recv_ctrl_data_apc_ups( unsigned short msg, void * ptr )
{
	return internal_recv_ctrl_data( msg, ptr);
}

