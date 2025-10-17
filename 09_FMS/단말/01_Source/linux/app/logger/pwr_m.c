/*********************************************************************
	file : pwr_m.c
	description :
	product by tory45( tory45@empal.com) ( 2016.08.05 )
	update by ?? ( . . . )
	
	- Control에서 fms_server로 보낼 데이터를 받아 fms_server로 보내는 기능
	- fms_server에서 데이터를 받아 Control로 정보를 보내는 기능 
 ********************************************************************/
#include <sys/ioctl.h>
#include "com_include.h"
#include "sem.h"
#include "com_sns.h"
#include "control.h"
#include "pwr_m.h"

#define PWR_M_STS		0x01
#define PWR_M_GRD		0x02

typedef struct modbus_recv_sr_
{
	BYTE stan;
	BYTE spdf;
	BYTE len;
	float fdata[ MAX_PWR_M_DATA_CNT ];
	unsigned short sdata;
}modbus_recv_sr_t;

/*================================================================================================
 전역 변수 
 1.static 적용 
 2.숫자형 데이터 일경우 volatile 적용 
 3.초기값 적용.
================================================================================================*/
static volatile int g_pwr_m_sem_key 						= PWR_M_SEM_KEY;
static volatile int g_pwr_m_sem_id 						= 0;

static send_data_func  g_ptrsend_pwr_m_data_control_func = NULL;		/* CONTROL 모듈  센서 데이터 ,  알람  정보를 넘기는 함수 포인트 */
static sr_queue_t g_pwr_m_queue;
static send_sr_list_t g_send_sr_list;
static volatile int g_pwr_m_sr_timeout		= 0;

/* thread ( pwr_m )*/
static volatile int g_start_pwr_m_pthread 	= 0; 
static volatile int g_end_pwr_m_pthread 		= 1; 
static pthread_t pwr_m_pthread;
static void * pwr_m_pthread_idle( void * ptrdata );
static volatile int g_release				= 0;

/* serial */
static int set_pwr_m_fd( void );
static int release_pwr_m_fd( void );
static volatile int g_pwr_m_fd 				= -1;

/*================================================================================================
  내부 함수 선언
  static 적용 
================================================================================================*/
static int send_pwr_m_data_control( UINT16 inter_msg, void * ptrdata );

static int internal_init( void );
static int internal_idle( void * ptr );
static int internal_release( void );
static int create_pwr_m_thread( void );

static int release_buf( void );
static int write_pwr_m( int len, char * ptrdata , BYTE cmd);

/* Proc 함수 [ typedef int ( * proc_func )( unsigned short , void * ); ] */ 
//static int rtusvr_sns_data_res( unsigned short inter_msg, void * ptrdata );
/*================================================================================================
  Proc Function Point 구조체
 	Messsage ID 	: 내부 Message ID
	Proc Function 	: Message ID 처리 함수 
	Control에서 받은 데이터를 처리할 함수를 아래의 형식으로 등록하시면 됩니다. 
	typedef int ( * proc_func )( unsigned short , void * );
	이해가되시면 주석은 삭제하셔도 됩니다. 
================================================================================================*/

static int parse_pwr_m_status_packet( BYTE cmd, void * ptrdata );
static int parse_pwr_m_ground_packet( BYTE cmd, void * ptrdata );

static int proc_pwr_m_status_req( void );
static int proc_pwr_m_ground_req( void );

static proc_func_t g_proc_pwr_m_func_list []= 
{
	/* inter msg id   		,  			proc function point */
	/*=============================================*/

	{ 0, NULL }
};

static parse_cmd_func_t g_parse_pwr_m_func_list[ ] = 
{
	{ PWR_M_STS, 	parse_pwr_m_status_packet },
	{ PWR_M_GRD, 	parse_pwr_m_ground_packet },
	{ 0xFF	, 	NULL }
};

static UINT16 checksum_crc16( BYTE * ptrdata, int len )
{

	static const UINT16 wcrctable[] = 
	{
			0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
			0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
			0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
			0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
			0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
			0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
			0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
			0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
			0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
			0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
			0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
			0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
			0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
			0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
			0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
			0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
			0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
			0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
			0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
			0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
			0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
			0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
			0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
			0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
			0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
			0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
			0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
			0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
			0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
			0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
			0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
			0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040 
	};

	BYTE ntemp = 0;
	UINT16 wcrcword = 0xffff;

	while ( len --)
	{
		ntemp = *ptrdata++ ^ wcrcword;
		wcrcword >>= 8;
		wcrcword ^= wcrctable[ntemp];
	}

	return wcrcword;
}

static int check_pwr_m_sr_timeout( void )
{
	int ret = ERR_SUCCESS;
	int resend_pos = -1;
	char * ptrdata = NULL;
	BYTE cmd;
	int len = 0;


	lock_sem( g_pwr_m_sem_id, g_pwr_m_sem_key );
	ret = chk_send_list_timeout_com_sns( &g_send_sr_list , &resend_pos, DEV_ID_PWR_M );
	unlock_sem( g_pwr_m_sem_id, g_pwr_m_sem_key );

	if ( ret == RET_TIMEOUT )
	{
		set_pwr_m_trans_sts_ctrl( ESNSRCNNST_ERROR );
	}
	else if( ret == RET_RESEND )
	{
#if 1
		if( resend_pos >=0 )
		{
			cmd 	= g_send_sr_list.send_data[ resend_pos ].cmd;
			len		= g_send_sr_list.send_data[ resend_pos ].len;
			ptrdata = g_send_sr_list.send_data[ resend_pos ].ptrdata;

			write_pwr_m( len, ptrdata, cmd );

			print_dbg( DBG_INFO, 1, "Resend PWR_M SR Data cmd:%d, len:%d", cmd, len );

		}
#endif
	}
	return ERR_SUCCESS;
}

static int check_pwr_m_sr_restore_timeout( BYTE cmd )
{
	int ret = 0;

	lock_sem( g_pwr_m_sem_id, g_pwr_m_sem_key );
	ret = del_send_list_com_sns( cmd, &g_send_sr_list, DEV_ID_PWR_M );
	unlock_sem( g_pwr_m_sem_id, g_pwr_m_sem_key );
		
	/* 0: 2초이내 응답이 온 경우, 1: 응답은왔으나 2초 이내가 아닐 경우 */
	if ( ret == 0 )
	{
		set_pwr_m_trans_sts_ctrl( ESNSRCNNST_NORMAL );
	}

	return ERR_SUCCESS;
}

static int swap_float_buf( char * ptr )
{
	char sztemp[ 4 ];

	memset( sztemp, 0, sizeof( sztemp ));

	sztemp[ 0 ] = ptr[ 3 ] & 0xFF;
	sztemp[ 1 ] = ptr[ 2 ] & 0xFF;
	sztemp[ 2 ] = ptr[ 1 ] & 0xFF;
	sztemp[ 3 ] = ptr[ 0 ] & 0xFF;

	memcpy( ptr, sztemp, 4 );
	return ERR_SUCCESS;
}
static int swap_short_buf( char * ptr )
{
	char sztemp[ 2 ];

	memset( sztemp, 0, sizeof( sztemp ));

	sztemp[ 0 ] = ptr[ 1 ] & 0xFF;
	sztemp[ 1 ] = ptr[ 0 ] & 0xFF;

	memcpy( ptr, sztemp, 2 );
	return ERR_SUCCESS;
}

#if 0
static float orderfloat( float fval )
{
	unsigned long *puval = ( unsigned long *)&fval;
	*puval = htonl( * puval );
	return fval;
}
#endif
static int parse_packet_modbus( int size, sr_queue_t * ptrsr_queue, parse ptrparse_func, char * ptrname, modbus_recv_sr_t * ptrsr_data )
{
	int head =0;
	int t_size, r_size,d_size = 0;
	float f32 =0;
	unsigned short s16 =0;

	int i,j;
	char * ptrqueue= NULL;
	char fbuf[ 4 ];
	char sbuf[ 2 ];

	int recv_cnt  =0;
	int type = 0;
	head        = ptrsr_queue->head;
	ptrqueue    = ( char *)&ptrsr_queue->ptrlist_buf[ 0 ] ;

	t_size = r_size =0;
	
	/* stan */
	d_size = 1;
	ptrsr_data->stan = *ptrqueue++;
	t_size += d_size;

	/* spdf */
	d_size = 1;
	ptrsr_data->spdf = *ptrqueue++;
	t_size += d_size;

	/* data len */
	d_size = 1;
	ptrsr_data->len = *ptrqueue++;
	t_size += d_size;

	//printf("======================datalen:%d..................\r\n", ptrsr_data->len );
	
	if ( ptrsr_data->len == 120 )
	{
		d_size = 0;

		for( i = 0 ; i < MAX_PWR_M_DATA_CNT ; i++ )
		{
			memset( fbuf, 0, sizeof( fbuf ));
			memcpy( fbuf, ptrqueue, 4);

			swap_float_buf( fbuf );

			/* IEEE 754 float */
			for( j = 0 ; j < 4 ; j++ )
			{
				((char *)(&f32))[ j ]= fbuf[  j ];
			}

			ptrsr_data->fdata[ recv_cnt ] = f32;
			//printf("....idx:%d, fval:%f.....\r\n", i+1, f32);
			ptrqueue += 4;
			d_size +=4;
			recv_cnt ++;
		}
	}
	else
	{
#if 1
		d_size = 0;
		memset( sbuf, 0, sizeof( sbuf ));
		memcpy( sbuf, ptrqueue, 2);

		swap_short_buf( sbuf );
		memcpy( &s16, sbuf, 2 );

		ptrsr_data->sdata= s16;
		//printf("...........sval:%x.....\r\n", s16);
		ptrqueue += 2;
		d_size +=2;


        //print_buf( t_size + 2 + 2 , (BYTE*)&ptrsr_queue->ptrlist_buf[ 0 ] );
#else
		d_size = 0;
		memset( fbuf, 0, sizeof( fbuf ));
		memcpy( fbuf, ptrqueue, 4);

		swap_float_buf( fbuf );

		/* IEEE 754 float */
		for( j = 0 ; j < 4 ; j++ )
		{
			((char *)(&f32))[ j ]= fbuf[  j ];
		}

		ptrsr_data->fdata[ recv_cnt ] = f32;
		printf("....fval:%f.....\r\n", f32);
		ptrsr_data->sdata= (unsigned short)f32;

		ptrqueue += 4;
		d_size +=4;
#endif
	}

	t_size += d_size + 2; /*crc16 size */
	
	if ( head < t_size )
	{
		memset( ptrsr_queue->ptrlist_buf, 0, MAX_SR_QUEUE_SIZE );
		ptrsr_queue->head = 0;
		print_dbg( DBG_ERR,1,"Invalid %s Packet data head:%d, t_size:%d" , ptrname, head, t_size );
		return -1;
	}

	r_size = head - t_size;

	if ( r_size == 0 )
	{
		memset( ptrsr_queue->ptrlist_buf, 0, MAX_SR_QUEUE_SIZE );
		ptrsr_queue->head = 0;
	}
	else
	{
		memmove( &ptrqueue[ 0 ], &ptrqueue[ t_size ], r_size );
		ptrsr_queue->head = r_size;
	}
	//print_dbg( DBG_INFO, 1," Cortex RESP ADDR:%04x", ptrsr_data->addr& 0x0000FFFF ); 

	if ( ptrparse_func != NULL )
	{
		if ( ptrsr_data->len == 120 )
		{
			type = PWR_M_STS;
		}
		else
		{
			type = PWR_M_GRD;
		}

		ptrparse_func( type, ptrsr_data );
	}

	return r_size;
}

static int valid_packet_modbus( int size, sr_queue_t * ptrsr_queue, parse ptrparse_func, char * ptrname,modbus_recv_sr_t * ptrsr_data   )
{
	int head =0;
	int chk_len = 0;

	char * ptrqueue= NULL;
	BYTE * ptrchk_data = NULL;
	short data_len = 0;
	UINT16 crc16;
	BYTE recv_crc16[ 2 ];	
	BYTE crc_buf[ 2 ] ;

	head  = ptrsr_queue->head;

	if ( head < MIN_SR_QUEUE_SIZE )
	{
		//print_dbg( DBG_INFO,1,"Lack of SR data size( step 1 )" );
		return 0;
	}

	memset( &recv_crc16, 0, sizeof( recv_crc16));
	memset( &crc_buf , 0, sizeof( crc_buf ));

	ptrqueue = (char*)&ptrsr_queue->ptrlist_buf[ 0 ] ;
	ptrchk_data = (BYTE*)&ptrsr_queue->ptrlist_buf[ 0 ]; 

	crc16 = 0;

	/* data len */
	data_len = ptrqueue[ 2 ] & 0xFF ;

	/* stan + spdf + data len + crc : 5byte */
	if ( head < ( data_len + 5 ) )
	{
		//print_dbg( DBG_INFO,1,"Lack of SR data size( step 2 )" );
		return 0;
	}

	/* stan + spdf + data len  */
	recv_crc16[ 0 ]= ptrqueue[ 3 + data_len ];
	recv_crc16[ 1 ]= ptrqueue[ 4 + data_len ];

	/* stan + spdf + len + data len  */
	chk_len     = 3 + data_len; 				/* length size + data_size */
	crc16      = checksum_crc16(ptrchk_data, chk_len );

	//print_dbg( DBG_INFO,1,"sb:%x, eb:%x, chksum:%x, recv_chksum:%x, chk_len:%d", st, et, chksum, recv_chksum, chk_len );
	crc_buf[ 0 ] = (BYTE)((crc16>> 0 ) & 0x00FF );
	crc_buf[ 1 ] = (BYTE)((crc16>> 8 ) & 0x00FF );

	//printf("....data_len:%d, r0:%x, r1:%x, c:%x, c2:%x................\r\n",data_len, recv_crc16[ 0 ], recv_crc16[1], crc_buf[ 0], crc_buf[ 1 ]);
	if ( (recv_crc16[ 0 ] == crc_buf[ 0 ] ) && ( recv_crc16 [ 1 ] == crc_buf[ 1 ] ))
	{
		return 1;
	}
	else
	{
		memset( ptrsr_queue->ptrlist_buf, 0, MAX_SR_QUEUE_SIZE );
		ptrsr_queue->head = 0;
		print_dbg( DBG_ERR,1,"Invalid %s Packet data", ptrname );
	}

	return 0;
}

static int parse_serial_data_modbus( int size, sr_queue_t * ptrsr_queue, parse ptrparse_func, char * ptrname, modbus_recv_sr_t * ptrsr_data   )
{
	int head = 0;
	char *ptrqueue = NULL;
	char *ptrrecv = NULL;
	int valid = 0;
	int remain_size = 0;

	if ( ptrsr_queue == NULL || ptrparse_func == NULL || ptrname == NULL || ptrsr_data == NULL )
	{
		print_dbg( DBG_ERR, 1, "Isnull prase data[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrqueue    = ptrsr_queue->ptrlist_buf;
	ptrrecv     = ptrsr_queue->ptrrecv_buf;

	head        = ptrsr_queue->head;

	if ( head + size >= MAX_SR_QUEUE_SIZE )
	{
		print_dbg( DBG_ERR,1, "Overflow %s QUEUE BUF Head:%d, Size:%d",ptrname, head, size );
		memset( ptrqueue, 0, MAX_SR_QUEUE_SIZE );
		ptrsr_queue->head = 0;

		return ERR_RETURN;
	}

	memcpy( &ptrqueue[ head ], ptrrecv , size );
	ptrsr_queue->head += size;

	valid = valid_packet_modbus( size, ptrsr_queue, ptrparse_func, ptrname, ptrsr_data );

	while( valid )
	{
		remain_size = parse_packet_modbus( size, ptrsr_queue, ptrparse_func, ptrname, ptrsr_data );

		if ( remain_size > 0 )
			valid = valid_packet_modbus( remain_size, ptrsr_queue, ptrparse_func, ptrname, ptrsr_data  );
		else
			valid = 0;
	}

	return ERR_SUCCESS;
	
}

static int make_sr_packet_modbus(int type,  BYTE * ptrsend )
{
	int n;
	int ch_size = 0;
	UINT16  crc16 = 0x00;
	BYTE * ptrchk_data = NULL;
	int t_size = 0;
	short w_cnt = 0;

	if ( ptrsend == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrchk_data = ptrsend;
	/* STAN(국번) */
	n = 1;
	*ptrsend++ = 0x01;
	t_size += n;
	ch_size +=n;

	/* SPDF 4 = 19200: Function(*/
	n = 1;
	*ptrsend++ = 0x04; /* read input */
	t_size += n;
	ch_size +=n;
	
	/* STS */
	if ( type == PWR_M_STS )
	{
		/* address ( 30001 번지 ) */
		n = 2;
		*ptrsend++ = 0x00;
		*ptrsend++ = 0x00;
		t_size += n;
		ch_size +=n;

		/* 데이터 워드 개수 : 60개  */
		n = sizeof( short );
		w_cnt = MAX_PWR_M_WRD_CNT;
		w_cnt = htons( w_cnt );
		memcpy(ptrsend, &w_cnt, n );
		ptrsend += n;
		t_size += n;
		ch_size +=n;
	}
	/* Ground :지락 */
	else
	{
#if 1
		/* address ( 30096 번지 ) */
		n = 2;
		*ptrsend++ = 0x00;
		*ptrsend++ = 0x5f; /* offset : 95 */
		//*ptrsend++ = 0x49; /* offset :날짜*/
		t_size += n;
		ch_size +=n;

		/* 데이터 워드 개수 : 1개  */
		n = sizeof( short );
		w_cnt = 1;
		w_cnt = htons( w_cnt );
		memcpy(ptrsend, &w_cnt, n );
		ptrsend += n;
		t_size += n;
		ch_size +=n;
#else
		/* address ( 30096 번지 ) */
		n = 2;
		*ptrsend++ = 0x00;
		*ptrsend++ = 0x18; /* offset : 95 */
		t_size += n;
		ch_size +=n;

		/* 데이터 워드 개수 : 1개  */
		n = sizeof( short );
		w_cnt = 2;
		w_cnt = htons( w_cnt );
		memcpy(ptrsend, &w_cnt, n );
		ptrsend += n;
		t_size += n;
		ch_size +=n;

#endif
	}
	/* CHC_SUM : 16bit CRC */
	crc16      = checksum_crc16( ptrchk_data, ch_size );

	n =2;
	*ptrsend++ = (BYTE)((crc16>> 0 ) & 0x00FF );
	*ptrsend++ = (BYTE)((crc16>> 8 ) & 0x00FF );
    
	t_size += n;
	return t_size;
}

static int parse_pwr_m_status_packet( BYTE cmd, void * ptrdata )
{
	int i;
	modbus_recv_sr_t * ptrsr = NULL;
	static pwr_m_data_t pwr_m_data;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	//print_dbg(DBG_NONE, 1,"Recv PWR_M Status Packet");

	check_pwr_m_sr_restore_timeout( PWR_M_STS );

	memset(&pwr_m_data, 0, sizeof( pwr_m_data ));

	ptrsr = ( modbus_recv_sr_t *)ptrdata;
	for( i =0 ; i<  MAX_PWR_M_DATA_CNT ; i++ )
	{
		pwr_m_data.fval[ i ]= ptrsr->fdata[ i ];
	}

	set_pwr_m_data_ctrl(&pwr_m_data );

	return ERR_SUCCESS;
}

static int parse_pwr_m_ground_packet( BYTE cmd, void * ptrdata )
{
	modbus_recv_sr_t * ptrsr = NULL;
	unsigned short val;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	//print_dbg(DBG_NONE, 1,"Recv PWR_M Ground Packet");

	check_pwr_m_sr_restore_timeout( PWR_M_GRD );

	ptrsr = ( modbus_recv_sr_t *)ptrdata;
	val = ptrsr->sdata;

	set_pwr_m_ground_ctrl( val );

	return ERR_SUCCESS;
}


static int proc_pwr_m_status_req( void )
{
	int t_size = 0;
	static BYTE sts_send_buf[ MAX_SR_DEF_DATA_SIZE ];
	
	memset( sts_send_buf,0, sizeof( sts_send_buf ));
	t_size = make_sr_packet_modbus(PWR_M_STS,  sts_send_buf );

	if ( t_size > 0 )
	{
		write_pwr_m( t_size, (char*)&sts_send_buf[0] , PWR_M_STS ); 
	}

	return ERR_RETURN;
}

static int proc_pwr_m_ground_req( void )
{
	int t_size = 0;
	static BYTE sts_send_buf[ MAX_SR_DEF_DATA_SIZE ];
	
	memset( sts_send_buf,0, sizeof( sts_send_buf ));
	t_size = make_sr_packet_modbus( PWR_M_GRD, sts_send_buf );

	if ( t_size > 0 )
	{
		write_pwr_m( t_size, (char*)&sts_send_buf[0] , PWR_M_GRD ); 
        //print_buf( t_size, &sts_send_buf[ 0 ]);

	}

	return ERR_RETURN;
}


/*================================================================================================
내부 함수  정의 
매개변수가 pointer일경우 반드시 NULL 체크 적용 
================================================================================================*/
static int parse_pwr_m_cmd( BYTE cmd, void * ptrdata )
{
	int i,cnt = 0;
	parse tmp_parse = NULL;
	BYTE l_cmd = 0;

	cnt = ( sizeof( g_parse_pwr_m_func_list ) / sizeof( parse_cmd_func_t ));

	for( i = 0 ; i < cnt ; i++ )
	{
		tmp_parse 	= g_parse_pwr_m_func_list[ i ].parse_func ;
		l_cmd 		= g_parse_pwr_m_func_list[ i ].cmd; 

		if ( tmp_parse!= NULL && cmd == l_cmd )
		{
			tmp_parse( cmd, ptrdata );
		}
	}

	return ERR_SUCCESS;	
}

static int write_pwr_m( int len, char * ptrdata, BYTE cmd )
{
	int ret = ERR_SUCCESS;

	if( g_pwr_m_fd < 0 )
	{
		print_dbg( DBG_ERR,1, "Can not Write PWR_M Serial, fd:%d", g_pwr_m_fd );
		return ERR_RETURN;
	}

	if ( ptrdata == NULL || len ==  0 )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* lock */
	//print_buf( len , (BYTE*)ptrdata);
    //printf("==============pwrm send size:%d....\r\n", len );
	ret = write_serial_com_sns( g_pwr_m_fd, ptrdata, len );
	/* unlock */
	if ( ret <= 0 )
	{
		print_dbg( DBG_ERR,1, "Can not Write PWR_M Serial, fd:%d, len:%d", g_pwr_m_fd, len );
	}
	else
	{
		/* send list buf에 등록 */
		lock_sem( g_pwr_m_sem_id, g_pwr_m_sem_key );
		add_send_list_com_sns( cmd, &g_send_sr_list, ret, ptrdata , DEV_ID_PWR_M );
		unlock_sem( g_pwr_m_sem_id, g_pwr_m_sem_key );

	}
	return ret;
}

static int read_pwr_m( void )
{

	int size ;
	char * ptrrecv= NULL;
	static modbus_recv_sr_t mod_sr_data;

	if ( g_pwr_m_fd > 0 )
	{
		ptrrecv = g_pwr_m_queue.ptrrecv_buf;

		if ( ptrrecv != NULL )
		{
			size =0;
			memset( ptrrecv, 0, MAX_SR_RECV_SIZE );
			size = read( g_pwr_m_fd , ptrrecv, MAX_SR_RECV_SIZE  );

			if ( size > 0 )
			{
				memset( &mod_sr_data, 0, sizeof( mod_sr_data ));
				//print_buf( size, (BYTE*)ptrrecv );
				parse_serial_data_modbus( size, &g_pwr_m_queue, parse_pwr_m_cmd, PWR_M_NAME, &mod_sr_data );
				//g_last_read_time = time( NULL );
			}
			//print_buf(size, (BYTE*)ptrrecv );
			//print_dbg( DBG_INFO,1,"size:%d, %s", size, ptrrecv);
		}
		else
		{
			print_dbg( DBG_ERR, 1, "Not have pwr_m recv buffer");
		}
	}
	return ERR_SUCCESS;
}

static int create_pwr_m_thread( void )
{
	int ret = ERR_SUCCESS;

	if ( g_release == 0 && g_start_pwr_m_pthread == 0 && g_end_pwr_m_pthread == 1 )
	{
		g_start_pwr_m_pthread = 1;

		if ( pthread_create( &pwr_m_pthread, NULL, pwr_m_pthread_idle , NULL ) < 0 )
		{
			g_start_pwr_m_pthread = 0;
			print_dbg( DBG_ERR, 1, "Fail to Create PWR_M Thread" );
			ret = ERR_RETURN;
		}
		else
		{
			//print_dbg( DBG_INIT, 1, "mini-SEED Thread Create OK. ");
		}
	}

	return ret;
}

#if 0
static int release_pwr_m_thread( void )
{
	int status = 0;
	
	if ( g_end_pwr_m_pthread == 0 )
	{
		pthread_join( pwr_m_pthread , ( void **)&status );
		g_start_pwr_m_pthread 	= 0;
		g_end_pwr_m_pthread 		= 1;
	}

	return ERR_SUCCESS;
}
#endif

static void *  pwr_m_pthread_idle( void * ptrdata )
{
	( void ) ptrdata;
	int status;

	g_end_pwr_m_pthread = 0;

	while( g_release == 0 && g_start_pwr_m_pthread == 1 )
	{
		read_pwr_m();
	}

	if( g_end_pwr_m_pthread == 0 )
	{
		pthread_join( pwr_m_pthread , ( void **)&status );
		g_start_pwr_m_pthread 	= 0;
		g_end_pwr_m_pthread 		= 1;
	}

	return ERR_SUCCESS;
}

static int set_pwr_m_fd( void )
{
	int fd = -1;

	if ( g_pwr_m_fd >= 0 )
		return ERR_SUCCESS;

	fd = set_serial_fd_com_sns( PWR_M_BAUD, PWR_M_TTY, PWR_M_NAME );

	if ( fd >= 0 )
	{
		g_pwr_m_fd = fd;
	}
	else
	{
		set_pwr_m_trans_sts_ctrl( ESNSRCNNST_ERROR );
		return ERR_RETURN;
	}

	return ERR_SUCCESS;
}

static int release_pwr_m_fd( void )
{
	if ( g_pwr_m_fd >= 0 )
	{
		close ( g_pwr_m_fd );
		g_pwr_m_fd = -1;
	}

	return ERR_SUCCESS;
}


/* CONTROL module에  보낼 데이터(제어, 설정, 알람)가  있을 경우 호출 */
static int send_pwr_m_data_control( UINT16 inter_msg, void * ptrdata  )
{
	if ( g_ptrsend_pwr_m_data_control_func != NULL )
	{
		return g_ptrsend_pwr_m_data_control_func( inter_msg, ptrdata );
	}

	return ERR_RETURN;
}

/* CONTROL Module로 부터 수집 데이터, 알람, 장애  정보를 받는 내부 함수로
   등록된 proc function list 중에서 control에서  넘겨 받은 inter_msg와 동일한 msg가 존재할 경우
   해당  함수를 호출하도록 한다. 
 */
static int internal_pwr_m_recv_ctrl_data( unsigned short inter_msg, void * ptrdata )
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
	proc_func_cnt= ( sizeof( g_proc_pwr_m_func_list ) / sizeof( proc_func_t ));

	/* 등록된 proc function에서 inter_msg가 동일한 function 찾기 */
	for( i = 0 ; i < proc_func_cnt ; i++ )
	{    
		list_msg 			= g_proc_pwr_m_func_list[ i ].inter_msg; 	/* function list에 등록된 msg id */
		ptrtemp_proc_func 	= g_proc_pwr_m_func_list[ i ].ptrproc;	/* function list에 등록된 function point */

		if ( ptrtemp_proc_func != NULL && list_msg == inter_msg )
		{    
			ptrtemp_proc_func( inter_msg, ptrdata );
			break;
		}    
	} 

	return ERR_SUCCESS;
}

/* pwr_m module 초기화 내부 작업 */
static int internal_init( void )
{
	int i,ret = ERR_SUCCESS;

	/* warring을 없애기 위한 함수 호출 */
	send_pwr_m_data_control( 0, NULL );

	ret = set_pwr_m_fd();
	
	if ( ret == ERR_SUCCESS )
	{
		ret = create_pwr_m_thread( );
	}

	memset( &g_pwr_m_queue, 0, sizeof( g_pwr_m_queue ));
	memset( &g_send_sr_list, 0,sizeof( g_send_sr_list ));

	/* buf 생성 */
	g_pwr_m_queue.ptrlist_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_QUEUE_SIZE );
	g_pwr_m_queue.ptrrecv_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_RECV_SIZE );	
	g_pwr_m_queue.ptrsend_buf = ( char *)malloc( sizeof( BYTE ) * MAX_SR_SEND_SIZE );	

	if( g_pwr_m_queue.ptrlist_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create PWR_M SR List Buffer");
		ret = ERR_RETURN;
	}

	if( g_pwr_m_queue.ptrrecv_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create PWR_M SR Recv Buffer");
		ret = ERR_RETURN;
	}

	if( g_pwr_m_queue.ptrsend_buf == NULL )
	{
		print_dbg(DBG_ERR, 1, "Can not Create PWR_M SR Send Buffer");
		ret = ERR_RETURN;
	}

	if ( ret == ERR_SUCCESS )
	{
		print_dbg(DBG_INFO, 1, "Success of PWR_M Module Initialize");
	}
	else
	{
		release_buf();
	}


	/* send list buf 생성 */
	for( i = 0 ; i < MAX_SR_SEND_CNT ; i++ )
	{
		g_send_sr_list.send_data[ i ].ptrdata= ( char *)malloc( sizeof( BYTE ) * MAX_SR_DEF_DATA_SIZE);
		if ( g_send_sr_list.send_data[ i ].ptrdata== NULL )
		{
			print_dbg(DBG_ERR, 1, "Can not Create PWR_M SR Send List Buffer");
			ret = ERR_RETURN;
		}
		else
		{
			memset( g_send_sr_list.send_data[ i ].ptrdata, 0, MAX_SR_DEF_DATA_SIZE );
		}
	}

	g_pwr_m_sem_id = create_sem( g_pwr_m_sem_key );
	print_dbg( DBG_INFO, 1, "PWR_M SEMA : %d", g_pwr_m_sem_id );


	return ret;
}

static int idle_pwr_m_func( void )
{
	static volatile int flag = 0;

	if ( flag == 0 )
	{
		proc_pwr_m_status_req( );
		//proc_pwr_m_ground_req( );

		flag = 1;
	}
	else
	{
		proc_pwr_m_ground_req( );
		flag = 0;
	}
	
	check_pwr_m_sr_timeout( );

	return ERR_SUCCESS;
}

/* pwr_m module loop idle 내부 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
static int internal_idle( void * ptr )
{
	( void )ptr;

	if( g_release == 0 )
	{
		create_pwr_m_thread();
		set_pwr_m_fd( );

		idle_pwr_m_func();
	}
	return ERR_SUCCESS;
}

static int release_buf ( void )
{
	int i = 0;

	if ( g_pwr_m_queue.ptrlist_buf != NULL )
	{
		free( g_pwr_m_queue.ptrlist_buf );
		g_pwr_m_queue.ptrlist_buf  = NULL;
	}

	if ( g_pwr_m_queue.ptrrecv_buf != NULL )
	{
		free( g_pwr_m_queue.ptrrecv_buf );
		g_pwr_m_queue.ptrrecv_buf  = NULL;
	}

	if ( g_pwr_m_queue.ptrsend_buf != NULL )
	{
		free( g_pwr_m_queue.ptrsend_buf );
		g_pwr_m_queue.ptrsend_buf  = NULL;
	}

	
	for( i = 0 ; i < MAX_SR_SEND_CNT ;i++ )
	{
		if ( g_send_sr_list.send_data[ i ].ptrdata != NULL )
		{
			free( g_send_sr_list.send_data[ i ].ptrdata );
			g_send_sr_list.send_data[ i ].ptrdata = NULL;
		}
	}
	if ( g_pwr_m_sem_id > 0 )
	{
		destroy_sem( g_pwr_m_sem_id, g_pwr_m_sem_key );
		g_pwr_m_sem_id = -1;
	}

	return ERR_SUCCESS;
}

/* pwr_m module의 release 내부 작업 */
static int internal_release( void )
{
	g_release = 1;
	
	release_pwr_m_fd();
	//release_pwr_m_thread();

	print_dbg(DBG_INFO, 1, "Release of PWR_M Module");
	return ERR_SUCCESS;
}

/*================================================================================================
외부  함수  정의 
================================================================================================*/
int init_pwr_m( send_data_func ptrsend_data )
{
	/* CONTROL module의 함수 포인트 등록 */
	g_ptrsend_pwr_m_data_control_func = ptrsend_data ;	/* CONTROL module로 센서 , 장애  데이터를 보낼 수 있는 함수 포인터 */

	/* 기타 초기화 작업 */
	internal_init();

	return ERR_SUCCESS;
}

/* pwr_m module loop idle 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
int idle_pwr_m( void * ptr )
{
	return internal_idle( ptr );
}

/* 동적 메모리, fd 등 resouce release함수 */
int release_pwr_m( void )
{
	/* release 작업  */
	return internal_release(); 
}

/* CONTROL Module로 부터 제어, 설정, update 정보를 받는 외부 함수 */
int recv_ctrl_data_pwr_m( unsigned short inter_msg, void * ptrdata )
{
	return internal_pwr_m_recv_ctrl_data( inter_msg, ptrdata );
}
