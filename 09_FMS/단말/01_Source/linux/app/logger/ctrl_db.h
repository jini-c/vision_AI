#ifndef _CTRLDB_H__
#define _CTRLDB_H__

#include "type.h"
#include "com_def.h"
#if 0
#include "protocol.h"
#endif

int init_ctrl_db( void );
int release_ctrl_db( void );


int load_rtu_info_ctrl_db( void * ptrdata );
int load_fclt_info_ctrl_db( void * ptrdata );
int load_fclt_sns_ctrl_db( int fcltid, void * ptrdata );
int load_do_ctrl_db( int rtuid, void * ptrdata );
int load_di_ctrl_db( int rtuid, void * ptrdata );
int load_ai_ctrl_db( int rtuid, void * ptrdata );
int load_pwr_dev_ctrl_db( int snsr_sub_type, void * ptrdata );

int load_fclt_net_ctrl_db( int siteid, int fcltid, void * ptrdata );

int load_snsr_col_nm_ctrl_db( int siteid, void * ptrdata );

int load_pwr_m_data_pos_ctrl_db( int snsr_id, void * ptrdata );

int load_rtu_fclt_info_ctrl_db( void * ptrdata );

int update_rtu_site_id_ctrl_db( int site_id );

#endif


