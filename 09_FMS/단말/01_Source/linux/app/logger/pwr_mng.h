#ifndef _PWR_MNG_H_
#define _PWR_MNG_H_

/*================================================================================================
공용 함수로 main에서 호출함으로 다른 모듈에서 호출 하면 안됨.(다른 파일에서 호출 하지 마세요)
================================================================================================*/
int init_pwr_mng( void );
int idle_pwr_mng( void * ptr );
int release_pwr_mng( void );

/*================================================================================================
  CONTROL Module에서 제어나 설정 데이터를 받 을 수 있는 함수.(다른 파일에서 호출하지 마세요)
================================================================================================*/
int recv_ctrl_data_pwr_mng( unsigned short inter_msg, void * ptrdata );	

int set_ctrl_pwr_c_data_pwr_mng( void * ptrpwr_c );
int set_ctrl_mcu_etc_data_pwr_mng( unsigned char * ptrdoe  );

/*================================================================================================
	Radio 모듈에서 전원 제어 상태를 보낸다.  
================================================================================================*/
int recv_pwr_ctrl_resp_pwr_mng( unsigned short inter_msg, void * ptrdata );
int recv_pwr_ctrl_release_pwr_mng( int errcode, int err_step, int max_step );

/*================================================================================================
	2016 11 14 new protocol : on / off option   
================================================================================================*/
int recv_ctrl_data_pwr_off_opt_mng( unsigned short inter_msg, void * ptrdata );
int recv_ctrl_data_pwr_on_opt_mng( unsigned short inter_msg, void * ptrdata );
int recv_ctrl_data_eqp_status_mng( unsigned short inter_msg, void * ptrdata );

//int set_doe_remote_mcu( int val );
//int get_doe_remote_mcu( void );
#endif
