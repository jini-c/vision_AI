#ifndef _COM_SNS_H__
#define _COM_SNS_H__

#include "type.h"
#include "com_def.h"
#if 0
#include "protocol.h"
#endif

#define MAX_RESEND_CNT	2
#define RET_TIMEOUT		1
#define RET_RESEND		2
#define TIME_OUT		3

typedef struct di_val_
{
	volatile int fcltid;
	volatile int snsr_id;			/* sensor id */
	volatile int snsr_no;			/* sensor index */
	volatile int snsr_sub_idx;		/* senosr sub index */
	volatile int snsr_type;			/* sensor type */
	volatile int snsr_sub_type;		/* sensor sub type */
	char alarm_yn[ 2 ];
	volatile int grade;
	volatile int good_val;			/* good val */
	volatile int last_val;			/* last val */
	volatile int last_sts;
	char szip[ MAX_IP_SIZE];
	char szport[ MAX_PORT_SIZE ];
    char szlabel0[ SZ_MAX_LABEL ];
    char szlabel1[ SZ_MAX_LABEL ];
	volatile UINT32 alarm_keep_time;
	volatile time_t alarm_last_time;
}di_val_t;

typedef struct do_val_
{
	volatile int fcltid;
	volatile int snsr_id;
	volatile int snsr_no;
	volatile int snsr_sub_idx;		/* senosr sub index */
	volatile int snsr_type;			/* sensor type */
	volatile int snsr_sub_type;
	char alarm_yn[ 2 ];
	volatile int grade;
	volatile int good_val;			/* good val */
	volatile int last_val;
    char szlabel0[ SZ_MAX_LABEL ];
    char szlabel1[ SZ_MAX_LABEL ];

}do_val_t;

typedef struct doe_val_
{
	volatile int fcltid;
	volatile int snsr_id;
	volatile int snsr_no;
	volatile int snsr_sub_idx;		/* senosr sub index */
	volatile int snsr_type;			/* sensor type */
	volatile int snsr_sub_type;
	char alarm_yn[ 2 ];
	volatile int grade;
	volatile int good_val;			/* good val */
	volatile int last_val;
    char szlabel0[ SZ_MAX_LABEL ];
    char szlabel1[ SZ_MAX_LABEL ];

}doe_val_t;

typedef struct ai_val_
{
	volatile int fcltid;
	volatile int snsr_id;
	volatile int snsr_no;
	volatile int snsr_sub_idx;			/* senosr sub index */
	volatile int snsr_type;			/* sensor type */
	volatile int snsr_sub_type;
	char alarm_yn[ 2 ];
	volatile float range_val[ MAX_RANG_CNT ];
	volatile float offset_val;
	volatile float min_val;
	volatile float max_val;
	volatile float last_val;
	volatile int last_sts;
	volatile int data_pos;	/* data pos from snsrmethod table 2016 11 16 */
	volatile UINT32 alarm_keep_time;
	volatile time_t alarm_last_time;
	char szip[ MAX_IP_SIZE];
	char szport[ MAX_PORT_SIZE ];
}ai_val_t;


typedef struct pwr_c_val_
{
	volatile int fcltid;
	volatile int snsr_id;
	volatile int snsr_no;
	volatile int snsr_sub_idx;		/* senosr sub index */
	volatile int snsr_type;			/* sensor type */
	volatile int snsr_sub_type;
	char alarm_yn[ 2 ];
	volatile int grade;
	volatile BYTE good_val;			/* good val */
	volatile BYTE last_val;
    char szlabel0[ SZ_MAX_LABEL ];
    char szlabel1[ SZ_MAX_LABEL ];

}pwr_c_val_t;

typedef struct temp_val_
{
	volatile int fcltid;
	volatile int snsr_id;
	volatile int snsr_no;
	volatile int snsr_sub_idx;		/* senosr sub index */
	volatile int snsr_type;			/* sensor type */
	volatile int snsr_sub_type;
	char alarm_yn[ 2 ];
	volatile float range_val[ MAX_RANG_CNT ];
	volatile float offset_val;
	volatile float min_val;
	volatile float max_val;
	volatile float last_val;
	volatile int last_sts;
	volatile UINT32 alarm_keep_time;
	volatile time_t alarm_last_time;
	char szip[ MAX_IP_SIZE];
	char szport[ MAX_PORT_SIZE ];
}temp_val_t;

typedef struct pwr_m_val_
{
	volatile int fcltid;
	volatile int snsr_id;
	volatile int snsr_no;
	volatile int snsr_sub_idx;			/* senosr sub index */
	volatile int snsr_type;			/* sensor type */
	volatile int snsr_sub_type;
	char alarm_yn[ 2 ];
	volatile float range_val[ MAX_RANG_CNT ];
	volatile float offset_val;
	volatile float min_val;
	volatile float max_val;
	volatile float last_val;
	volatile int last_sts;
	volatile int data_pos;	/* data pos from snsrmethod table 2016 11 16 */
	volatile UINT32 alarm_keep_time;
	volatile time_t alarm_last_time;

}pwr_m_val_t;

typedef struct remot_val_
{
	volatile int fcltid;
	volatile int snsr_id;
	volatile int snsr_no;
	volatile int snsr_sub_idx;			/* senosr sub index */
	volatile int snsr_type;			/* sensor type */
	volatile int snsr_sub_type;
	char alarm_yn[ 2 ];
	volatile int grade;
	volatile int good_val;			/* good val */
	volatile int last_val;
}remote_val_t;

typedef struct fclt_info_
{
	int fcltid;
	int fcltcode;
	char szip[ MAX_IP_SIZE ];
	unsigned short port;
	int send_cycle;
	int time_cycle;
	int gather_cycle;
}__attribute__ ((packed)) fclt_info_t;

typedef struct radio_sns_val_
{
	volatile int fclt_id;
	volatile int snsr_id;
	volatile int snsr_no;
	volatile int snsr_sub_idx;			/* senosr sub index */
	volatile int snsr_type;			/* sensor type */
	volatile int snsr_sub_type;
	char alarm_yn[ 2 ];
	volatile int grade;
	volatile int good_val;			/* good val */
	volatile int last_val;
	volatile int last_sts;
    char szlabel0[ SZ_MAX_LABEL ];
    char szlabel1[ SZ_MAX_LABEL ];
	volatile UINT32 alarm_keep_time;
	volatile time_t alarm_last_time;

}radio_sns_val_t;

typedef struct radio_val_manager_
{
	int cnt;
	radio_sns_val_t radio_val[ MAX_RADIO_SNS_CNT ];
}radio_val_manager_t;


/* 전원 장치  정보 */
typedef struct pwr_mthod_info_
{
	volatile BYTE comport;
	volatile BYTE dev_pos;
	volatile BYTE req_val;
	volatile BYTE last_val;
}pwr_mthod_info_t;

typedef struct pwr_mthod_mng_
{
	volatile int cnt;
	volatile int last_step;
	volatile int send_flag;
	volatile time_t last_ctrl_time;
	pwr_mthod_info_t dev[ MAX_PWR_MTHOD_CNT ];
}pwr_mthod_mng_t ;

typedef struct pwr_dev_val_
{
	volatile int snsr_id;
	volatile int snsr_no;
	volatile int snsr_sub_idx;		/* senosr sub index */
	volatile int snsr_type;			/* sensor type */
	volatile int snsr_sub_type;
	volatile int snsr_comport;
	pwr_mthod_mng_t pwr_mthod_mng;
}pwr_dev_val_t;

typedef struct mcu_data_
{
	BYTE di[ MAX_GPIO_CNT ];
	BYTE do_[MAX_GPIO_CNT ];
	BYTE doe[ MAX_GPIO_CNT ];
}mcu_data_t;

typedef struct temp_data_
{
	float heat;
	float humi;
}temp_data_t;

typedef struct pwr_c_data_
{
	UINT32 uval[ MAX_PWR_C_CNT ];
	BYTE val[ MAX_PWR_C_CNT ];
}pwr_c_data_t;

typedef struct pwr_m_data_
{
	float fval[ MAX_PWR_M_DATA_CNT ];
}pwr_m_data_t;

typedef struct remote_data_
{
	BYTE remote_val;
}remote_data_t;

typedef struct radio_data_
{
	int sub_type[ MAX_RADIO_SNS_CNT ];
	BYTE val[ MAX_RADIO_SNS_CNT ];
}radio_data_t;

typedef struct ups_data_
{
	int sub_type[ MAX_APC_UPS_DATA_CNT ];
	int val[ MAX_APC_UPS_DATA_CNT ];
}ups_data_t;

typedef struct dev_version_
{
	BYTE version[ MAX_VER_SIZE ];
}dev_version_t;

typedef struct result_
{
	UINT16 msg;
	int clientid;
	int sts;
}result_t;

typedef struct set_port_
{
	BYTE comport;
	BYTE pos;
	BYTE req_val;
}set_port_t;

typedef struct sr_data_
{
	unsigned short len;
	unsigned char cmd;
	unsigned char dev_id;
	unsigned char dev_addr;
	unsigned char dev_ch;
	unsigned char mode;
	unsigned char resp_time;
	char * ptrdata;
	unsigned char check_sum;
}__attribute__ ((packed)) sr_data_t;

typedef struct sr_queue_
{
	int head;
	char * ptrlist_buf;
	char * ptrrecv_buf;
	char * ptrsend_buf;
}__attribute__ ((packed)) sr_queue_t; 

typedef struct send_sr_data_
{
	BYTE cmd;
	int len;
	time_t send_time;
	int send_cnt;
	char * ptrdata;
}send_sr_data_t;
typedef struct send_sr_list_
{
	send_sr_data_t send_data[ MAX_SR_SEND_CNT ];
}send_sr_list_t;


int init_com_sns( void );
int release_com_sns( void );

int set_serial_fd_com_sns( unsigned int baud, char * ptrtty, char * ptrname );
int write_serial_com_sns( int fd, char * ptrdata, int len );

int parse_serial_data_com_sns( int fd, int size, sr_queue_t * ptrsr_queue, parse ptrparse_func, char * ptrname,sr_data_t * ptrsr_data ); 
BYTE checksum_com_sns(BYTE  *ptr, int len);
int make_sr_packet_com_sns( sr_data_t * ptrsr, BYTE * ptrsend );

int add_send_list_com_sns( BYTE cmd, send_sr_list_t * ptrlist, int len, char * ptrdata, BYTE dev_id );
int del_send_list_com_sns( BYTE cmd, send_sr_list_t * ptrlist, BYTE dev_id  );
int chk_send_list_timeout_com_sns( send_sr_list_t * ptrlist, int * ptr_resendpos, BYTE dev_id );

int set_get_proc_used_func( proc_used_func proc_used );

#endif
