#ifndef _UPDATE_H_
#define _UPDATE_H_

/*================================================================================================
공용 함수로 main에서 호출함으로 다른 모듈에서 호출 하면 안됨.(다른 파일에서 호출 하지 마세요)
================================================================================================*/
int init_update_client( send_data_func ptrsend_data );
int idle_update_client( void * ptr );
int release_update_client( void );

/*================================================================================================
  CONTROL Module에서 센서 데이터나 장애 데이터를 받을 수 있는 함수.(다른 파일에서 호출하지 마세요)
================================================================================================*/
int recv_cmd_data_update_client( unsigned short inter_msg, void * ptrdata );	


int update_connect_req(char* server_ip,  unsigned short port);

#endif
