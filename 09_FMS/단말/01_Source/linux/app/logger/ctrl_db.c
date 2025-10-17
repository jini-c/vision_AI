#include "com_include.h"
#include "core_conf.h"
#include "com_sns.h"

#include <my_global.h>
#include <mysql.h>

#define MAX_SQL_SIZE 	400
static db_info_t g_db_info;

static int close_db( MYSQL * ptrconn )
{
	if ( ptrconn != NULL )
	{
		mysql_close( ptrconn );
		ptrconn = NULL;
	}

	return ERR_SUCCESS;
}

static int db_connect ( MYSQL * ptrconn )
{
	int ret = ERR_SUCCESS;
    
    if ( strlen( g_db_info.szdbuser ) == 0 ||
         strlen( g_db_info.szdbuser ) == 0 ||
         strlen( g_db_info.szdbuser ) == 0  )
    {
        print_dbg( DBG_ERR, 1, "Invalid DB Info User:%s, Pwr:%s, Name:%s",
                                g_db_info.szdbuser, g_db_info.szdbpwr, g_db_info.szdbname );
        return ERR_RETURN;
    }
#if 0
    print_dbg( DBG_INFO, 1, "DB Info User:%s, Pwr:%s, Name:%s",
            g_db_info.szdbuser, g_db_info.szdbpwr, g_db_info.szdbname );
#endif

	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "is null ptrconn [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	if (mysql_real_connect( ptrconn, "127.0.0.1", g_db_info.szdbuser, g_db_info.szdbpwr, g_db_info.szdbname, 3306, NULL, 0) == NULL)
	//if (mysql_real_connect( ptrconn, "127.0.0.1", "fmsuser", "fms123", "FMSDB", 3306, NULL, 0) == NULL)
	{
		print_dbg( DBG_ERR,1, "Cannot Connect LocalDB [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	return ret ;
}

static int inter_load_rtu_info( void * ptrdata )
{
	int ret = ERR_SUCCESS;
	rtu_info_t * ptrrtu = NULL ;

	MYSQL * ptrconn = NULL;
	MYSQL_RES * ptrsql_result = NULL;
	MYSQL_ROW row;

	char szsql[ MAX_SQL_SIZE ];
	int i,cnt = 0;
	char * ptrrow = NULL;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrrtu = ( rtu_info_t * )ptrdata;
	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* rtu info */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"select FCLT_ID,SITE_ID, IP, PORT, SEND_CYCLE, UTCK_CYCLE, GATEWAY, SUBNETMASK  \
					from TFMS_FCLT_MST \
					where SITE_ID = %04d and FCLT_CD = 001 ",
			       ptrrtu->siteid );

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrsql_result = mysql_store_result( ptrconn );	

	if ( ptrsql_result == NULL )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* column count */
	cnt = mysql_num_fields( ptrsql_result);

	while ((row = mysql_fetch_row( ptrsql_result))) 
	{ 
		for(i = 0; i < cnt ; i++) 
		{ 
			/* start new row */
			if ( i == 0 )
			{
				//printf("\r\n");
			}

			ptrrow = row[ i ];
			
			//printf("======i:%d==ptrrow:%s................\r\n",i, ptrrow );

			if ( i == 0 && ptrrow != NULL )
			{
				ptrrtu->fcltid = atoi( ptrrow );
			}
			else if ( i == 2 && ptrrow != NULL )
			{
				memcpy( ptrrtu->net.ip, ptrrow , strlen( ptrrow ));
			}
			else if ( i == 3 && ptrrow != NULL )
			{
				ptrrtu->op_port = atoi( ptrrow );
			}
			else if ( i == 4 && ptrrow != NULL )
			{
				ptrrtu->send_cycle = atoi( ptrrow );
			}
			else if ( i == 5 && ptrrow != NULL )
			{
				ptrrtu->time_cycle= atoi( ptrrow );
			}
			else if ( i == 6 && ptrrow != NULL )
			{
				memcpy( ptrrtu->net.gateway,ptrrow , strlen( ptrrow ));
			}
			else if ( i == 7 && ptrrow != NULL )
			{
				memcpy( ptrrtu->net.netmask,ptrrow , strlen( ptrrow ));
			}
		} 
		//printf("\n"); 
	}

	/* fms server info  */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"select IP, PORT \
					from TFMS_FCLT_MST \
					where FCLT_CD = 004 ");

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}
	
	ptrsql_result = mysql_store_result( ptrconn );	

	if ( ptrsql_result == NULL )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}
	
	/* column count */
	cnt = mysql_num_fields( ptrsql_result);

	while ((row = mysql_fetch_row( ptrsql_result))) 
	{ 
		for(i = 0; i < cnt ; i++) 
		{ 
			/* start new row */
			if ( i == 0 )
			{
				//printf("\r\n");
			}

			ptrrow = row[ i ];
			
	        //printf("======i:%d==ptrrow:%s................\r\n",i, ptrrow );

			if ( i == 0 && ptrrow != NULL )
			{
				memcpy( ptrrtu->fms_server_ip, ptrrow , strlen( ptrrow ));
			}
			else if ( i == 1 && ptrrow != NULL )
			{
				ptrrtu->fms_server_port= atoi( ptrrow );
			}
		} 
		//printf("\n"); 
	}
	mysql_close( ptrconn );
	
	return ret ;

}

static int inter_load_fclt_info( void * ptrdata )
{
	int ret = ERR_SUCCESS;
	fclt_info_t * ptrfclt = NULL ;

	MYSQL * ptrconn = NULL;
	MYSQL_RES * ptrsql_result = NULL;
	MYSQL_ROW row;

	char szsql[ MAX_SQL_SIZE ];
	int i,cnt = 0;
	char * ptrrow = NULL;
	int siteid = 0;
	int row_cnt = 0;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrfclt= ( fclt_info_t* )ptrdata;
	siteid  = get_my_siteid_shm();

	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* rtu info */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"select FCLT_ID, FCLT_CD, IP, PORT, SEND_CYCLE, NOTICE  \
					from TFMS_FCLT_MST \
					where SITE_ID = %04d and FCLT_CD in ( %03d, %03d, %03d, %03d )",
			       siteid , EFCLT_EQP01,EFCLT_EQP01+1, EFCLT_EQP01+ 2 , EFCLT_EQP01+3 );

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrsql_result = mysql_store_result( ptrconn );	

	if ( ptrsql_result == NULL )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* column count */
	cnt = mysql_num_fields( ptrsql_result);
	
	//MAX_FCLT_CNT 
	while ((row = mysql_fetch_row( ptrsql_result))) 
	{ 
		if ( row_cnt < MAX_FCLT_CNT )
		{
			for(i = 0; i < cnt ; i++) 
			{ 

				ptrrow = row[ i ];
				//printf("======i:%d==ptrrow:%s................\r\n",i, ptrrow );
				if ( ptrrow != NULL && i == 0 )
				{
					ptrfclt[ row_cnt ].fcltid = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 1 )
				{
					ptrfclt[ row_cnt ].fcltcode = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 2 )
				{
					memcpy( &ptrfclt[ row_cnt ].szip , ptrrow, strlen( ptrrow ) );
				}
				else if ( ptrrow != NULL && i == 3 )
				{
					ptrfclt[ row_cnt ].port= atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 4 )
				{
					ptrfclt[ row_cnt ].send_cycle = atoi( ptrrow ) * 60;
				}
				else if ( ptrrow != NULL && i == 5 )
				{
					ptrfclt[ row_cnt ].gather_cycle = atoi( ptrrow ) * 60;
				}
			}
			row_cnt++;
		}
		//printf("\n"); 
	}

	mysql_close( ptrconn );
	
	return ret ;

}

static int inter_load_rtu_fclt_info( void * ptrdata )
{
	int ret = ERR_SUCCESS;
	fclt_info_t * ptrfclt = NULL ;

	MYSQL * ptrconn = NULL;
	MYSQL_RES * ptrsql_result = NULL;
	MYSQL_ROW row;

	char szsql[ MAX_SQL_SIZE ];
	int i,cnt = 0;
	char * ptrrow = NULL;
	int siteid = 0;
	int row_cnt = 0;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrfclt= ( fclt_info_t* )ptrdata;
	siteid  = get_my_siteid_shm();

	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* rtu info */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"select FCLT_ID, FCLT_CD, IP, PORT, SEND_CYCLE, NOTICE  \
					from TFMS_FCLT_MST \
					where SITE_ID = %04d and FCLT_CD NOT in ( %03d, %03d, %03d, %03d, %03d )",
			       siteid , EFCLT_RTU, EFCLT_EQP01,EFCLT_EQP01+1, EFCLT_EQP01+ 2 , EFCLT_EQP01+3 );

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrsql_result = mysql_store_result( ptrconn );	

	if ( ptrsql_result == NULL )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* column count */
	cnt = mysql_num_fields( ptrsql_result);
	
	//MAX_FCLT_CNT 
	while ((row = mysql_fetch_row( ptrsql_result))) 
	{ 
		if ( row_cnt < MAX_FCLT_CNT )
		{
			for(i = 0; i < cnt ; i++) 
			{ 

				ptrrow = row[ i ];
				//printf("======i:%d==ptrrow:%s................\r\n",i, ptrrow );
				if ( ptrrow != NULL && i == 0 )
				{
					ptrfclt[ row_cnt ].fcltid = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 1 )
				{
					ptrfclt[ row_cnt ].fcltcode = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 2 )
				{
					memcpy( &ptrfclt[ row_cnt ].szip , ptrrow, strlen( ptrrow ) );
				}
				else if ( ptrrow != NULL && i == 3 )
				{
					ptrfclt[ row_cnt ].port= atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 4 )
				{
					ptrfclt[ row_cnt ].send_cycle = atoi( ptrrow ) * 60;
				}
				else if ( ptrrow != NULL && i == 5 )
				{
					ptrfclt[ row_cnt ].gather_cycle = atoi( ptrrow ) * 60;
				}
			}
			row_cnt++;
		}
		//printf("\n"); 
	}

	mysql_close( ptrconn );
	
	return ret ;

}

static int inter_load_fclt_sns(int fcltid, void * ptrdata )
{
	int ret = ERR_SUCCESS;
	radio_val_manager_t * ptrradio = NULL ;

	MYSQL * ptrconn = NULL;
	MYSQL_RES * ptrsql_result = NULL;
	MYSQL_ROW row;

	char szsql[ MAX_SQL_SIZE ];
	int i,cnt = 0;
	char * ptrrow = NULL;
	int siteid = 0;
	int row_cnt = 0;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrradio = ( radio_val_manager_t * )ptrdata;
	siteid  = get_my_siteid_shm();

	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* rtu info */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"select SN.FCLT_ID, SN.SNSR_ID, SN.SNSR_NO, SN.SNSR_IDX, SN.SNSR_TYPE,  \
					SN.SNSR_SUB_TYPE, SN.ALARM, DI.EVT_GRADE_CD, DI.NORMAL , DI.LABEL0, DI.LABEL1,SN.DURATION_TIME\
					from TFMS_SNSR_MST SN \
					left join TFMS_DIDOTHRLD_DTL DI on SN.SNSR_ID = DI.SNSR_ID \
					where SN.site_id = %04d and SN.fclt_id = %04d and SN.USE_YN = 'Y'",
			       siteid , fcltid);

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot [FCLT SNS] SiteID:%d, FcltID:%d[%s:%d:%s]", siteid, fcltid, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrsql_result = mysql_store_result( ptrconn );	

	if ( ptrsql_result == NULL )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* column count */
	cnt = mysql_num_fields( ptrsql_result);
	
	//MAX_FCLT_CNT 
	while ((row = mysql_fetch_row( ptrsql_result))) 
	{ 
		if ( row_cnt < MAX_RADIO_SNS_CNT )
		{
			for(i = 0; i < cnt ; i++) 
			{ 
				ptrrow = row[ i ];
				//printf("======i:%d==ptrrow:%s................\r\n",i, ptrrow );
				if ( ptrrow != NULL && i == 0 )
				{
					ptrradio->radio_val[ row_cnt ].fclt_id = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 1 )
				{
					ptrradio->radio_val[ row_cnt ].snsr_id = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 2 )
				{
					ptrradio->radio_val[ row_cnt ].snsr_no = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 3 )
				{
					ptrradio->radio_val[ row_cnt ].snsr_sub_idx = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 4 )
				{
					ptrradio->radio_val[ row_cnt ].snsr_type = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 5 )
				{
					ptrradio->radio_val[ row_cnt ].snsr_sub_type = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 6 )
				{
					memcpy( ptrradio->radio_val[ row_cnt ].alarm_yn, ptrrow,strlen( ptrrow) );
				}
				else if ( ptrrow != NULL && i == 7 )
				{
					ptrradio->radio_val[ row_cnt ].grade = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 8 )
				{
					ptrradio->radio_val[ row_cnt ].good_val = atoi( ptrrow );
					ptrradio->radio_val[ row_cnt ].last_sts = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 9 )
				{
					memcpy( ptrradio->radio_val[ row_cnt ].szlabel0, ptrrow,strlen( ptrrow) );
				}
				else if ( ptrrow != NULL && i == 10 )
				{
					memcpy( ptrradio->radio_val[ row_cnt ].szlabel1, ptrrow,strlen( ptrrow) );
				}
				else if ( ptrrow != NULL && i == 11 )
				{
					ptrradio->radio_val[ row_cnt ].alarm_keep_time = atoi( ptrrow );
				}

			}
			row_cnt++;
		}
	}
	ptrradio->cnt = row_cnt;
	mysql_close( ptrconn );
	return ret ;

}

static int inter_load_do(int rtuid, void * ptrdata )
{
	int ret = ERR_SUCCESS;
	do_val_t * ptrdo= NULL ;
	int max_cnt = MAX_GPIO_CNT * 2;

	MYSQL * ptrconn = NULL;
	MYSQL_RES * ptrsql_result = NULL;
	MYSQL_ROW row;

	char szsql[ MAX_SQL_SIZE ];
	int i,cnt = 0;
	char * ptrrow = NULL;
	int siteid = 0;
	int row_cnt = 0;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrdo = ( do_val_t *)ptrdata;
	siteid  = get_my_siteid_shm();

	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* rtu info */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"select SN.FCLT_ID, SN.SNSR_ID, SN.SNSR_NO, SN.SNSR_IDX, SN.SNSR_TYPE,  \
					SN.SNSR_SUB_TYPE, SN.ALARM, DI.EVT_GRADE_CD, DI.NORMAL, DI.LABEL0, DI.LABEL1 \
					from TFMS_SNSR_MST SN \
					left join TFMS_DIDOTHRLD_DTL DI on SN.SNSR_ID = DI.SNSR_ID \
					where SN.site_id = %04d and SN.fclt_id = %04d and SN.SNSR_TYPE= %02d and SN.USE_YN = 'Y' ",
			       siteid , rtuid, ESNSRTYP_DO);

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot [DO] SiteID:%d, rtuID:%d, SnsrType:%d [%s:%d:%s]", siteid, rtuid, ESNSRTYP_DI, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrsql_result = mysql_store_result( ptrconn );	

	if ( ptrsql_result == NULL )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* column count */
	cnt = mysql_num_fields( ptrsql_result);
	
	//MAX_FCLT_CNT 
	while ((row = mysql_fetch_row( ptrsql_result))) 
	{ 
		if ( row_cnt < max_cnt )
		{
			for(i = 0; i < cnt ; i++) 
			{ 
				ptrrow = row[ i ];
				//printf("======i:%d==ptrrow:%s................\r\n",i, ptrrow );

				if ( ptrrow != NULL && i == 0 )
				{
					ptrdo[ row_cnt ].fcltid = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 1 )
				{
					ptrdo[ row_cnt ].snsr_id = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 2 )
				{
					ptrdo[ row_cnt ].snsr_no = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 3 )
				{
					ptrdo[ row_cnt ].snsr_sub_idx = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 4 )
				{
					ptrdo[ row_cnt ].snsr_type = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 5 )
				{
					ptrdo[ row_cnt ].snsr_sub_type = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 6 )
				{
					memcpy( &ptrdo[ row_cnt ].alarm_yn, ptrrow,strlen( ptrrow) );
				}
				else if ( ptrrow != NULL && i == 7 )
				{
					ptrdo[ row_cnt ].grade = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 8 )
				{
					ptrdo[ row_cnt ].good_val = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 9 )
				{
					memcpy( &ptrdo[ row_cnt ].szlabel0, ptrrow,strlen( ptrrow) );
				}
				else if ( ptrrow != NULL && i == 10 )
				{
					memcpy( &ptrdo[ row_cnt ].szlabel1, ptrrow,strlen( ptrrow) );
				}
			}
			row_cnt++;
		}
	}
	mysql_close( ptrconn );
	return ret ;
}


static int inter_load_di(int rtuid, void * ptrdata )
{
	int ret = ERR_SUCCESS;
	di_val_t * ptrdi= NULL ;
	int max_cnt = MAX_GPIO_CNT * 2;

	MYSQL * ptrconn = NULL;
	MYSQL_RES * ptrsql_result = NULL;
	MYSQL_ROW row;

	char szsql[ MAX_SQL_SIZE ];
	int i,cnt = 0;
	char * ptrrow = NULL;
	int siteid = 0;
	int row_cnt = 0;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrdi = ( di_val_t *)ptrdata;
	siteid  = get_my_siteid_shm();

	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* rtu info */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"select SN.FCLT_ID, SN.SNSR_ID, SN.SNSR_NO, SN.SNSR_IDX, SN.SNSR_TYPE,  \
	SN.SNSR_SUB_TYPE, SN.ALARM, DI.EVT_GRADE_CD, DI.NORMAL,SN.IP, SN.PORT, DI.LABEL0, DI.LABEL1 , SN.DURATION_TIME \
	from TFMS_SNSR_MST SN \
	left join TFMS_DIDOTHRLD_DTL DI on SN.SNSR_ID = DI.SNSR_ID \
	where SN.site_id = %04d and SN.fclt_id = %04d and SN.SNSR_TYPE= %02d and SN.USE_YN = 'Y' ",
			       siteid , rtuid, ESNSRTYP_DI);

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot [DI] SiteID:%d, rtuID:%d, SnsrType:%d [%s:%d:%s]", siteid, rtuid, ESNSRTYP_DI, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrsql_result = mysql_store_result( ptrconn );	

	if ( ptrsql_result == NULL )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* column count */
	cnt = mysql_num_fields( ptrsql_result);
	
	//MAX_FCLT_CNT 
	while ((row = mysql_fetch_row( ptrsql_result))) 
	{ 
		if ( row_cnt < max_cnt )
		{
			for(i = 0; i < cnt ; i++) 
			{ 
				ptrrow = row[ i ];
				//printf("======i:%d==ptrrow:%s................\r\n",i, ptrrow );

				if ( ptrrow != NULL && i == 0 )
				{
					ptrdi[ row_cnt ].fcltid = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 1 )
				{
					ptrdi[ row_cnt ].snsr_id = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 2 )
				{
					ptrdi[ row_cnt ].snsr_no = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 3 )
				{
					ptrdi[ row_cnt ].snsr_sub_idx = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 4 )
				{
					ptrdi[ row_cnt ].snsr_type = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 5 )
				{
					ptrdi[ row_cnt ].snsr_sub_type = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 6 )
				{
					memcpy( &ptrdi[ row_cnt ].alarm_yn, ptrrow,strlen( ptrrow) );
				}
				else if ( ptrrow != NULL && i == 7 )
				{
					ptrdi[ row_cnt ].grade = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 8 )
				{
					ptrdi[ row_cnt ].good_val = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 9 )
				{
					memcpy( &ptrdi[ row_cnt ].szip, ptrrow,strlen( ptrrow) );
				}
				else if ( ptrrow != NULL && i == 10 )
				{
					memcpy( &ptrdi[ row_cnt ].szport, ptrrow,strlen( ptrrow) );
				}
				else if ( ptrrow != NULL && i == 11 )
				{
					memcpy( &ptrdi[ row_cnt ].szlabel0, ptrrow,strlen( ptrrow) );
				}
				else if ( ptrrow != NULL && i == 12 )
				{
					memcpy( &ptrdi[ row_cnt ].szlabel1, ptrrow,strlen( ptrrow) );
				}
				else if ( ptrrow != NULL && i == 13 )
				{
					ptrdi[ row_cnt ].alarm_keep_time = atoi( ptrrow );
				}
			}
			row_cnt++;
		}
	}
	mysql_close( ptrconn );
	return ret ;
}

static int inter_load_ai(int rtuid, void * ptrdata )
{
	int ret = ERR_SUCCESS;
	temp_val_t * ptrai= NULL ;

	MYSQL * ptrconn = NULL;
	MYSQL_RES * ptrsql_result = NULL;
	MYSQL_ROW row;

	char szsql[ 500 ]; 
	int i,cnt = 0;
	char * ptrrow = NULL;
	int siteid = 0;
	int row_cnt = 0;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrai = ( temp_val_t *)ptrdata;
	siteid  = get_my_siteid_shm();

	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* rtu info */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"select SN.FCLT_ID, SN.SNSR_ID, SN.SNSR_NO, SN.SNSR_IDX, SN.SNSR_TYPE,  \
					SN.SNSR_SUB_TYPE, SN.ALARM, AI.MIN_VAL, AI.MAX_VAL, AI.OFFSET, \
					AI.DA_LOW, AI.WA_LOW, AI.CA_LOW, AI.CA_HIGH, AI.WA_HIGH, AI.DA_HIGH, SN.DURATION_TIME,SN.IP, SN.PORT  \
					from TFMS_SNSR_MST SN \
					left join TFMS_AITHRLD_DTL AI on SN.SNSR_ID = AI.SNSR_ID \
					where SN.site_id = %04d and SN.fclt_id = %04d and SN.SNSR_TYPE= %02d and SN.USE_YN = 'Y' ",
			       siteid , rtuid, ESNSRTYP_AI);

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot [AI]%s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrsql_result = mysql_store_result( ptrconn );	

	if ( ptrsql_result == NULL )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* column count */
	cnt = mysql_num_fields( ptrsql_result);
	
	//MAX_FCLT_CNT 
	while ((row = mysql_fetch_row( ptrsql_result ))) 
	{ 
		if ( row_cnt < MAX_GRP_RSLT_AI )
		{
			for(i = 0; i < cnt ; i++) 
			{ 
				ptrrow = row[ i ];
				//printf("======i:%d==ptrrow:%s................\r\n",i, ptrrow );
				if ( ptrrow != NULL && i == 0 )
				{
					ptrai[ row_cnt ].fcltid = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 1 )
				{
					ptrai[ row_cnt ].snsr_id = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 2 )
				{
					ptrai[ row_cnt ].snsr_no = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 3 )
				{
					ptrai[ row_cnt ].snsr_sub_idx = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 4 )
				{
					ptrai[ row_cnt ].snsr_type = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 5 )
				{
					ptrai[ row_cnt ].snsr_sub_type = atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 6 )
				{
					memcpy( &ptrai[ row_cnt ].alarm_yn, ptrrow,strlen( ptrrow) );
				}
				else if ( ptrrow != NULL && i == 7 )
				{
					ptrai[ row_cnt ].min_val = atof( ptrrow );
				}
				else if ( ptrrow != NULL && i == 8 )
				{
					ptrai[ row_cnt ].max_val = atof( ptrrow );
				}
				else if ( ptrrow != NULL && i == 9 )
				{
					ptrai[ row_cnt ].offset_val = atof( ptrrow );
				}

				else if ( ptrrow != NULL && i == 10 )
				{
					ptrai[ row_cnt ].range_val[ 0 ] = atof( ptrrow );	/* DA_LOW */
				}
				else if ( ptrrow != NULL && i == 11 )
				{
					ptrai[ row_cnt ].range_val[ 1 ] = atof( ptrrow );	/* WA_LOW */
				}
				else if ( ptrrow != NULL && i == 12 )
				{
					ptrai[ row_cnt ].range_val[ 2 ] = atof( ptrrow );	/* CA_LOW */
				}
				else if ( ptrrow != NULL && i == 13 )
				{
					ptrai[ row_cnt ].range_val[ 3 ] = atof( ptrrow );	/* CA_HIGH */
				}
				else if ( ptrrow != NULL && i == 14 )
				{
					ptrai[ row_cnt ].range_val[ 4 ] = atof( ptrrow );	/* WA_HIGH */
				}
				else if ( ptrrow != NULL && i == 15 )
				{
					ptrai[ row_cnt ].range_val[ 5 ] = atof( ptrrow );	/* DA_HIGH */
				}
				else if ( ptrrow != NULL && i == 16 )
				{
					ptrai[ row_cnt ].alarm_keep_time= atoi( ptrrow );
				}
				else if ( ptrrow != NULL && i == 17)
				{
					memcpy( &ptrai[ row_cnt ].szip, ptrrow,strlen( ptrrow) );
				}
				else if ( ptrrow != NULL && i == 18 )
				{
					memcpy( &ptrai[ row_cnt ].szport, ptrrow,strlen( ptrrow) );
				}

			}
			row_cnt++;
		}
	}

	mysql_close( ptrconn );
	return ret ;
}

static int inter_load_pwr_m_data_pos( int snsr_id, int * ptrpos )
{
	int ret = ERR_SUCCESS;

	MYSQL * ptrconn = NULL;
	MYSQL_RES * ptrsql_result = NULL;
	MYSQL_ROW row;
	int pos = -1;

	char szsql[ MAX_SQL_SIZE ];
	int i,cnt = 0;
	char * ptrrow = NULL;
	int siteid = 0;

	if ( ptrpos == NULL )
	{
		print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	siteid  = get_my_siteid_shm();

	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* rtu info */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"select S01_POS from TFMS_SNSRMETHOD_MST \
					where SITE_ID = %04d and SNSR_ID = %04d",
			       siteid , snsr_id );

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrsql_result = mysql_store_result( ptrconn );	

	if ( ptrsql_result == NULL )
	{
		close_db( ptrconn );
		print_dbg( DBG_ERR,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* column count */
	cnt = mysql_num_fields( ptrsql_result);
	
	//MAX_FCLT_CNT 
	while ((row = mysql_fetch_row( ptrsql_result))) 
	{ 
		for(i = 0; i < cnt ; i++) 
		{ 
			ptrrow = row[ i ];
			//printf("======i:%d==ptrrow:%s................\r\n",i, ptrrow );
			if ( ptrrow != NULL && i == 0 )
			{
				pos = (int)ptrrow[ 0 ];
				//printf("========================================== data pos :%d,%d,%d\r\n", siteid, snsr_id, pos );
			}
		}
	}

	*ptrpos = pos;

	mysql_close( ptrconn );
	sys_sleep( 200 );

	return ret ;

}

static int cal_pos( int snsr_sub_type, int *ptrpos )
{
	int pos = * ptrpos;

	/* 고정 , 고정 방탐은 위치를 -1 해준다 */
	if ( snsr_sub_type != ESNSRSUB_EQPSRE_POWER )
	{
		pos -= 1;
	}

	if ( pos < 0 )
	{
		pos = 0;
	}
	*ptrpos = pos;

	return ERR_SUCCESS;
}
static int inter_load_pwr_dev(int snsr_sub_type, void * ptrdata )
{
	int ret = ERR_SUCCESS;
	pwr_dev_val_t * ptrdev= NULL ;

	MYSQL * ptrconn = NULL;
	MYSQL_RES * ptrsql_result = NULL;
	MYSQL_ROW row;

	char szsql[ 500 ]; 
	int i,cnt = 0;
	char * ptrrow = NULL;
	int siteid = 0;
	int port =0;
	int pos , mthod_cnt;
	int flag = 0;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrdev = ( pwr_dev_val_t *)ptrdata;
	siteid  = get_my_siteid_shm();

	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* rtu info */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"select SN.SNSR_ID, SN.SNSR_NO, SN.SNSR_TYPE, SN.SNSR_IDX, \
					SN.SNSR_SUB_TYPE, SN.PORT, \
					S01_POS, S02_POS, S03_POS, S04_POS, S05_POS, S06_POS,S07_POS,S08_POS \
					from TFMS_SNSR_MST SN \
					left join TFMS_SNSRMETHOD_MST MT on SN.SNSR_ID = MT.SNSR_ID \
					WHERE SN.SITE_ID = %04d AND SN.SNSR_SUB_TYPE = %03d and SN.USE_YN = 'Y'",
			       siteid, snsr_sub_type );

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_INFO,1, "Cannot Select PWR DEV SiteID:%d, SNSR_SUB_TYPE:%d [%s:%d:%s]",siteid, snsr_sub_type, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrsql_result = mysql_store_result( ptrconn );	

	if ( ptrsql_result == NULL )
	{
		close_db( ptrconn );
		print_dbg( DBG_INFO,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* column count */
	cnt = mysql_num_fields( ptrsql_result);
	mthod_cnt = 0;

	//MAX_FCLT_CNT 
	while ((row = mysql_fetch_row( ptrsql_result ))) 
	{ 
		flag = 1;
		for(i = 0; i < cnt ; i++) 
		{ 
			ptrrow = row[ i ];
			//printf("======i:%d==ptrrow:%s................\r\n",i, ptrrow );
			if ( ptrrow != NULL && i == 0 )
			{
				ptrdev->snsr_id = atoi( ptrrow );
			}
			else if ( ptrrow != NULL && i == 1 )
			{
				ptrdev->snsr_no = atoi( ptrrow );
			}
			else if ( ptrrow != NULL && i == 2 )
			{
				ptrdev->snsr_type = atoi( ptrrow );
			}
			else if ( ptrrow != NULL && i == 3 )
			{
				ptrdev->snsr_sub_idx = atoi( ptrrow );
			}
			else if ( ptrrow != NULL && i == 4 )
			{
				ptrdev->snsr_sub_type = atoi( ptrrow );
			}
			else if ( ptrrow != NULL && i == 5 )
			{
				if ( strcmp( "COM1", ptrrow ) == 0 )
					port = COM1;
				if ( strcmp( "COM2", ptrrow ) == 0 )
					port = COM2;
				if ( strcmp( "COM3", ptrrow ) == 0 )
					port = COM3;
				if ( strcmp( "COM4", ptrrow ) == 0 )
					port = COM4;
				if ( strcmp( "COM5", ptrrow ) == 0 )
					port = COM5;
				if ( strcmp( "COM6", ptrrow ) == 0 )
					port = COM6;
				ptrdev->snsr_comport = port;
			}
			else if ( (ptrrow != NULL && i == 6)  ||	/* S01 */
					  (ptrrow != NULL && i == 7)  || 	/* S02 */
					  (ptrrow != NULL && i == 8)  || 	/* S03 */
					  (ptrrow != NULL && i == 9)  || 	/* S04 */
					  (ptrrow != NULL && i == 10)  || 	/* S05 */
					  (ptrrow != NULL && i == 11)  || 	/* S06 */
					  (ptrrow != NULL && i == 12)  || 	/* S07 */
					  (ptrrow != NULL && i == 13) ) 	/* S08 */
			{
				pos = atoi( ptrrow );

				if ( pos > 0 )
				{
					cal_pos( snsr_sub_type, &pos );
					ptrdev->pwr_mthod_mng.dev[ mthod_cnt ].dev_pos = pos;
					ptrdev->pwr_mthod_mng.dev[ mthod_cnt ].comport = port;
					mthod_cnt++;
				}
			}
		}
	}

	ptrdev->pwr_mthod_mng.cnt = mthod_cnt;
	mysql_close( ptrconn );

	/* 데이터가 없을 경우 에러로 처리한다 */
	if ( flag == 0 )
		ret = ERR_RETURN;
	return ret ;
}

static int inter_load_fclt_net(int siteid, int fcltid, void * ptrdata )
{
	int ret = ERR_SUCCESS;
	fclt_net_t* ptrnet= NULL ;

	MYSQL * ptrconn = NULL;
	MYSQL_RES * ptrsql_result = NULL;
	MYSQL_ROW row;

	char szsql[ 500 ]; 
	int i,cnt = 0;
	char * ptrrow = NULL;
	int flag = 0;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrnet = ( fclt_net_t *)ptrdata;
	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* rtu info */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"select IP, PORT, FCLT_ID \
					from TFMS_FCLT_MST \
					WHERE SITE_ID = %04d AND FCLT_ID = %04d ",
			       siteid, fcltid );

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_INFO,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrsql_result = mysql_store_result( ptrconn );	

	if ( ptrsql_result == NULL )
	{
		close_db( ptrconn );
		print_dbg( DBG_INFO,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* column count */
	cnt = mysql_num_fields( ptrsql_result);

	//MAX_FCLT_CNT 
	while ((row = mysql_fetch_row( ptrsql_result ))) 
	{ 
		flag = 1;
		for(i = 0; i < cnt ; i++) 
		{ 
			ptrrow = row[ i ];
			//printf("======i:%d==ptrrow:%s................\r\n",i, ptrrow );
			if ( ptrrow != NULL && i == 0 )
			{
				memcpy( ptrnet->szip, ptrrow, strlen( ptrrow ));
			}
			else if ( ptrrow != NULL && i == 1 )
			{
				ptrnet->port = atoi( ptrrow );
			}
			else if ( ptrrow != NULL && i == 2 )
			{
				ptrnet->fcltid= atoi( ptrrow );
			}
		}
        break;
	}

	mysql_close( ptrconn );

	/* 데이터가 없을 경우 에러로 처리한다 */
	if ( flag == 0 )
		ret = ERR_RETURN;

	return ret ;
}

int inter_load_snsr_col_nm( int siteid, void * ptrdata )
{
	int ret = ERR_SUCCESS;
	snsr_col_mng_t * ptrmng = NULL ;
	snsr_col_nm_t * ptrcol = NULL;

	MYSQL * ptrconn = NULL;
	MYSQL_RES * ptrsql_result = NULL;
	MYSQL_ROW row;

	char szsql[ 500 ]; 
	int i,cnt = 0;
	char * ptrrow = NULL;
	int flag = 0;
	int row_cnt = 0;

	if ( ptrdata == NULL )
	{
		print_dbg( DBG_ERR,1, "ptrdata is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	ptrmng = ( snsr_col_mng_t *)ptrdata;
	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* rtu info */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"select SNSR_ID, FCLT_ID, SNSR_TYPE, SNSR_SUB_TYPE, SNSR_IDX, COL_NM  \
					from TFMS_SNSR_MST  \
					where SITE_ID = %04d and USE_YN ='Y' ",
			       siteid);

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_INFO,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	ptrsql_result = mysql_store_result( ptrconn );	

	if ( ptrsql_result == NULL )
	{
		close_db( ptrconn );
		print_dbg( DBG_INFO,1, "Cannot Result[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* column count */
	cnt = mysql_num_fields( ptrsql_result);

	//MAX_FCLT_CNT 
	
	ptrcol = (snsr_col_nm_t*)ptrmng->ptrcol_nm;
	while ((row = mysql_fetch_row( ptrsql_result ))) 
	{ 
		flag = 1;

		if ( row_cnt < MAX_COL_NAME_CNT )
		{
			if ( ptrcol != NULL )
			{
				for(i = 0; i < cnt ; i++) 
				{ 
					ptrrow = row[ i ];
				//	printf("======i:%d==ptrrow:%s................\r\n",i, ptrrow );
					if ( ptrrow != NULL && i == 0 )
					{
						ptrcol->snsr_id = atoi( ptrrow );
					}
					else if ( ptrrow != NULL && i == 1 )
					{
						ptrcol->fclt_id = atoi( ptrrow );
					}
					else if ( ptrrow != NULL && i == 2 )
					{
						ptrcol->snsr_type= atoi( ptrrow );
					}
					else if ( ptrrow != NULL && i == 3 )
					{
						ptrcol->snsr_sub_type= atoi( ptrrow );
					}
					else if ( ptrrow != NULL && i == 4 )
					{
						ptrcol->snsr_idx = atoi( ptrrow );
					}
					else if ( ptrrow != NULL && i == 5 )
					{
						memset( ptrcol->szcol, 0, MAX_COL_NAME_SIZE );
						memcpy( ptrcol->szcol, ptrrow, strlen( ptrrow ));
					}
				}
				ptrcol++;
				row_cnt++;
			}
		}
	}

	ptrmng->cnt = row_cnt;

	mysql_close( ptrconn );

	/* 데이터가 없을 경우 에러로 처리한다 */
	if ( flag == 0 )
		ret = ERR_RETURN;

	return ret ;

}
int inter_update_rtu_site_id( int siteid )
{
	int ret = ERR_SUCCESS;
	MYSQL * ptrconn = NULL;
	char szsql[ 500 ]; 
	int org_siteid = 0;

	if ( siteid == 0 )
	{
		print_dbg( DBG_ERR,1, "siteid is null [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	if (get_rtu_run_mode_shm() != RTU_RUN_ALONE )
	{
		print_dbg( DBG_SAVE,1, "is not RTU RUN ALONE MODE [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_SUCCESS;
	}

	org_siteid= get_my_siteid_shm();

	ptrconn = mysql_init( NULL );
	if ( ptrconn == NULL )
	{
		print_dbg( DBG_ERR,1, "Fail of initialize DB  [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ret;
	}

	/* DB Connect */
	ret = db_connect( ptrconn );
	if ( ret !=  ERR_SUCCESS )
	{
		close_db( ptrconn );
		return ret;
	}

	/* 해당 부분 변경 시 반드시 CLI 도 수정할 것 */
	/* TFMS_AITHRLD_DTL */
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_AITHRLD_DTL set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );

	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_INFO,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* TFMS_DIDOTHRLD_DTL */
	sys_sleep( 100 );
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_DIDOTHRLD_DTL set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );
	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_SAVE,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* TFMS_FCLT_MST */
	sys_sleep( 100 );
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_FCLT_MST set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );
	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_SAVE,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* TFMS_SITE_MST */
	sys_sleep( 100 );
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_SITE_MST set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );
	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_SAVE,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}
	
	/* TFMS_SNSRMETHOD_MST */
	sys_sleep( 100 );
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_SNSRMETHOD_MST set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );
	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_SAVE,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* TFMS_SNSR_MST */
	sys_sleep( 100 );
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_SNSR_MST set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );
	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_SAVE,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* TFMS_USER_MST */
	sys_sleep( 100 );
	memset( szsql, 0, sizeof(szsql));
	sprintf( szsql,"update TFMS_USER_MST set SITE_ID = %04d where SITE_ID= %04d", siteid, org_siteid );
	if( mysql_query( ptrconn, szsql ) != 0 )
	{
		close_db( ptrconn );
		print_dbg( DBG_SAVE,1, "Cannot Select %s[%s:%d:%s]",szsql, __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	mysql_close( ptrconn );

	/* 데이터가 없을 경우 에러로 처리한다 */

	return ret ;
}

/*================================================================================================
  외부 함수 정의
================================================================================================*/
int init_ctrl_db( void  )
{
	int ret = ERR_SUCCESS;

    memset( &g_db_info, 0, sizeof( g_db_info ));
  
    if ( get_db_info_shm( &g_db_info ) != ERR_SUCCESS )
    {
        ret = ERR_RETURN;
        print_dbg( DBG_ERR,1,"Can not Read DB Info");
    }
    print_dbg( DBG_INFO, 1, "22DB Info User:%s, Pwr:%s, Name:%s",
            g_db_info.szdbuser, g_db_info.szdbpwr, g_db_info.szdbname );

	return ret ;
}

int release_ctrl_db( void )
{
	return ERR_SUCCESS;
}

int load_rtu_info_ctrl_db( void * ptrdata )
{
	return inter_load_rtu_info( ptrdata );
}

int load_fclt_info_ctrl_db( void * ptrdata )
{
	return inter_load_fclt_info( ptrdata );
}

int load_fclt_sns_ctrl_db( int fcltid, void * ptrdata )
{
 	return inter_load_fclt_sns( fcltid, ptrdata );
}

int load_rtu_fclt_info_ctrl_db( void * ptrdata )
{
	return inter_load_rtu_fclt_info( ptrdata );
}

int load_do_ctrl_db( int rtuid, void * ptrdata )
{
	return inter_load_do( rtuid, ptrdata );
}

int load_di_ctrl_db( int rtuid, void * ptrdata )
{
	return inter_load_di( rtuid, ptrdata );
}

int load_ai_ctrl_db( int rtuid, void * ptrdata )
{
	return inter_load_ai( rtuid, ptrdata );
}

int load_pwr_dev_ctrl_db( int snsr_sub_type, void * ptrdata )
{
	return inter_load_pwr_dev( snsr_sub_type, ptrdata );
}

int load_fclt_net_ctrl_db( int siteid, int fcltid, void * ptrdata )
{
    return inter_load_fclt_net( siteid, fcltid, ptrdata );
}

int load_snsr_col_nm_ctrl_db( int siteid, void * ptrdata )
{
	return inter_load_snsr_col_nm( siteid, ptrdata );
}

int load_pwr_m_data_pos_ctrl_db( int snsr_id, int * ptrpos )
{
	return inter_load_pwr_m_data_pos( snsr_id, ptrpos );
}

int update_rtu_site_id_ctrl_db( int site_id )
{
	return inter_update_rtu_site_id( site_id );
}

