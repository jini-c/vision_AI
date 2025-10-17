#ifndef _COM_DEF_H_
#define _COM_DEF_H_

#define ok_error(num) ((num)==EIO)

#ifdef USE_WEBSOCK 
/*WebSocket을 사용하는 버전 */
#define APP_VER					"5.1"
#else
/*WebSocket을 사용하지 않는  버전 */
#define APP_VER					"92.7"
#endif

#define OBSERVER_VER			"0.5"
#define APP_PNAME   			"fms_logger"

#define RTU_RUN_SVR_LINK	0
#define RTU_RUN_ALONE		1

#define MAX_DELAY_TIME          5          /* 전원 포트 제어 delay 시간 */
#define TEST_MOR_TIME_SEC       15          /* 테스트 모니터링 전송 시간 */
#define REAL_MOR_TIME_SEC       ( 60 * 10 ) /* 실제 모니터링 전송 시간 */

#define REAL_CUR_TIME_SEC       15          /* 실시간 데이터 전송 시간 */
#define MAX_AUTO_ON_START_TIME	( 30 ) /* 자동 모드 전환 시간 */

/* Semaphore	*/
#define SHM_SEM_KEY				8000			/* share memory */
#define CTRL_SEM_KEY			8002			/* contorl memory */
#define FMS_CLIENT_KEY			8004			/* fms client */
#define OP_SERVER_KEY			8006			/* op server */
#define UPDATE_KEY				8008			/* update client */

#define MCU_SEM_KEY				8010			/* mcu module */
#define REMOTE_SEM_KEY			8012			/* remote module */
#define PWR_C_SEM_KEY			8014			/* power control module */
#define PWR_M_SEM_KEY			8016			/* power control module */

/* path */
#define APP_PATH_1				"/home/fms/appm/"
#define APP_PATH_2				"/home/fms/apps/"
#define APP_TEMP_PATH			"/home/fms/temp/"
#define CONF_PART_1				"/home/fms/confm/"
#define CONF_PART_2				"/home/fms/confs/"

#define SYS_CPU				"/home/fms/temp/cpu.txt"
#define SYS_RAM				"/home/fms/temp/ram.txt"
#define SYS_HRD				"/home/fms/temp/hrd.txt"

/* size */
#define ADD_9H					( 600 * 6 * 9 )	
#define MAX_IP_SIZE				20		/* ip string length */
#define MAX_PORT_SIZE			6		/* ip string length */
#define MAX_PATH_SIZE			300	
#define MAX_PRINT_SIZE			500
#define MAX_VER_SIZE			2
#define MAX_TIME_SIZE			40
#define MAX_DATA_SIZE           100

#define MAX_DI_SIZE				8
#define MAX_DO_SIZE				8
#define MAX_AI_SIZE				10

/* ERR CODE */
#define ERR_SUCCESS             0
#define ERR_FAIL                -1
#define ERR_RETURN              ERR_FAIL
#define ERR_BUSY                -2
#define ERR_TIMEOUT             -3 // Timeout or Error
#define ERR_INIT                -4 // Not Initialized
#define ERR_UPDATE_FAIL         -5 // Failed to update
#define ERR_OVERFLOW            -6 // array index overflow error

/* DEBUG TYPE */
#define DBG_NONE				0x01
#define DBG_INFO				0x02
#define DBG_INIT				0x03
#define DBG_ETC					0x04
#define DBG_LINE				0x10
#define DBG_SAVE				0x20
#define DBG_ERR					0xFF
/* common */
#define VAL_VERIFY				"angel"
#define STR_SPACE				"       "
#define STR_SEP					","
/* CHECK DATA */
#define CHK_DATA1				0x31
#define CHK_DATA2				0x32

/* conf session */
#define SESSION_DEF				"default"
#define SESSION_BASE			"basic"

/* conf key*/
#define KEY_VERIFY				"verify"
#define KEY_SAVE_LOG			"save_log"
#define KEY_SITEID				"siteid"
#define KEY_IFNAME				"ifname"
#define IFNAME					"p1p1"

/* network info */
#define KEY_IP				    "ip"
#define KEY_SUB			        "netmask"
#define KEY_GATE			    "gateway"

#define KEY_DB_USER             "db_user"
#define KEY_DB_PWR              "db_pwr"
#define KEY_DB_NAME             "db_name"
#define KEY_WEB_PORT            "web_port"
#define KEY_NTP_ADDR            "ntp_addr"
#define KEY_RTU_RUN_MODE        "rtu_mode"
#define KEY_RTU_VER        		"rtu_ver"
#define KEY_RTU_WEB        		"rtu_web"

#define KEY_UPDATE_IP           "update_ip"
#define KEY_UPDATE_PORT         "update_port"
/* File Name */
#define VERIFY_FILE				"verify_file"
#define CONF_FILE				"conf.dat"	/* config 파일 */
#define RUN_FILE				"run.dat"	/* run 파일 */
#define NET_FILE			    "net.dat"	/* 네트워크 파일 */
#define UPDATE_FILE			    "update.dat"	/* 네트워크 파일 */

/* protocol Define */
#define OP_FMS_PROT				0xE2D8
#define SZ_MAX_DATE             8        // "20161212"
#define SZ_MAX_TIME             10       // "235852.001"
#define SZ_MAX_NAME             20       // 설비 이름
#define SZ_MAX_LABEL          	30       // 설비 이름


#define MAX_DB_USER_NAME_SIZE   25 
#define MAX_DB_USER_PWR_SIZE    25
#define MAX_DB_NAME_SIZE        25

#define TCP_TIMEOUT				3
#define PNT_SIZE				8
#define MAX_NTP_ADDR_SIZE 		30
// -----------------------------------------?????
// FCLT_TID : 전국적으로 Unique ID 생성
// FCLT_TID 컬럼 문자 길이
// 4 4 7 4 2 4 = 24 문자
// 년도(4) + 날짜(4) + 시간(초)(6) + 국소ID(4) + SNSR_TYPE(2) + 시퀀스(4)
#define MAX_DBFCLT_TID			(24 + 1)	
#define MAX_USER_ID_OLD			(10 + 1)
#define MAX_USER_ID				(128 + 1)

#define MAX_GRP_RSLT_DI			16		// 여유있게 16개 까지 허용
#define MAX_GRP_RSLT_AI			8		// 여유있게 8개 까지 허용
#define MAX_CHN_RSLT_AI			4		// 하나의  AI 센서의 최대 데이터 개수 

#define MAX_FCLT_CNT			10
#define MAX_RADIO_SNS_CNT		20
#define MAX_UPS_SNS_CNT			10

/* PROC INDEX  */
#define MAX_PROC_CNT	15

/* TTY INFO */
#define DEV_ID_COM				0x00
#define DEV_ID_TEMP				0x01
#define DEV_ID_PWR_C			0x04
#define DEV_ID_MCU				0x05
#define DEV_ID_REMOTE			0x06
#define DEV_ID_PWR_M			0x99

#define PROC_PWR_C				DEV_ID_PWR_C	
#define PROC_REMOTE				DEV_ID_REMOTE	
#define PROC_TEMP				DEV_ID_TEMP
#define PROC_PWR_M				DEV_ID_PWR_M	
#define PROC_RADIO				0x9A	
#define PROC_APC_UPS			0x9B

#define MCU_TTY					"/dev/ttyS0"
#define MCU_BAUD				115200
#define MCU_NAME				"MCU"

#define REMOTE_TTY				"/dev/ttyS1"
#define REMOTE_BAUD				115200
#define REMOTE_NAME				"Remote Control"

#define PWR_M_TTY				"/dev/ttyS2"
#define PWR_M_BAUD				19200	
#define PWR_M_NAME				"Power Monitor"

#define TEMP_TTY				"/dev/ttyS3"
#define TEMP_BAUD				115200
#define TEMP_NAME				"Temprature"

#define PWR_C_TTY				"/dev/ttyS4"
#define PWR_C_BAUD				115200
#define PWR_C_NAME				"Power Control"

#define SR_STX					0x02
#define SR_ETX					0x03
#define MAX_SEND_CNT			9999

#define MAX_FIRM_SIZE			1024	/* 한번에 전송할 펌웨어 최대 사이즈 */
#define MAX_SR_DEF_DATA_SIZE	125		/* 기본 Serial DATA 버퍼 사이즈 */
#define MAX_SR_RECV_SIZE		125		/* 최대 Serial Read 사이즈 */
#define MAX_SR_SEND_SIZE		1100	/* 펌웨어를 포함할 경우 가정 */	
#define MAX_SR_QUEUE_SIZE		( MAX_SR_RECV_SIZE * 5 )
#define MIN_SR_QUEUE_SIZE		3
#define MAX_SR_SEND_CNT			5

#define SR_STX_POS				0
#define SR_LEN_POS				1
#define SR_CHK_POS				9 	/* + DATA SIZE */
#define SR_ETX_POS				10  /* + DATA SIZE */
#define SR_FIX_SIZE				11

#define MAX_GPIO_CNT			8
#define MAX_TEMP_CNT			4
#define MAX_PWR_C_CNT			12
#define MAX_PWR_C_PORT_CNT		6
#define MAX_PWR_M_CNT			6
#define MAX_RANG_CNT			6
#define MAX_PWR_MTHOD_CNT		8
#define MAX_PWR_DEV_CNT			8	
#define MAX_APC_UPS_DATA_CNT	6

#define MAX_PWR_M_WRD_CNT		60
#define MAX_PWR_M_DATA_CNT		( MAX_PWR_M_WRD_CNT / 2 )
#define MAX_PWR_M_VALID_CNT		4

#define MAX_BIT_CNT				8
#define BIT_0				0x01
#define BIT_1				0x02
#define BIT_2				0x04
#define BIT_3				0x08
#define BIT_4				0x10
#define BIT_5				0x20
#define BIT_6				0x40
#define BIT_7				0x80

#define COM1				1
#define COM2				2
#define COM3				3
#define COM4				4
#define COM5				5
#define COM6				6

#define TEST_INTVAL			1

#define SEND_FMS				0
#define SEND_OP					1
#define SEND_WEB				2

#define MAX_DBG_BUF_SIZE 	1500
#define DEBUG_PORT			40405
#define DEBUG_IP			"127.0.0.1"

#define NOT_ALIVE 	( -1 )
#define ALIVE		( 1 )

#define DO_TYPE				1
#define LIGHT_TYPE			2
#define DOE_TYPE			3
#define PWR_TYPE			4
#define AIR_TYPE			5
#define DI_TYPE				6
#define VER_TYPE			7
#define REBOOT_TYPE			8
#define RESERVED_TYPE		9

#define MAX_COL_NAME_SIZE 	3
#define MAX_COL_NAME_CNT	50

#define MAX_PWR_SWITCH_CNT  5
#define MAX_VERSION_SIZE	20

#define WEB_CLIENTID		-100
/*====================================================================================================
  struct 
  ===================================================================================================*/

/* function point */
typedef int ( * send_debug_func )( char * , int );

/* 모듈간 Interface 함수 포인터 */
typedef int ( * send_data_func )( unsigned short , void * );
typedef int ( * send_noti_func )( unsigned short , void * );

/* 메시지 별 처리 함수 포인터 */
typedef int ( * proc_func )( unsigned short , void * );

/* 공통 함수 포인트 */
typedef int ( * init )( send_data_func );
typedef int ( * release )( void );
typedef int ( * idle )( void * );

/* SNS Parse 함수 포인터 */
typedef int ( * parse )( unsigned char , void * );

/* SNS 사용 여부  함수 포인터 */
typedef int ( * proc_used_func )( int );

/* main call function struct*/
typedef struct call_fucn_
{
	init 			init_func;
	release 		release_func;
	idle 			idle_func;
}call_func_t;

/* Transfer process Function struct */
typedef struct proc_func_
{
	unsigned short inter_msg;
	proc_func ptrproc;
}proc_func_t;

/* SNS function struct */
typedef struct pars_cmd_func_
{
	unsigned char cmd;
	parse  parse_func;
}parse_cmd_func_t;

/* config setting */
typedef struct session_default_value
{
	char *session;
	char *key;
	char *val;
} session_default_value_t;

typedef struct default_value
{
	char *key;
	char *val;
} default_value_t;

/* conf data */
#if defined(__linux__) || defined(__GNUC__)
typedef struct conf_data_
{
	volatile unsigned int  save_log;						/* save_log*/

}__attribute__ ((packed)) conf_data_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct conf_data_
{
	volatile unsigned int save_log;						/* save_log*/

} conf_data_t;
#pragma pack(pop)
#endif

/* rtu network data */
#if defined(__linux__) || defined(__GNUC__)
typedef struct net_info_
{
	int fcltid;									/* 설비 ID */
	char ip [ MAX_IP_SIZE ];					/* RTU IP */
	char netmask[ MAX_IP_SIZE ];				/* RTU NETMASK */
	char gateway[ MAX_IP_SIZE ];				/* RTU gateway */
	unsigned short port;
}__attribute__ ((packed)) net_info_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct net_info_
{
	int fcltid;									/* 설비  ID */
	char ip [ MAX_IP_SIZE ];					/* RTU IP */
	char netmask[ MAX_IP_SIZE ];				/* RTU NETMASK */
	char gateway[ MAX_IP_SIZE ];				/* RTU gateway */
	unsigned short port;
} net_info_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
/* version info */
typedef struct ver_info_
{
	unsigned char ob_ver[ MAX_VER_SIZE ];		/* observer version */
	unsigned char app_ver[ MAX_VER_SIZE ];		/* app version */
	unsigned char mcu_ver[ MAX_VER_SIZE ];		/* mcu version */
	unsigned char remote_ver[ MAX_VER_SIZE ];	/* remote control version */
	unsigned char temp_ver[ MAX_VER_SIZE ];		/* temprature version */
	unsigned char pwr_c_ver[ MAX_VER_SIZE ];		/* power control version */
	unsigned char pwr_m_ver[ MAX_VER_SIZE ];	/* power monitor version */
}__attribute__ ((packed)) ver_info_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
/* version info */
typedef struct ver_info_
{
	unsigned char ob_ver[ MAX_VER_SIZE ];		/* observer version */
	unsigned char app_ver[ MAX_VER_SIZE ];		/* app version */
	unsigned char mcu_ver[ MAX_VER_SIZE ];		/* mcu version */
	unsigned char remote_ver[ MAX_VER_SIZE ];	/* remote control version */
	unsigned char temp_ver[ MAX_VER_SIZE ];		/* temprature version */
	unsigned char pwr_c_ver[ MAX_VER_SIZE ];		/* power control version */
	unsigned char pwr_m_ver[ MAX_VER_SIZE ];	/* power monitor version */
} ver_info_t;
#pragma pack(pop)
#endif


#if defined(__linux__) || defined(__GNUC__)
typedef struct db_info_
{
    char szdbuser[ MAX_DB_USER_NAME_SIZE ];
    char szdbpwr [ MAX_DB_USER_PWR_SIZE ];
    char szdbname[ MAX_DB_NAME_SIZE ];
}__attribute__ ((packed)) db_info_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct db_info_
{
    char szdbuser[ MAX_DB_USER_NAME_SIZE ];
    char szdbpwr [ MAX_DB_USER_PWR_SIZE ];
    char szdbname[ MAX_DB_NAME_SIZE ];
}db_info_t;
#pragma pack(pop)
#endif


#if defined(__linux__) || defined(__GNUC__)
/* rtu info data */
typedef struct rtu_info_
{
	volatile int fms_net_sts;							/* fms server network status */
	net_info_t net;										/* rtu network 정보 */
	ver_info_t ver;										/* rtu version 정보 */
	volatile int siteid;								/* rtu site id */
	volatile int fcltid;								/* rtu rtu fcltid */
	volatile int time_cycle;							/* rtu request time interval */
	volatile int init_sts;								/* rtu init status */
	volatile int send_cycle;							/* 전송 주기 */
	volatile unsigned short op_port;					/* op listen port */
	char fms_server_ip[ MAX_IP_SIZE ];					/* fms server ip */
	volatile unsigned short fms_server_port;			/* fms server port */
}__attribute__ ((packed)) rtu_info_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
/* rtu info data */
typedef struct rtu_info_
{
	volatile int fms_net_sts;							/* fms server network status */
	net_info_t net;										/* rtu network 정보 */
	ver_info_t ver;										/* rtu version 정보 */
	volatile int siteid;								/* rtu site id */
	volatile int fcltid;								/* rtu rtu fcltid */
	volatile int time_cycle;							/* rtu request time interval */
	volatile int init_sts;								/* rtu init status */
	volatile int send_cycle;							/* 전송 주기 */
	volatile unsigned short op_port;					/* op listen port */
	char fms_server_ip[ MAX_IP_SIZE ];					/* fms server ip */
	volatile unsigned short fms_server_port;			/* fms server port */
} rtu_info_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct tid_
{
	time_t cur_time;
	int siteid;
	int sens_type;
	int seq;
	char szdata[ MAX_DBFCLT_TID ];
}tid_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct tid_
{
	time_t cur_time;
	int siteid;
	int sens_type;
	int seq;
	char szdata[ MAX_DBFCLT_TID ];
}tid_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
/* update net data */
typedef struct update_net_
{
	char szip[ MAX_IP_SIZE ];					/* fms server ip */
	volatile unsigned short port;					/* op listen port */
}__attribute__ ((packed)) update_net_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct update_net_
{
	char szip[ MAX_IP_SIZE ];					/* fms server ip */
	volatile unsigned short port;					/* op listen port */
}update_net_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct fclt_net_
{
	char szip[ MAX_IP_SIZE ];					/* fms server ip */
	unsigned short port;						/* op listen port */
	int fcltid;
}__attribute__ ((packed)) fclt_net_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct fclt_net_
{
    char szip[ MAX_IP_SIZE ];					/* fms server ip */
    unsigned short port;					/* op listen port */
	int fcltid;
} fclt_net_t;
#pragma pack(pop)
#endif

typedef struct snsr_col_nm_
{
	volatile int snsr_id;
	volatile int fclt_id;
	volatile int snsr_type;
	volatile int snsr_sub_type;
	volatile int snsr_idx;
	char szcol[ MAX_COL_NAME_SIZE ];
}snsr_col_nm_t;

typedef struct snsr_col_mng_
{
	volatile int cnt;
	//snsr_col_nm_t col_nm[ MAX_COL_NAME_CNT ];
	snsr_col_nm_t * ptrcol_nm;
}snsr_col_mng_t;

typedef struct snsr_info_
{
	int snsr_id;
	char szcol[ MAX_COL_NAME_SIZE ];
}snsr_info_t;

typedef struct sys_info_
{
	volatile float use_cpu;
	volatile float use_mem;
	volatile float use_hard;
}sys_info_t;

#define MAX_CH_VAL_SIZE			30
typedef struct web_req_
{
	volatile int site_id;
	volatile int web_msg;
	volatile int n_val1;
	volatile int n_val2;
	volatile int n_val3;
	volatile int n_val4;
	volatile int n_val5;
	char ch_val1[ MAX_CH_VAL_SIZE];
	char ch_val2[ MAX_CH_VAL_SIZE];
	char ch_val3[ MAX_CH_VAL_SIZE];
	char ch_val4[ MAX_CH_VAL_SIZE];
	char ch_val5[ MAX_CH_VAL_SIZE];
}__attribute__ ((packed)) web_req_t;


#endif

