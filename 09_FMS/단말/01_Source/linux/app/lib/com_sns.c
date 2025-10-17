#include "com_include.h"
#include "core_conf.h"
#include "com_sns.h"

/*================================================================================================
  내부  함수 정의
================================================================================================*/
proc_used_func g_proc_used_func = NULL;

static int clear_recv_buf( int fd  )
{
	if ( fd > 0 )
	{    
		tcflush( fd , TCIOFLUSH );
	}

	return ERR_SUCCESS;
}

static int set_tty( int fd, UINT32 baud , char * ptrname )
{
	int err = 0;
	struct termios tios;

	tcgetattr( fd, &tios);
	memset(&tios, 0 , sizeof(tios));

	tios.c_cflag    &= ~( CSIZE | CSTOPB | PARENB  );
	tios.c_cflag    |= CS8 | CLOCAL | CREAD | HUPCL;           /*port option */

	if ( baud == 57600 )
		tios.c_cflag |= B57600;
	else if ( baud == 38400 )
		tios.c_cflag |= B38400;
	else if ( baud == 19200 )
		tios.c_cflag |= B19200;
	else if ( baud == 9600 )
		tios.c_cflag |= B9600;
	else if ( baud == 4800 )
		tios.c_cflag |= B4800;
	else if ( baud == 2400 )
		tios.c_cflag |= B2400;
	else if ( baud == 1200 )
		tios.c_cflag |= B1200;
	else if ( baud == 115200)
		tios.c_cflag |= B115200 ;
	else
	{
		print_dbg(DBG_ERR, 1, "Not have %s Baurate:%d",ptrname,  baud );
		return ERR_RETURN ;
	}

	tios.c_iflag        = IGNPAR | IGNBRK ;
	tios.c_oflag        = 0;
	tios.c_lflag        = 0;
	tios.c_cc[ VTIME ]  = 0; /* Timeout ( 0 ) */
	tios.c_cc[ VMIN ]   = 1; /* Min of receive data size*/

	tcflush(fd, TCIOFLUSH );
	while ( tcsetattr( fd, TCSAFLUSH , &tios) < 0 && !ok_error(errno ))
	{
		err++;
		if ( err > 5 )
			break;
		sys_sleep( 100 );
	}

	if ( err > 5 )
	{
		print_dbg(DBG_ERR, 1, "Fail to setting tty");
		return ERR_RETURN ;
	}

	return ERR_SUCCESS;
}
static int inter_set_fd( UINT32 baud, char * ptrtty, char * ptrname )
{
	int fd = -1;

	if ( ptrtty == NULL || ptrname == NULL )
	{
		print_dbg( DBG_ERR, 1, "Is null ptrtty");
		return ERR_RETURN;
	}

	fd = open( ptrtty, O_RDWR | O_NOCTTY );

	if ( fd >= 0 )
	{
		if ( set_tty( fd, baud, ptrname ) != ERR_SUCCESS )
		{
			close( fd );
			fd = -1;
			print_dbg( DBG_ERR, 1, "Fail to Open MCU fd");
		}
	}

	if ( fd >=0  )
	{
		if( ptrname != NULL )
		{
			print_dbg( DBG_NONE, 1, "%s FD is :%d, Baud:%lu", ptrname, fd, baud );
		}
	}
	else
	{
		print_dbg( DBG_NONE, 1, "Fail to %s FD :%d", ptrname, fd );
	}

	return fd;
}

static int inter_write_serial( int fd, char * ptrdata, int len )
{
	int t =0, w, size;

	size = len;
	if ( ptrdata == NULL || len == 0 || fd < 0 )
	{   
		return ERR_RETURN;
	}   
	
	//print_buf( len, (BYTE*)ptrdata);

	while ( size > 0 && fd >= 0 ) 
	{   
		if ( ( w = write( fd , ptrdata, size) ) < 0 ) 
		{   
			if (errno == EINTR || errno == EAGAIN )
			{   
				continue;
			}   
			else
				break;
		}   

		if ( !w  ) break;

		size -= w;
		ptrdata += w;
		t += w;
	}   
	
	//print_dbg( DBG_INFO,1, "SEND SERIAL SIZE :%d", t );
	return t;
}

static int parse_packet( int fd, int size, sr_queue_t * ptrsr_queue, parse ptrparse_func, char * ptrname, sr_data_t * ptrsr_data )
{
	int head =0;
	int t_size, r_size,d_size = 0;
	short len = 0;

	char * ptrqueue= NULL;

	head        = ptrsr_queue->head;
	ptrqueue    = ( char *)&ptrsr_queue->ptrlist_buf[ 0 ] ;

	t_size = r_size =0;

	/* st */
	ptrqueue++;

	/* len */
	d_size = sizeof( short );
	memcpy( &len, ptrqueue, d_size );
	ptrsr_data->len	= ntohs( len ); 
	ptrqueue += d_size;
	t_size += d_size;

	/* cmd */
	d_size = sizeof( char );
	ptrsr_data->cmd        = *ptrqueue++;
	t_size += d_size;

	/* dev_id */
	ptrsr_data->dev_id   = *ptrqueue++;
	t_size += d_size;

	/*dev_addr*/
	ptrsr_data->dev_addr    = *ptrqueue++;
	t_size += d_size;

	/* dev_ch */
	ptrsr_data->dev_ch    = *ptrqueue++;
	t_size += d_size;

	/* mode */
	ptrsr_data->mode= *ptrqueue++;
	t_size += d_size;

	/* resp_time */
	ptrsr_data->resp_time = *ptrqueue++;
	t_size += d_size;
	
	/* copy data */
	d_size = ptrsr_data->len;
	if (d_size > 0 )
	{
		if ( ptrsr_data->ptrdata != NULL )
		{
			memcpy( ptrsr_data->ptrdata, ptrqueue, d_size );
		}
		ptrqueue += d_size;
		t_size += d_size;
	}

	d_size = sizeof( char );
	ptrsr_data->check_sum = *ptrqueue++;
	t_size += d_size;

	t_size += 2; /* stx + etx */

	if ( head < t_size )
	{
		memset( ptrsr_queue->ptrlist_buf, 0, MAX_SR_QUEUE_SIZE );
		ptrsr_queue->head = 0;
		clear_recv_buf( fd );
		print_dbg( DBG_ERR,1,"Invalid2 %s Packet data" , ptrname );

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
		ptrparse_func( ptrsr_data->cmd, ptrsr_data );
	}

	return r_size;
}

static int valid_packet( int fd, int size, sr_queue_t * ptrsr_queue, parse ptrparse_func, char * ptrname,sr_data_t * ptrsr_data   )
{
	int head =0;
	int chk_len = 0;

	char * ptrqueue= NULL;
	char * ptrchk_data = NULL;
	BYTE st, et, chksum, recv_chksum;
	UINT16 data_len = 0;

	if ( ptrsr_queue == NULL || ptrparse_func == NULL || ptrname == NULL || ptrsr_data == NULL )
	{
		print_dbg( DBG_ERR, 1, "Isnull prase data[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	head  = ptrsr_queue->head;

	if ( head < MIN_SR_QUEUE_SIZE )
	{
		//print_dbg( DBG_INFO,1,"Lack of SR data size( step 1 )" );
		return 0;
	}

	ptrqueue = (char*)&ptrsr_queue->ptrlist_buf[ 0 ] ;
	st = et = chksum = data_len = recv_chksum = 0;
	
	if( ptrqueue == NULL )
	{
		print_dbg( DBG_ERR, 1, "Isnull ptrqueue[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	st          = ptrqueue[ SR_STX_POS ];
	memcpy( &data_len, &ptrqueue[ SR_LEN_POS ], sizeof( short ));
	data_len 	= ntohs( data_len );

	//print_dbg( DBG_INFO,1, "data_len(%s):%d...",ptrname, data_len );
	
	if( data_len >= 100 )
	{
		memset( ptrsr_queue->ptrlist_buf, 0, MAX_SR_QUEUE_SIZE );
		ptrsr_queue->head = 0;
		clear_recv_buf( fd );

		print_dbg( DBG_ERR, 1, "Data Len is abnormal Size %d,(%s) , ST:%x", data_len, ptrname , st );
		return 0;
	}

	if ( head < ( data_len + SR_FIX_SIZE ) )
	{
		//print_dbg( DBG_INFO,1,"Lack of SR data size( step 2 )" );
		return 0;
	}

	et          = ptrqueue[ SR_ETX_POS + data_len ];
	recv_chksum = ptrqueue[ SR_CHK_POS + data_len ];
	ptrchk_data = &ptrqueue[ SR_LEN_POS ];   	/* length  position */
	chk_len     = 8 + data_len; 				/* length size + data_size */
	chksum      = checksum_com_sns((BYTE*)ptrchk_data, chk_len );

	//print_dbg( DBG_INFO,1,"sb:%x, eb:%x, chksum:%x, recv_chksum:%x, chk_len:%d", st, et, chksum, recv_chksum, chk_len );
	if ( st == SR_STX && et == SR_ETX && chksum == recv_chksum )
	{
		return 1;
	}
	else
	{
		if ( st != SR_STX )
		{
			print_dbg( DBG_ERR,1,"Invalid STX %s Packet data", ptrname );
		}

		if ( et != SR_ETX )
		{
			print_dbg( DBG_ERR,1,"Invalid ETX %s Packet data", ptrname );
		}

		if ( chksum != recv_chksum )
		{
			print_dbg( DBG_ERR,1,"Invalid CHKSUM %s Packet data", ptrname );
		}
		memset( ptrsr_queue->ptrlist_buf, 0, MAX_SR_QUEUE_SIZE );
		ptrsr_queue->head = 0;
		clear_recv_buf( fd );
		print_dbg( DBG_ERR,1,"Invalid %s Packet data", ptrname );

	}

	return 0;
}

static int inter_parse_data( int fd, int size, sr_queue_t * ptrsr_queue, parse ptrparse_func, char * ptrname,sr_data_t * ptrsr_data   )
{
	int head = 0;
	char *ptrqueue = NULL;
	char *ptrrecv = NULL;
	volatile int valid = 0;

	int remain_size = 0;

	if ( ptrsr_queue == NULL || ptrparse_func == NULL || ptrname == NULL || ptrsr_data == NULL )
	{
		print_dbg( DBG_ERR, 1, "Isnull prase data[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}


	ptrqueue    = ptrsr_queue->ptrlist_buf;
	ptrrecv     = ptrsr_queue->ptrrecv_buf;

	head        = ptrsr_queue->head;
	
	if( ptrqueue == NULL || ptrrecv == NULL )
	{
		print_dbg( DBG_ERR, 1, "Isnull ptrqueue[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		//return ERR_RETURN;
	}
	
	//print_dbg( DBG_INFO,1,"NAME:%s, Head:%d.size:%d.......................", ptrname, head, size );
	if ( head + size >= MAX_SR_QUEUE_SIZE )
	{
		print_dbg( DBG_ERR,1, "Overflow %s QUEUE BUF Head:%d, Size:%d",ptrname, head, size );
		memset( ptrqueue, 0, MAX_SR_QUEUE_SIZE );
		ptrsr_queue->head = 0;

		return ERR_RETURN;
	}

	memcpy( &ptrqueue[ head ], ptrrecv , size );
	ptrsr_queue->head += size;

	//printf("valid 1.............\r\n");
	valid = valid_packet(fd, size, ptrsr_queue, ptrparse_func, ptrname, ptrsr_data );

	while( valid )
	{
		remain_size = parse_packet( fd, size, ptrsr_queue, ptrparse_func, ptrname, ptrsr_data );

		if ( remain_size > 0 )
		{
			//printf("valid 2.............\r\n");

			valid = valid_packet(fd, remain_size, ptrsr_queue, ptrparse_func, ptrname, ptrsr_data  );

		}
		else
			valid = 0;
	}

	return ERR_SUCCESS;
	
}

int inter_make_sr_packet( sr_data_t * ptrsr, BYTE * ptrsend )
{
	int n;
	int ch_size = 0;
	BYTE chksum = 0x00;
	BYTE * ptrchk_data = NULL;
	int t_size = 0;
	int len = 0;

	if ( ptrsr == NULL || ptrsend == NULL )
	{
		print_dbg( DBG_ERR, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* STX */
	n = 1;
	*ptrsend++ = SR_STX;
	t_size += n;

	/* LEN */
	ptrchk_data = ptrsend;
	n = 2;
	len = htons(ptrsr->len);
	memcpy( ptrsend, &len, n);
	ptrsend += n;
	t_size += n;
	ch_size +=n;

	/* CMD */
	n = 1;
	*ptrsend++ = ptrsr->cmd;
	t_size += n;
	ch_size +=n;

	/* DEV_ID */
	n = 1;
	*ptrsend++ = ptrsr->dev_id ;
	t_size += n;
	ch_size +=n;

	/* DEV_ADD */
	n = 1;
	*ptrsend++ = ptrsr->dev_addr;
	t_size += n;
	ch_size +=n;

	/* DEV_CH */
	n = 1;
	*ptrsend++ = ptrsr->dev_ch;
	t_size += n;
	ch_size +=n;

	/* MODE */
	n = 1;
	*ptrsend++ = ptrsr->mode;
	t_size += n;
	ch_size +=n;

	/* REP_TIME */
	n = 1;
	*ptrsend++ = ptrsr->resp_time;
	t_size += n;
	ch_size +=n;

	/* DATA */
	if ( ptrsr->len > 0 )
	{
		n = ptrsr->len ;
		if ( ptrsr->ptrdata != NULL )
		{
			memcpy( ptrsend, ptrsr->ptrdata, n );
		}
		t_size += n;
		ch_size +=n;
		ptrsend += n;
	}

	/* CHC_SUM */
	chksum      = checksum_com_sns(ptrchk_data, ch_size );

	n =1;
	*ptrsend++ = chksum;
	t_size += n;

	/* ETX */
	n =1;
	*ptrsend++ = SR_ETX;
	t_size += n;

	return t_size;
}

static int inter_add_send_list( BYTE cmd, send_sr_list_t * ptrlist, int len, char * ptrdata , BYTE dev_id )
{
	int i = 0;
	BYTE l_cmd;
	int l_len = 0;
	char * l_ptrdata = NULL;
	time_t cur_time;
	int cpy_len = 0;
	int cnt = 0;

	if ( ptrlist == NULL || ptrdata == NULL )
	{
		print_dbg( DBG_ERR, 1, "Isnull ptrdata[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	cur_time = time( NULL );
	
	/* list가 모두 찼으면 문제가 있으므로 초기화 한다 */
	cnt = 0;
	for ( i = 0 ; i < MAX_SR_SEND_CNT ; i++ )
	{
		l_len		= ptrlist->send_data[ i ].len;
		if( l_len > 0 ) 
			cnt++;
	}

	if ( cnt >= MAX_SR_SEND_CNT )
	{
		print_dbg( DBG_ERR,1,"overflow send list buf DeviceID:%d",dev_id );
		for ( i = 0 ; i < MAX_SR_SEND_CNT ; i++ )
		{
			ptrlist->send_data[ i ].len = 0;
			ptrlist->send_data[ i ].cmd = 0;
			ptrlist->send_data[ i ].send_time= 0;
			ptrlist->send_data[ i ].send_cnt= 0;
		}
	}

	for ( i = 0 ; i < MAX_SR_SEND_CNT ; i++ )
	{
		l_cmd 		= ptrlist->send_data[ i ].cmd;
		l_len		= ptrlist->send_data[ i ].len;
		l_ptrdata 	= ptrlist->send_data[ i ].ptrdata ;

		/* 같은 데이터가 있거나 비어 있는 버퍼 */
		if ( (l_cmd == cmd && l_len != 0) || (l_len == 0 ))
		{
			if ( l_ptrdata != NULL )
			{
				memset( l_ptrdata, 0, MAX_SR_DEF_DATA_SIZE );
				cpy_len = len;
				if ( cpy_len > MAX_SR_DEF_DATA_SIZE )
				{
					cpy_len = MAX_SR_DEF_DATA_SIZE;
				}
                //if( dev_id == DEV_ID_PWR_M )
                //  printf("======================kkkkkk add :cmd :%d...\r\n", cmd );

				ptrlist->send_data[ i ].cmd 		= cmd;
				ptrlist->send_data[ i ].len			= cpy_len;
				ptrlist->send_data[ i ].send_time	= cur_time;
				ptrlist->send_data[ i ].send_cnt++;
				memcpy( l_ptrdata, ptrdata, cpy_len );

				//print_dbg( DBG_NONE, 1, "@@@@@@@@@@@@@@@ Add Send SR DevID:%d, List Pos:%d, cmd:%x, time:%ld", dev_id, i, cmd, cur_time );
				break;	
			}
		}
	}

	return ERR_SUCCESS;
}

static int inter_del_send_list( BYTE cmd, send_sr_list_t * ptrlist, BYTE dev_id )
{
	int i;
	BYTE l_cmd ;
	time_t l_time ;
	int del = 0;
	int restore = 0;
	int ret = ERR_RETURN;
	time_t cur_time = time( NULL );
    int timeout;

	if ( ptrlist == NULL )
	{
		print_dbg( DBG_ERR, 1, "Isnull ptrdata[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	for ( i = 0 ; i < MAX_SR_SEND_CNT ; i++ )
	{
		l_cmd = ptrlist->send_data[ i ].cmd;
		l_time = ptrlist->send_data[ i ].send_time;

		/* 동일한 cmd가 존재하면 삭제한다.*/
		if( (l_cmd == cmd) && l_time > 0 ) 
		{
			//if( dev_id == DEV_ID_PWR_M )
			//   printf("======================kkkkkk del :cmd :%d...\r\n", cmd );

            if ( dev_id == DEV_ID_PWR_M )
                timeout = ( TIME_OUT + 2 );
            else
                timeout = TIME_OUT;

			if ( ( cur_time - l_time ) <= timeout )
			{
				restore = 1;
			}
			del = 1;
			ptrlist->send_data[ i ].len = 0;
			ptrlist->send_data[ i ].cmd = 0;
			ptrlist->send_data[ i ].send_time= 0;
			ptrlist->send_data[ i ].send_cnt= 0;
			//print_dbg( DBG_INFO, 1, "Delete of cmd:%x,Pos :%d DevID:%d ", cmd,i, dev_id);
			break;
		}
	}

	if ( del == 0 )
	{
		print_dbg( DBG_INFO, 1, "Can not find del cmd:%x, DevID:%d ", cmd, dev_id);
	}

	/* 2초 안에 response가 온 경우 시리얼 통신 정상 */
	if ( restore == 1 )
		ret = 0;
	else
		ret = 1;

	return ret;
}

static int inter_chk_send_list_timeout( send_sr_list_t * ptrlist, int * ptr_resendpos, BYTE dev_id )
{
	int ret = ERR_SUCCESS;
	int i;
	int l_send_cnt = 0;
	int l_len = 0;
	time_t l_time, cur_time;
    int timeout;

	if ( ptrlist == NULL || ptr_resendpos == NULL )
	{
		print_dbg( DBG_ERR, 1, "Isnull ptrdata[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* send count가 2이상이 존재하면 통신 timeout으로 처리한다. */
	cur_time = time( NULL );
	for ( i = 0 ; i < MAX_SR_SEND_CNT ; i++ )
	{
		l_send_cnt		= ptrlist->send_data[ i ].send_cnt;
        if( l_send_cnt > MAX_RESEND_CNT ) 
        {
            ret = 1;
            break;
        }
	}

	/* timeout이 발생 하였을 경우 */
	if ( ret == 1 )
	{
		for ( i = 0 ; i < MAX_SR_SEND_CNT ; i++ )
		{
			ptrlist->send_data[ i ].len = 0;
			ptrlist->send_data[ i ].cmd = 0;
			ptrlist->send_data[ i ].send_time= 0;
			ptrlist->send_data[ i ].send_cnt= 0;
		}

		if ( g_proc_used_func != NULL && g_proc_used_func( dev_id ) == 1 )
		{
			print_dbg( DBG_ERR,1,"Time out send list buf DeviceID:%d",dev_id );
		}
	}
	else
	{
		for ( i = 0 ; i < MAX_SR_SEND_CNT ; i++ )
		{
			l_len	= ptrlist->send_data[ i ].len;
			l_time 	= ptrlist->send_data[ i ].send_time;


            if( dev_id == DEV_ID_PWR_M )
                sys_sleep( 10 );

            if ( dev_id == DEV_ID_PWR_M )
                timeout = ( TIME_OUT + 2 );
            else
                timeout = TIME_OUT;

			/* send time이 2초 이상 소용되는것은 time out으로 처리한다. */
			if ( (l_len > 0) &&  (l_time > 0) &&  ((cur_time - l_time) > timeout ) )
			{
				/* 재전송을 해야 할 경우 */
				*ptr_resendpos = i;
				ret = 2;
		        //print_dbg( DBG_ERR,1,"########### Resend ......cur_time:%u, l_time:%uuf DeviceID:%d",cur_time, l_time, dev_id );

                ptrlist->send_data[ i ].len = 0;
                ptrlist->send_data[ i ].cmd = 0;
                ptrlist->send_data[ i ].send_time= 0;
                ptrlist->send_data[ i ].send_cnt= 0;

                break;
			}
		}
	}
	return ret;
}

/*================================================================================================
  외부 함수 정의
================================================================================================*/
int init_com_sns( void )
{
	return ERR_SUCCESS;
}

int release_com_sns( void )
{
	return ERR_SUCCESS;
}

int set_serial_fd_com_sns( unsigned int baud, char * ptrtty, char * ptrname  )
{
	return inter_set_fd( baud, ptrtty, ptrname );
}

int write_serial_com_sns( int fd, char * ptrdata, int len )
{
	return inter_write_serial( fd, ptrdata, len );
}

int parse_serial_data_com_sns( int fd, int size, sr_queue_t * ptrsr_queue, parse ptrparse_func, char * ptrname ,sr_data_t * ptrsr_data  )
{
	return inter_parse_data( fd, size, ptrsr_queue, ptrparse_func, ptrname, ptrsr_data );
}

BYTE checksum_com_sns(BYTE  *ptr, int len)
{
	BYTE csum;
	int i;

	csum = 0x00 ;
	
	for (i= len ; i > 0; i--)
		csum ^= *ptr++;

	return (csum) ;
}

int make_sr_packet_com_sns( sr_data_t * ptrsr, BYTE * ptrsend )
{
	return inter_make_sr_packet( ptrsr, ptrsend );
}

int add_send_list_com_sns( BYTE cmd, send_sr_list_t * ptrlist, int len, char * ptrdata, BYTE dev_id)
{
	return inter_add_send_list( cmd,  ptrlist, len, ptrdata , dev_id );
}

int del_send_list_com_sns( BYTE cmd, send_sr_list_t * ptrlist, BYTE dev_id  )
{
	return inter_del_send_list(cmd, ptrlist, dev_id );
}

int chk_send_list_timeout_com_sns( send_sr_list_t * ptrlist, int * ptr_resendpos , BYTE dev_id )
{
	return inter_chk_send_list_timeout( ptrlist, ptr_resendpos, dev_id );
}

int set_get_proc_used_func( proc_used_func proc_used )
{
	g_proc_used_func = proc_used;
	return ERR_SUCCESS;
}
