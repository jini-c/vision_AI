#ifndef _FMS_CONTROL_H_
#define _FMS_CONTROL_H_

#include "com_sns.h"

/*================================================================================================
공용 함수로 main에서 호출함으로 다른 모듈에서 호출 하면 안됨.   
================================================================================================*/
int init_control( send_data_func ptrsend_data );
int idle_control( void * ptr );
int release_control( void );

int idle_rtu_fclt_control( void );

int get_proc_used( int proc_id );

/* util -> control */
int send_debug_data( char * ptrdata, int size );

/*================================================================================================
  Transfer layer에서 설정, 제어  데이터나 장애 데이터를 받을 수 있는 함수.
================================================================================================*/

int recv_cmd_data_transfer( unsigned short inter_msg, void * ptrdata );	
int recv_update_data_transfer( unsigned short inter_msg, void * ptrdata  );

/*================================================================================================
  sensor module에서 수집 데이터나 장애 데이터를 받을 수 있는 함수.
================================================================================================*/
int recv_sns_data_sensor( unsigned short inter_msg, void * ptrdata );	

/*================================================================================================
  websock module에서 수집 데이터나 장애 데이터를 받을 수 있는 함수.
================================================================================================*/
int recv_cmd_data_websock( unsigned short inter_msg, void * ptrdata );	

/*================================================================================================
  sensor module 함수 포인터 등록 
  ===============================================================================================*/

int set_trans_module_recv_func( send_data_func ptrfms_client_func, send_data_func ptrop_server_func ,
								send_data_func ptrupdate_func, send_data_func ptrwebsock_func  );

int set_sns_module_recv_func( send_data_func ptrradio_func, send_data_func ptrmcu_func, 
							  send_data_func ptrpwr_c_func, send_data_func ptrremote_func,
							  send_data_func ptrtemp_func,  send_data_func ptrpwr_m_func, 
							  send_data_func ptrapc_ups_func  );

/*================================================================================================
 Sensor 처리 함수 GPIO RECV 
 ================================================================================================*/
int set_gpio_data_ctrl( mcu_data_t * ptrdata   );
int set_gpio_ver_ctrl( BYTE *ptrver  );
int set_gpio_trans_sts_ctrl(int val );
int set_gpio_do_onoff_result_ctrl( result_t * ptrresult, BYTE * ptrdata );
int set_gpio_doe_onoff_result_ctrl( result_t * ptrresult, BYTE * ptrdata );

/*================================================================================================
 Sensor 처리 함수 Temprature RECV 
 ================================================================================================*/
int set_temp_data_ctrl( temp_data_t * ptrdata   );
int set_temp_ver_ctrl( BYTE *ptrver  );
int set_temp_trans_sts_ctrl(int val );

/*================================================================================================
 Sensor 처리 함수 Power Contorl  RECV 
 ================================================================================================*/
int set_pwr_c_data_ctrl( pwr_c_data_t * ptrdata   );
int set_pwr_c_ver_ctrl( BYTE *ptrver  );
int set_pwr_c_trans_sts_ctrl(int val );
int set_pwr_c_onoff_result_ctrl( result_t * ptrresult, BYTE * ptrdata );

/*================================================================================================
 Sensor 처리 함수 Power Monitor RECV 
 ================================================================================================*/
int set_pwr_m_data_ctrl( pwr_m_data_t * ptrdata   );
int set_pwr_m_ver_ctrl( BYTE *ptrver  );
int set_pwr_m_trans_sts_ctrl(int val );

int set_pwr_m_ground_ctrl( unsigned short val );

/*================================================================================================
 Sensor 처리 함수 Remote Control RECV 
 ================================================================================================*/
int set_remote_data_ctrl( remote_data_t * ptrdata   );
int set_remote_ver_ctrl( BYTE *ptrver  );
int set_remote_trans_sts_ctrl(int val );
int set_remote_onoff_result_ctrl( result_t * ptrresult, BYTE * ptrdata );

/*================================================================================================
 Sensor 처리 함수 APC UPS Control RECV 
 ================================================================================================*/
int get_apc_ups_fclt_info_ctrl( fclt_info_t * ptrfclt );
int set_apc_ups_data_ctrl( ups_data_t * ptrdata );
int set_apc_ups_trans_sts_ctrl(int val );

/*================================================================================================
 전파 감시 장치  처리 함수 radio Control RECV 
 ================================================================================================*/
int get_radio_fclt_info_ctrl( int fcltcode , fclt_info_t * ptrfclt );
int set_radio_tcp_conn_sts_ctrl( int fclt_id, int fclt_code, int sts );

int set_fre_data_ctrl( radio_data_t * ptrradio );
int set_frs_data_ctrl( radio_data_t * ptrradio );
int set_sre_data_ctrl( radio_data_t * ptrradio );
int set_fde_data_ctrl( radio_data_t * ptrradio );

/*================================================================================================
 Power Mansger 처리 함수  
 ================================================================================================*/
int get_fcltip_ctrl( int fcltcode,  char * ptrip );
int get_pwr_mthod_data_ctrl( int snsr_sub_type, pwr_mthod_mng_t * ptrmthod_mng );

int send_eqp_pwr_data_ctrl( UINT16 msg , void * ptrdata , int op_send );
int set_eqp_pwr_value_ctrl( void * ptrset );

int get_fclt_netsts_ctrl( int fcltcode );
int get_sr_netsts_ctrl( int fcltcode );

#endif
