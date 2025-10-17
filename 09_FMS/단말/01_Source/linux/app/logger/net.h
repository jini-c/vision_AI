#ifndef __NET_H__
#define __NET_H__

#define NET_DEFAULT_PORT_FMS    6000    // fms server port
#define NET_DEFAULT_PORT_FOP    6002    // op server port
#define NET_DEFAULT_PORT_RUS    6004    // update server port

#define NET_RECV_BUFFER_MAX     2048
#define NET_SEND_BUFFER_MAX     2048

#define NET_FMS_STATE_NORMAL    0
#define NET_FMS_STATE_ERRORS    1

#define NET_FMS_PACKET_QUE      256

#define NET_MAX_CLIENT          20
#define NET_MAX_EVENTS          100

#define NET_MAX_CHECKSUM        8

//////////////////////////////////////////////////////////////////////////

int net_connect_server(int port, int maxclnt);
int net_connect_client(char* szaddr, int port);
int net_connect_close(int sock);

//////////////////////////////////////////////////////////////////////////

typedef int ( * net_send_proc_func )( short , void * );
typedef int ( * net_recv_proc_func )( short , void * );

typedef struct net_packet_t_
{
    volatile short sendid;       // (2 BYTE) ecmd_rms_eqptatus_req,, 
    volatile short recvid;       // (2 BYTE) ecmd_rms_eqptatus_req,, 
    volatile long sendtime;
    volatile unsigned int timeout;
    net_recv_proc_func func;
    volatile int state; // 0:null , 1:send , 2:recv , 3:timeout
    volatile int cmd;
} net_packet_t;


typedef struct packet_rule_ {
    volatile short sendid;
    volatile short recvid;
    volatile unsigned int timeout;
    volatile int cmd;
} packet_rule_t;

// return -1 que full
int net_packet_empty_que( net_packet_t * que, int nsize );

// rtu -> fms , rtu -> op
void net_packet_rule_init(net_packet_t * packet);
int  net_packet_rule_make(net_packet_t * packet, packet_rule_t * ptrrule, int cnt);
int  net_packet_timeoutcheck(net_packet_t * packet);
int  net_packet_fms_rule_make(net_packet_t * packet);
int  net_packet_cmd_rule_make(net_packet_t * packet);
int  net_packet_update_rule_make(net_packet_t * packet);


typedef struct recv_data_buffer_ {
    BYTE buf[NET_RECV_BUFFER_MAX];
    volatile int posr;
    volatile int posw;
} recv_data_buffer_t;

typedef struct send_data_buffer_ {
    BYTE buf[NET_SEND_BUFFER_MAX];
    volatile int size;
} send_data_buffer_t;

typedef struct net_sock_
{
    volatile int clntsock;       /* client socket fds */
    char clntip[MAX_IP_SIZE];    /* client connection ip */
    volatile int clntport;
    recv_data_buffer_t recvbuf;
    send_data_buffer_t sendbuf;
    volatile int isalive;

} net_sock_t;

#if 0
const char * addr_to_string(char *str, struct in_addr addr, size_t len) {
    return inet_ntop(AF_INET, &addr, str, len);
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////

void parse_rtupk_collectrslt_aidido(fms_collectrslt_aidido_t * ptrdata);
void parse_rtupk_collectrsltai(fms_collectrsltai_t * ptrdata, int ncount);
void parse_rtupk_collectrsltdido(fms_collectrsltdido_t * ptrdata, int ncount);
void parse_rtupk_evt_faultaidi(fms_evt_faultaidi_t * ptrdata);
void parse_rtupk_faultdataai(fms_faultdataai_t * ptrdata, int ncount);
void parse_rtupk_faultdatadi(fms_faultdatadi_t * ptrdata, int ncount);
void parse_rtupk_ctl_base(fms_ctl_base_t * ptrdata);
void parse_rtupk_thrlddataai(fms_thrlddataai_t * ptrdata);
void parse_rtupk_thrlddatadi(fms_thrlddatadi_t * ptrdata);
void parse_rtupk_fclt_reqtimedata(fclt_reqtimedata_t * ptrdata);
void parse_rtupk_fclt_evt_dbdataupdate(fclt_evt_dbdataupdate_t * ptrdata);
void parse_rtupk_fclt_ctlthrldcompleted(fclt_ctlthrldcompleted_t * ptrdata);
void parse_rtupk_fclt_evt_faultfclt(fclt_evt_faultfclt_t * ptrdata);
int  parse_rtupk_get_snsrtype(void * ptrdata, int pos);

void parse_rtupk_eqp_sts_resp( fclt_ctlstatus_eqponoffrslt_t * ptrdata );
void parse_rtupk_eqp_net_sts_resp( fclt_cnnstatus_eqprslt_t * ptrdata );

//////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef int ( * send_packet_func )( void * , void * , void * );
typedef int ( * make_packet_head_func )( void * , int , int );

typedef struct net_comm_
{
    net_sock_t *            ptrsock;
    make_packet_head_func   make_header;
    send_packet_func        send_packet;
    send_data_func          send_ctlmsg;
} net_comm_t;

int ctl_sns_data_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_sns_cudata_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_sns_evt_fault(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_sns_ctl_power(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_sns_ctl_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_sns_ctl_thrld(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_sns_completed_thrld(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_eqp_data_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_eqp_cudata_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_eqp_evt_fault(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_eqp_ctl_power(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_eqp_ctl_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_eqp_ctl_thrld(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_eqp_completed_thrld(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_timedat_req(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_database_update_res(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_evt_network_err(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_eqp_sts_resp(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_eqp_net_sts_resp(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_eqp_off_opt_resp(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);
int ctl_eqp_on_opt_resp(unsigned short inter_msg, void * ptrdata, net_comm_t * ptrcomm);




int net_packet_parser(fms_packet_header_t* ptrhead, void * ptrbody, net_comm_t * ptrcomm);


//////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct conv_update_name_
{
    BYTE    code; // enumfirmware_code
    char *  name; // filename
} conv_update_name_t;

char * get_conv_update_name(BYTE code);

void set_op_cmdctl_state(int state, int idx);
int  get_op_cmdctl_state();
int  is_op_cmdctl_req(int code);

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// data base

typedef struct db_syshist_
{
    fms_elementfclt_t tfcltelem;		// 설비까지 구분
    fms_transdbdata_t tdbtans;		// rtu-svr 간 db 트랜젝션 확인을 위한 정보
    int errcd;
    char ipaddr[MAX_IP_SIZE];
    int port;
} db_syshist_t;

int net_close_db();
int net_connect_db();

// 제어 히스토리
int select_snsr_id(int nsiteid, int nfcltid, int esnsrtype, int esnsrsub, int idxsub);
int update_db_ctl_hist(void * ptrdata);
int insert_db_eqp_ctl_hist_onoffrslt(void * ptrdata, int snsr_id);
int insert_db_eqp_ctl_hist_onoff(void * ptrdata, int snsr_id);
int insert_db_eqp_ctl_hist(void * ptrdata, int snsr_id);
int insert_db_sns_ctl_hist_onoff(void * ptrdata, int snsr_id);
int insert_db_sns_ctl_hist(void * ptrdata, int snsr_id);

// ai임계치설정 , 히스토리
int update_db_aithrld(void * ptrdata);
int insert_db_aithrld(void * ptrdata, int snsr_id);
int update_db_aithrld_hist(void * ptrdata);
int insert_db_aithrld_hist(void * ptrdata, int snsr_id);

// 이벤트 히스토리
int update_db_event(void * ptrdata);
int insert_db_event(int snsrtype, void * ptrdata, int* snsr_id, int snsr_cnt, char * szdata, int ndata);

// dido임계치설정 , 히스토리
int update_db_didothrld(void * ptrdata);
int insert_db_didothrld(void * ptrdata, int snsr_id);
int update_db_didothrld_hist(void * ptrdata);
int insert_db_didothrld_hist(void * ptrdata, int snsr_id);

// ai 히스토리 (모니터링)
int update_db_ai_hist(void * ptrdata);
int insert_db_ai_hist(void * ptrdata);

// dido 히스토리 (모니터링)
int update_db_dido_hist(void * ptrdata);
int insert_db_dido_hist(void * ptrdata);

// 고정, 운영, 방탐, 준고정
int update_db_eqp_hist(void * ptrdata);
int insert_db_eqp_hist(int snsrtype, void * ptrdata);

// 고정감시 히스토리 (모니터링)
int update_db_fre_hist(void * ptrdata);
int insert_db_fre_hist(void * ptrdata);

// 고정감시운영SW 히스토리 (모니터링)
int update_db_frs_hist(void * ptrdata);
int insert_db_frs_hist(void * ptrdata);

// 고정방탐 히스토리 (모니터링)
int update_db_fde_hist(void * ptrdata);
int insert_db_fde_hist(void * ptrdata);

// 준고정 히스토리 (모니터링)
int update_db_sre_hist(void * ptrdata);
int insert_db_sre_hist(void * ptrdata);

// 시스템장애 히스토리
int update_db_sys_hist(void * ptrdata);
int insert_db_sys_hist(void * ptrdata);

#endif//__NET_H__
