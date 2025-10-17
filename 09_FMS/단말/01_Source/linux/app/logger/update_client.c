/*********************************************************************
	file : update_client.c
	description :
	product by tory45( tory45@empal.com) ( 2016.08.05 )
	update by ?? ( . . . )
	
	- Control에서 fms_server로 보낼 데이터를 받아 fms_server로 보내는 기능
	- fms_server에서 데이터를 받아 Control로 정보를 보내는 기능 
 ********************************************************************/
#include "com_include.h"
#include "sem.h"
#include "shm.h"

#include "update_client.h"
#include "net.h"

/*================================================================================================
 DEFINE  
================================================================================================*/

#define FILE_WRITE_PATH "/home/fms/temp"
#define FILE_MOVEM_PATH "/home/fms/appm"
#define FILE_MOVES_PATH "/home/fms/apps"

#define USE_REBOOT 1 // 업데이트 후 리부팅 적용여부

/*================================================================================================
 전역 변수 
 1.static 적용 
 2.숫자형 데이터 일경우 volatile 적용 
 3.초기값 적용.
================================================================================================*/
static volatile int g_sem_key = UPDATE_KEY;	/* Updat Client 용 세마포어 키 */
static volatile int g_sem_id = 0;				/* Update Client 용 세마포어 id */

static send_data_func  g_ptrsend_data_control_func = NULL;		/* CONTROL 모듈  설정,제어, 알람  정보를 넘기는 함수 포인트 */

static volatile int g_start_upc_pthread 	= 0; 
static volatile int g_end_upc_pthread 		= 1; 
static pthread_t upc_pthread;
static void * upc_pthread_idle( void * ptrdata );
static volatile int g_release				= 0;

static volatile int g_clntsock = -1;

static send_data_buffer_t g_send_data_buffer;
static volatile int g_send_data_buffer_init = 0;
static recv_data_buffer_t g_recv_data_buffer;
static volatile int g_recv_data_buffer_init = 0;

static net_sock_t g_netsock;

static char g_update_server_ip[20] = {0,};
static unsigned short g_update_server_port = 0;
static volatile int g_update_connect_run = 0;
static volatile BYTE g_update_connect_req = 0;
static volatile int g_update_internal_init = 0;


typedef struct update_version_info_
{
    BYTE curdata[2];
    BYTE newdata[2];
    BYTE code;	//enumfirmware_code 참조
    volatile int  check;
    char * filename;
} update_version_info_t;

static update_version_info_t g_update_ver_data[EUT_FMCODE_MAX_COUNT];
static volatile int g_update_ver_data_cnt = 0;
static volatile int g_update_ver_data_idx = 0;
static volatile int g_update_ver_data_run = 0;

static volatile int g_download_data_idx = 0;
static char g_update_filename[MAX_PATH_SIZE] = {0,};
static volatile int g_update_recv_totsize = 0;;

static net_packet_t g_update_packet_que;

static unsigned int crc32tab[] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
    0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
    0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
    0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
    0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
    0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
    0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
    0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
    0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
    0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
    0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
    0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
    0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
    0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
    0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
    0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
    0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
    0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
    0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
    0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
    0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};



/*================================================================================================
  내부 함수 선언
  static 적용 
================================================================================================*/

static int recv_packet(char * data, int len);
static int send_packet(update_packet_header_t * ptrheader, void * ptrbody);
static int create_upc_thread( void );
static void* upc_pthread_idle( void * ptrdata );

static update_packet_header_t make_packet_header(unsigned short msgid, short paysize);

static int parser_packet(update_packet_header_t* ptrhead, void * ptrbody);

static update_version_info_t * get_update_current_data();
static char * get_update_file_name(update_version_info_t * ptrdata);
static void set_update_data_check(update_version_info_t * ptrdata, int val);
static int  get_update_data_check(update_version_info_t * ptrdata);

static void crc_init(unsigned int * ptrcrc);
static void crc_update(unsigned int * ptrcrc, unsigned char * ptrdata, unsigned long usize);
static void crc_finish(unsigned int * ptrcrc);

static int verify_update_file(char * szname, BYTE * ptrhash);
static int write_update_data_move();
static int write_update_data_temp(char * filename, char * szmod, char * ptrdata, int len);
static void delete_update_data_temp(char * filename);

static int send_version_info_req();
static int send_download_data_req();
static int send_download_finish_req();
static int send_update_forced_req(BYTE code);
static int send_ctl_down_ok();
static int send_update_finish_req();


static int send_data_control( UINT16 inter_msg, void * ptrdata );

static int internal_init( void );
static int internal_idle( void * ptr );
static int internal_release( void );

/* Proc 함수 [ typedef int ( * proc_func )( unsigned short , void * ); ] */ 
static int rtusvr_sns_data_res( unsigned short inter_msg, void * ptrdata );

static int rtusvr_evt_allim_msg(unsigned short inter_msg, void * ptrdata);


/*================================================================================================
  Proc Function Point 구조체
 	Messsage ID 	: 내부 Message ID
	Proc Function 	: Message ID 처리 함수 
	Control에서 받은 데이터를 처리할 함수를 아래의 형식으로 등록하시면 됩니다. 
	typedef int ( * proc_func )( unsigned short , void * );
	이해가되시면 주석은 삭제하셔도 됩니다. 
================================================================================================*/
static proc_func_t g_proc_func_list []= 
{
	/* inter msg id   		,  			proc function point */
	/*=============================================*/
	{ EFT_SNSRDAT_RES, 			rtusvr_sns_data_res },	/* Sample 구조체 : Control에서 환경 감시 센서 데이터가 넘어 올 경우 */
    { EFT_ALLIM_MSG,            rtusvr_evt_allim_msg },

	{ 0, NULL }
};

/*================================================================================================
내부 함수  정의 
매개변수가 pointer일경우 반드시 NULL 체크 적용 
================================================================================================*/

static int connect_upcsvr( void )
{
    char szip[MAX_IP_SIZE];
    unsigned short port = 0;
    int ret = ERR_SUCCESS;

    memset(szip, 0, sizeof(szip));

    get_update_server_net_shm(szip, &port);

    g_netsock.clntsock = net_connect_client(szip, port);

    print_line();

    if (g_netsock.clntsock > 0)
    {
        print_dbg(DBG_INFO, 1, "connect_updatesvr ok..... clntsock (%d) [%s:%d] [%s:%d:%s]",
            g_netsock.clntsock, szip, port, __FILE__, __LINE__, __FUNCTION__);
        g_netsock.isalive = 1;
        ret = ERR_SUCCESS;
    }
    else
    {
        print_dbg(DBG_ERR, 1, "connect_updatesvr error..... [%s:%d] [%s:%d:%s]",
            szip, port, __FILE__, __LINE__, __FUNCTION__);
        g_netsock.isalive = 0;
        ret = ERR_FAIL;
    }

    return ret;
}

static int connect_close( void )
{
    if (g_netsock.clntsock > 0)
    {
        char szip[MAX_IP_SIZE];
        unsigned short port = 0;

        get_update_server_net_shm(szip, &port);

        print_line();
        print_dbg(DBG_NONE, 1, "close_updatesvr [%s:%d] [%s:%d:%s]", szip, port, __FILE__, __LINE__, __FUNCTION__);
        net_connect_close(g_netsock.clntsock);
        g_netsock.clntsock = -1;
        g_netsock.isalive  = 0;
        g_update_connect_run = 0;
        g_start_upc_pthread = 0;

    }
    return ERR_SUCCESS;
}

static int create_upc_thread( void )
{
    int ret = ERR_SUCCESS;

    print_dbg(DBG_NONE, 1, "create_upc_thread()");

    if ( g_release == 0 && g_start_upc_pthread == 0 && g_end_upc_pthread == 1 )
    {

        g_start_upc_pthread = 1;

        if ( pthread_create( &upc_pthread, NULL, upc_pthread_idle , NULL ) < 0 )
        {
            g_start_upc_pthread = 0;
            print_dbg( DBG_ERR, 1, "Fail to Create upc client Thread" );
            ret = ERR_RETURN;
        }
        else
        {
            print_dbg( DBG_NONE, 1, "Create upc client Thread Create OK. ");
            ret = send_version_info_req();
        }
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

static update_packet_header_t make_packet_header(unsigned short msgid, short paysize)
{
    update_packet_header_t header;
    memset(&header, 0, sizeof(header));
    header.totsize = sizeof(header) + paysize;
    header.msgid = msgid;
    header.directionl = 0x02;
    header.var1 = 0;
    header.rtuid = get_my_siteid_shm();
    header.paysize = paysize;

    return header;
}

static int send_version_info_req()
{

    update_packet_header_t head;
    update_version_data_t  body[EUT_FMCODE_MAX_COUNT];
    ver_info_t * ptrver = get_rtu_version_shm();

    memset(&head, 0, sizeof(head));
    memset(&body, 0, sizeof(body));

    head = make_packet_header(EUT_VERSION_INFO_REQ, sizeof(body));

    body[0].code = EUT_FMCODE_EMBEDDE;
    memcpy(body[0].bVer, ptrver->app_ver, sizeof(body[0].bVer));
    body[1].code = EUT_FMCODE_MCU;
    memcpy(body[1].bVer, ptrver->mcu_ver, sizeof(body[1].bVer));
    body[2].code = EUT_FMCODE_REMOTE;
    memcpy(body[2].bVer, ptrver->remote_ver, sizeof(body[2].bVer));
    body[3].code = EUT_FMCODE_TEMPER;
    memcpy(body[3].bVer, ptrver->temp_ver, sizeof(body[3].bVer));
    body[4].code = EUT_FMCODE_POWER;
    memcpy(body[4].bVer, ptrver->pwr_c_ver, sizeof(body[4].bVer));
    body[5].code = EUT_FMCODE_RESERVED;
    body[5].bVer[0] = g_update_connect_req;
    body[6].code = EUT_FMCODE_EMBEDDE_OBSER;
    memcpy(body[6].bVer, ptrver->ob_ver, sizeof(body[6].bVer));

    print_dbg(DBG_NONE, 1, "[SEND] EUT_VERSION_INFO_REQ [0x%02X:%02d.%02d] [0x%02X:%02d.%02d] [0x%02X:%02d.%02d] [0x%02X:%02d.%02d] [0x%02X:%02d.%02d] [0x%02X:%02d] [0x%02X:%02d.%02d]",
        body[0].code, body[0].bVer[0], body[0].bVer[1],
        body[1].code, body[1].bVer[0], body[1].bVer[1],
        body[2].code, body[2].bVer[0], body[2].bVer[1],
        body[3].code, body[3].bVer[0], body[3].bVer[1],
        body[4].code, body[4].bVer[0], body[4].bVer[1],
        body[5].code, body[5].bVer[0],
        body[6].code, body[6].bVer[0], body[6].bVer[1]);

    return send_packet(&head, &body);
}

static update_version_info_t * get_update_current_data()
{
    return (g_update_ver_data + g_update_ver_data_idx);
}

static char * get_update_file_name(update_version_info_t * ptrdata)
{
    char* filename = NULL;
    
    if ((filename = get_conv_update_name(ptrdata->code)) == NULL)
    {
        print_dbg(DBG_NONE, 1, "update conv filename null code:%d.... [%s%s]",ptrdata->code, __FILE__, __FUNCTION__);
        return NULL;
    }

    sprintf(g_update_filename, "%s/%s", FILE_WRITE_PATH, filename);

    return g_update_filename;
}

static void delete_update_data_temp(char * filename)
{
    delete_file(filename);
}


static int write_update_data_move()
{
    static char query[MAX_PATH_SIZE] = {0,};

    int i;
    for (i = 0; i < EUT_FMCODE_MAX_COUNT; i++)
    {
        if (get_update_data_check(&(g_update_ver_data[i])) == 1)
        {
            sprintf(query, "cp -f %s/%s %s/%s && chmod +x %s/%s"
                , FILE_WRITE_PATH, g_update_ver_data[i].filename
                , FILE_MOVEM_PATH, g_update_ver_data[i].filename
                , FILE_MOVEM_PATH, g_update_ver_data[i].filename);

            print_dbg(DBG_ERR, 1, "write_update_data_move [ %s ]", query);
            system_cmd(query);
        }
    }

    return ERR_SUCCESS;
}

static void set_update_data_check(update_version_info_t * ptrdata, int val)
{
    ptrdata->check = val;
}

static int get_update_data_check(update_version_info_t * ptrdata)
{
    return ptrdata->check;
}

static int write_update_data_temp(char * filename, char * szmod, char * ptrdata, int len)
{
    FILE* ptrfp = fopen(filename, szmod);
    int ret = 0;

    if (ptrfp == NULL)
    {
        print_dbg(DBG_ERR, 1, "io open error ... filepath(%s) mod(%s) PTR(%p) SIZE(%d) [%s%s]"
            , filename, szmod, ptrdata, len, __FILE__, __FUNCTION__);
        return ERR_FAIL;
    }

    if (ptrdata == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is Null .... [%s%s]", __FILE__, __FUNCTION__);
        return ERR_FAIL;
    }

    ret = fwrite( ptrdata, sizeof( char ), len, ptrfp );

    fclose(ptrfp);

    if( ret < 0 )
    {
        print_dbg( DBG_ERR, 1, "dowload data write error ... (%s)[%s:%s]", filename, __FILE__, __FUNCTION__);
        // TEMP 파일 삭제
        delete_update_data_temp(filename);
        send_update_forced_req(0x02);

        ret = ERR_RETURN;
    }
    else
    {
        ret = ERR_SUCCESS;
    }

    return ret;
}

static void crc_init(unsigned int * ptrcrc)
{
    *ptrcrc = 0xFFFFFFFF;
}

static void crc_update(unsigned int * ptrcrc, unsigned char * ptrdata, unsigned long usize)
{
    unsigned long i;

    for(i = 0; i < usize; i++)
    {
        *ptrcrc = ((*ptrcrc) >> 8) ^ crc32tab[(ptrdata[i]) ^ ((*ptrcrc) & 0x000000FF)];
    }

}
static void crc_finish(unsigned int * ptrcrc)
{
    *ptrcrc = ~(*ptrcrc);
}


static int verify_update_file(char * szname, BYTE * ptrhash)
{
    unsigned int usrc = 0;
    unsigned int ucmp = 0;
    static unsigned char databuf[1024];
    FILE * fp;
    int len = 0;

    if (ptrhash == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null ptrhash ... [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    memcpy(&ucmp, ptrhash, sizeof(ucmp));

    if (szname == NULL)
    {
        print_dbg(DBG_ERR, 1, "Is null szname ... [%s:%d:%s]", __FILE__, __LINE__, __FUNCTION__);
        return ERR_RETURN;
    }

    crc_init(&usrc);

    if ((fp = fopen(szname , "r")) == NULL)
    {
        return ERR_RETURN;
    }

    while (feof(fp) == 0)
    {
        memset(databuf, 0 ,sizeof(databuf));
        len = fread( databuf, 1, 1024, fp );
        crc_update(&usrc, databuf, len);
    }

    if ( fp != NULL ) fclose( fp );

    crc_finish(&usrc);

    if (usrc != ucmp)
    {
        print_dbg(DBG_ERR, 1, "update file verify failed ... (%u)(%u) [%s:%d:%s]", usrc, ucmp, __FILE__, __LINE__, __FUNCTION__);
        return ERR_FAIL;
    }
    else
    {
        print_dbg(DBG_ERR, 1, "update file verify ok ....... (%u)(%u) [%s:%d:%s]", usrc, ucmp, __FILE__, __LINE__, __FUNCTION__);
    }

    return ERR_SUCCESS;
}

static int send_download_data_req()
{
    update_packet_header_t head = make_packet_header(EUT_DOWNLOAD_DATA_REQ, 1);
    BYTE rlst = 0;
    if (g_update_ver_data_run == 1)
    {
        print_dbg(DBG_NONE, 1, "[SEND] EUT_DOWNLOAD_DATA_REQ");
        sprintf(g_update_filename, "%s", get_update_file_name(get_update_current_data()));
        rlst = g_update_ver_data[g_update_ver_data_idx].code;
        return send_packet(&head, (char *)(&rlst));
    }
    else
    {
        print_dbg(DBG_INFO, 1, " Download Forced .... [%s:%s]", __FILE__, __FUNCTION__);
        return ERR_RETURN;
    }
}

static int send_download_finish_req()
{
    update_packet_header_t head = make_packet_header(EUT_DOWNLOAD_FINISH_REQ, 0);
    print_dbg(DBG_NONE, 1, "[SEND] EUT_DOWNLOAD_FINISH_REQ");
    return send_packet(&head, NULL);
};

static int send_update_forced_req(BYTE code)
{
    update_packet_header_t head = make_packet_header(EUT_UPDATE_FORCED_REQ, 1);
    BYTE rlst = code;
    print_dbg(DBG_NONE, 1, "[SEND] EUT_UPDATE_FORCED_REQ (0x%02X)", rlst);
    g_update_ver_data_run = 0;
    return send_packet(&head, (char *)(&rlst));
};

static int send_ctl_down_ok()
{
    int ret = ERR_SUCCESS;
    UINT16 cmd = EFT_ALLIM_MSG;
    inter_msg_t inter_msg;
    allim_msg_t allim_msg;

    memset( &inter_msg, 0, sizeof( inter_msg ));
    memset( &allim_msg, 0, sizeof( allim_msg ));

    /* allim msg */
    allim_msg.allim_msg = ALM_UPDATE_DOWN_OK_MSG;

    /* intr msg */
    inter_msg.client_id    = 0;
    inter_msg.msg          = cmd;
    inter_msg.ptrbudy      = &allim_msg;

    send_data_control(cmd, &inter_msg);

    g_update_ver_data_run = 0;

    print_dbg(DBG_NONE, 1, "@@@@@@@@@@@@@@ send_ctl_down_ok ALM_UPDATE_DOWN_OK_MSG ");

    return ret;
};

static int send_update_finish_req()
{
    update_packet_header_t head = make_packet_header(EUT_UPDATE_FINISH_REQ, 0);

    print_dbg(DBG_NONE, 1, "[SEND] EUT_UPDATE_FINISH_REQ");

    send_packet(&head, NULL);

    return ERR_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

static void* upc_pthread_idle( void * ptrdata )
{
    ( void ) ptrdata;

    int max_fd = -1;
    fd_set readsets;
    struct timeval tv;
    int n;
    char recvdata[512];
    int recvsize;

    int status;

    g_end_upc_pthread = 0;
#if 1
    while ( g_release == 0 && g_start_upc_pthread == 1 )
    {
        FD_ZERO(&readsets);
        FD_SET(g_netsock.clntsock, &readsets);

        max_fd = g_netsock.clntsock;

        tv.tv_sec  = 1;	/* 1sec */
        tv.tv_usec = 0;

        n = select( max_fd + 1 , &readsets, NULL, NULL , &tv );

        if (n == -1) /*select 함수 오류 발생 */
        { 
            print_dbg( DBG_ERR, 1, "select function error [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
            sys_sleep(1000);
            continue;
        }
        else if (n == 0) /* time-out에 의한 리턴 */
        { 
            //print_dbg( DBG_NONE, 1,"===>> 시간이 초과 되었습니다 : sock %d", g_sock);
        }
        else /* 파일 디스크립터 변화에 의한 리턴 */
        { 
            //print_dbg( DBG_NONE, 1,"===>> fms read event : sock %d", g_sock);

            if (FD_ISSET(g_netsock.clntsock, &readsets))
            {
                recvsize = read(g_netsock.clntsock, recvdata, 512);

                if (recvsize > 0)
                {
                    //print_dbg( DBG_NONE, 1, "upc recv datasize : %d", recvsize);

                    if (recv_packet(recvdata, recvsize) != ERR_SUCCESS)
                    {
                        ;;;
                    }
                    // select init set
                    FD_ZERO(&readsets);
                    FD_SET(g_netsock.clntsock, &readsets);
                }
                else
                {
                    print_dbg( DBG_NONE, 1,"upc close event : %d \n", g_netsock.clntsock); 
                    // server socket cloase
                    FD_CLR(g_netsock.clntsock, &readsets);
                    connect_close();
                }
            }
        }
        sys_sleep(1);
    }
#endif
    if( g_end_upc_pthread == 0 )
    {
        pthread_join( upc_pthread , ( void **)&status );
        g_start_upc_pthread 	= 0;
        g_end_upc_pthread 		= 1;

        print_dbg(DBG_INFO, 1, "####### ====== >>> pthread_join end !!!!!!!!!! [%s:%s]", __FILE__, __FUNCTION__);

    }

    return ERR_SUCCESS;
}


static int parser_packet(update_packet_header_t* ptrhead, void * ptrbody)
{
    net_packet_t* ptrque = NULL;

    if ( ptrhead == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    int msgid = ptrhead->msgid;

    ptrque = &(g_update_packet_que);

    if (ptrque->state == 1 && ptrque->recvid == msgid)
    {
        // recv packet delete
        ptrque->state = 0;
        //set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
    }

    switch (msgid)
    {
    case EUT_FIRMWARE_WRITE_STATE:
        {
            print_dbg(DBG_NONE, 1, "[RECV] EUT_FIRMWARE_WRITE_STATE");
        }
        break;
    case EUT_SERVER_CONNECT:
        {
            print_dbg(DBG_NONE, 1, "[RECV] EUT_SERVER_CONNECT");
        }
        break;
    case EUT_VERSION_INFO_REQ:
        {
            print_dbg(DBG_NONE, 1, "[RECV] EUT_VERSION_INFO_REQ");
        }
        break;
    case EUT_VERSION_INFO_RSP:
        {
            update_version_data_t * ptrdata = NULL;
            ver_info_t * ptrver = NULL;
            BYTE curver[2], newver[2];
            int ncount = 0;
            int i, idx = 0;

            print_dbg(DBG_NONE, 1, "[RECV] EUT_VERSION_INFO_RSP");


            if (g_update_connect_req == 0)
            {
                print_dbg( DBG_INFO, 1, "####### ====== >>> system reboot update completed .... [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
                connect_close();
                //net_connect_close(g_netsock.clntsock);
                //g_netsock.clntsock = -1;
                //g_netsock.isalive  = 0;
                return ERR_SUCCESS;
            }

            if (ptrbody == NULL)
            {
                print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
                return ERR_RETURN;
            }

            ptrdata = (update_version_data_t *)(ptrbody + 1);

            ncount = ((BYTE *)(ptrbody))[0];

            ptrver = get_rtu_version_shm();

            for (i = 0; i < ncount; i++)
            {
                newver[0] = ptrdata[i].bVer[0];
                newver[1] = ptrdata[i].bVer[1];

                switch (ptrdata[i].code)
                {
                case EUT_FMCODE_EMBEDDE:// Embedde PC
                    curver[0] = ptrver->app_ver[0];
                    curver[1] = ptrver->app_ver[1];
                    break;
                case EUT_FMCODE_MCU:// MCU
                    curver[0] = ptrver->mcu_ver[0];
                    curver[1] = ptrver->mcu_ver[1];
                    break;
                case EUT_FMCODE_REMOTE:// 리모컨
                    curver[0] = ptrver->remote_ver[0];
                    curver[1] = ptrver->remote_ver[1];
                    break;
                case EUT_FMCODE_TEMPER:// 온습도
                    curver[0] = ptrver->temp_ver[0];
                    curver[1] = ptrver->temp_ver[1];
                    break;
                case EUT_FMCODE_POWER:// 전원제어기
                    curver[0] = ptrver->pwr_c_ver[0];
                    curver[1] = ptrver->pwr_c_ver[1];
                    break;
                case EUT_FMCODE_EMBEDDE_OBSER:// Embedde PC obser
                    curver[0] = ptrver->ob_ver[0];
                    curver[1] = ptrver->ob_ver[1];
                    break;
                case EUT_FMCODE_RESERVED:// 0x06 ~ : Reserved
                    break;
                }

                print_dbg(DBG_ERR, 1, "CODE(%X) OLD_VER(H:%03d L:%03d) NEW_VER(H:%03d L:%03d) "
                    , ptrdata[i].code
                    , curver[0], curver[1]
                    , newver[0], newver[1]);

                // 업데이트 정보 확인
                if (curver[0] != newver[0] || curver[1] != newver[1])
                {
                    g_update_ver_data[idx].code = ptrdata[i].code;
                    g_update_ver_data[idx].curdata[0] = curver[0];
                    g_update_ver_data[idx].curdata[1] = curver[1];
                    g_update_ver_data[idx].newdata[0] = newver[0];
                    g_update_ver_data[idx].newdata[1] = newver[1];
                    g_update_ver_data[idx].check = 0;
                    g_update_ver_data[idx].filename = get_conv_update_name(g_update_ver_data[idx].code);
                    idx++;
                }
            }

            g_update_ver_data_cnt = idx;
            g_update_ver_data_idx = 0;
            g_update_ver_data_run = 0;
            g_download_data_idx = 0;
            g_update_recv_totsize = 0;

            // 업데이트 파일존재여부 확인
            if (g_update_ver_data_cnt > 0)
            {
                g_update_ver_data_run = 1;

                send_download_data_req();
            }
            else
            {
                send_update_finish_req();
                set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
            }
        }
        break;
    case EUT_DOWNLOAD_DATA_REQ:
        {
            print_dbg(DBG_NONE, 1, "[RECV] EUT_DOWNLOAD_DATA_REQ");
        }
        break;
    case EUT_DOWNLOAD_DATA_RSP:
        {
            update_download_rsp_t * ptrdata = NULL;

            int datasize = 0; //현재 전송하는 펌웨어 사이즈
            int datacnt = 0;
            BYTE hashcode[NET_MAX_CHECKSUM];

            memset(hashcode, 0, sizeof(hashcode));

            print_dbg(DBG_NONE, 1, "[RECV] EUT_DOWNLOAD_DATA_RSP");

            g_update_packet_que.sendid = EUT_DOWNLOAD_DATA_REQ;
            g_update_packet_que.state = 1;
            net_packet_update_rule_make(&(g_update_packet_que));

            if (ptrbody == NULL)
            {
                print_dbg(DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
                return ERR_RETURN;
            }

            ptrdata = (update_download_rsp_t *) ptrbody;


            if (ptrdata->totsize <= 0)
            {
                print_dbg(DBG_ERR, 1, "download data total size zero error .... [%s:%s]", __FILE__, __FUNCTION__);
                return ERR_RETURN;
            }

            datacnt = ptrdata->totsize / MAX_FIRM_SIZE;

            if (ptrdata->indx == 0)
            {
                datasize = ptrdata->sendsize; //현재 전송하는 펌웨어 사이즈
                g_download_data_idx = ptrdata->indx; //1~ 1000000 	펌웨어 파일 전송 순번
                g_update_recv_totsize = datasize; //현재 전송하는 펌웨어 사이즈

                // 데이터 파일 전체 크기와 수신크기가 같다면
                if (g_update_recv_totsize == ptrdata->totsize)
                {
                    datasize -= NET_MAX_CHECKSUM;
                    memcpy(hashcode, ptrdata->data + datasize, sizeof(hashcode));
                    print_dbg(DBG_ERR, 1, "##### ==>> FILEDOWNLOAD CODE(%02X) IDX(%d) TOTAL(%d) RECV_TOTAL(%d) DATAIDX(%d)"
                        , g_update_ver_data[g_update_ver_data_idx].code
                        , ptrdata->indx
                        , ptrdata->totsize
                        , g_update_recv_totsize
                        , g_update_ver_data_idx);

                }
                else
                {
                    print_dbg(DBG_NONE, 1, "CODE(%02X) IDX(%d) TOTAL(%d) SIZE(%d) RECV_TOTAL(%d) DATACNT(%d) DATAIDX(%d) WRITESIZE(%d)"
                        , g_update_ver_data[g_update_ver_data_idx].code
                        , ptrdata->indx
                        , ptrdata->totsize
                        , ptrdata->sendsize
                        , g_update_recv_totsize
                        , datacnt
                        , g_update_ver_data_idx
                        , datasize);
                }

                //다운로드 시작 인덱스 데이터
                write_update_data_temp(get_update_file_name(get_update_current_data()), "w+b", (char * ) ptrdata->data, datasize);

                g_download_data_idx++;
            }
            else if (ptrdata->indx == g_download_data_idx)
            {
                datasize = ptrdata->sendsize; //현재 전송하는 펌웨어 사이즈
                //다운로드 정상 인덱스 데이터
                g_download_data_idx = ptrdata->indx; //1~ 1000000 	펌웨어 파일 전송 순번
                g_update_recv_totsize += datasize;

                
                // 데이터 파일 전체 크기와 수신크기가 같다면
                if (g_update_recv_totsize == ptrdata->totsize)
                {
                    datasize -= NET_MAX_CHECKSUM;
                    memcpy(hashcode, ptrdata->data + datasize, sizeof(hashcode));
                    print_dbg(DBG_ERR, 1, "##### ==>> FILEDOWNLOAD CODE(%02X) IDX(%d) TOTAL(%d) RECV_TOTAL(%d) DATAIDX(%d)"
                        , g_update_ver_data[g_update_ver_data_idx].code
                        , ptrdata->indx
                        , ptrdata->totsize
                        , g_update_recv_totsize
                        , g_update_ver_data_idx);
                }
                else
                {
                    print_dbg(DBG_NONE, 1, "CODE(%02X) IDX(%d) TOTAL(%d) SIZE(%d) RECV_TOTAL(%d) DATACNT(%d) DATAIDX(%d) WRITESIZE(%d)"
                        , g_update_ver_data[g_update_ver_data_idx].code
                        , ptrdata->indx
                        , ptrdata->totsize
                        , ptrdata->sendsize
                        , g_update_recv_totsize
                        , datacnt
                        , g_update_ver_data_idx
                        , datasize);
                }

                //다운로드 시작 인덱스 데이터
                write_update_data_temp(get_update_file_name(get_update_current_data()), "a+b", (char * ) ptrdata->data, datasize);

                g_download_data_idx++;
            }
            else
            {
                //다운로드 인덱스 오류
                print_dbg( DBG_ERR, 1, "download data index error index(%d) download_idx(%d)... [%s:%d:%s]"
                    , ptrdata->indx, g_download_data_idx + 1, __FILE__, __LINE__,__FUNCTION__ );
                // TEMP 파일 삭제
                delete_update_data_temp(get_update_file_name(get_update_current_data()));
                send_update_forced_req(0x01);
                set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);

                return ERR_RETURN;
            }

            // 데이터 파일 전체 크기와 수신크기가 같다면
            if (g_update_recv_totsize == ptrdata->totsize)
            {
                char *  filename = get_update_file_name(get_update_current_data());

                if (filename == NULL)
                {
                    print_dbg( DBG_ERR, 1, "download filename error... [%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__ );
                    send_update_forced_req(0x02);
                    set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
                    return ERR_RETURN;
                }

                // 파일 HASHCODE 검증
                if (verify_update_file(filename, hashcode) == ERR_FAIL)
                {
                    delete_update_data_temp(filename);
                    send_update_forced_req(0x02);
                    set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
                    return ERR_RETURN;
                }

                set_update_data_check(get_update_current_data(), 1);

                if (g_update_ver_data_cnt > (++g_update_ver_data_idx))
                {
                    send_download_data_req();
                }
                else
                {
                    send_download_finish_req();
                }
            }

        }
        break;
    case EUT_DOWNLOAD_FINISH_REQ:
        {
            print_dbg(DBG_NONE, 1, "[RECV] EUT_DOWNLOAD_FINISH_REQ");
        }
        break;
    case EUT_DOWNLOAD_FINISH_RSP:
        {
            print_dbg(DBG_NONE, 1, "[RECV] EUT_DOWNLOAD_FINISH_RSP");

            // 데이터 이동
            write_update_data_move();
#if USE_REBOOT // 콘트롤로 메시지 송신후 5초후에 리부팅함 [10/20/2016 redIce]
            send_ctl_down_ok();
#endif
        }
        break;
    case EUT_UPDATE_FORCED_REQ:
        {
            print_dbg(DBG_NONE, 1, "[RECV] EUT_UPDATE_FORCED_REQ");
        }
        break;
    case EUT_UPDATE_FORCED_RSP:
        {
            print_dbg(DBG_NONE, 1, "[RECV] EUT_UPDATE_FORCED_RSP");
            connect_close();
        }
        break;
    case EUT_UPDATE_FINISH_REQ:
        {
            print_dbg(DBG_NONE, 1, "[RECV] EUT_UPDATE_FINISH_REQ");
        }
        break;
    case EUT_UPDATE_FINISH_RSP:
        {
            print_dbg(DBG_NONE, 1, "[RECV] EUT_UPDATE_FINISH_RSP");

            connect_close();
        }
        break;
    }

    return ERR_SUCCESS;
}

static int recv_packet(char * data, int len)
{
    recv_data_buffer_t * ptrbuf = NULL;
    update_packet_header_t headerdata;
    int headersize = sizeof(headerdata);
    int datasize = 0;

    if (data == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_FAIL;
    }

    ptrbuf = &(g_recv_data_buffer);

    if ((ptrbuf->posw + len) > NET_RECV_BUFFER_MAX)
    {
        print_dbg( DBG_ERR, 1, "recv data buffer overflow [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        ptrbuf->posw = 0;
        ptrbuf->posr = 0;
        return ERR_OVERFLOW;
    }

    memcpy(ptrbuf->buf + ptrbuf->posw, data, len);
    ptrbuf->posw += len;

    while ((datasize = ptrbuf->posw - ptrbuf->posr) >= headersize)
    {
        int packetsize = 0;

        memcpy(&headerdata, ptrbuf->buf + ptrbuf->posr, headersize);
#if 0
        print_dbg(DBG_INFO, 1, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  %d", len);
        print_dbg(DBG_INFO, 1, " totsize(%d) msgid(%d) directionl(%d) var1(%d) rtuid(%d) paysize(%d) "
            , headerdata.totsize
            , headerdata.msgid
            , headerdata.directionl
            , headerdata.var1
            , headerdata.rtuid
            , headerdata.paysize
            );
#endif
        packetsize = headerdata.totsize;

        // 리시브 데이터 크기와 패킷사이즈 확인
        if (packetsize > datasize)
        {
            return ERR_SUCCESS;
        }

        // 패킷데이터 파서
        print_dbg(DBG_INFO, 1, "[RECV BUFFER] POSR(%d) POSW(%d) PACKETSIZE(%d) [%s:%s]", ptrbuf->posr, ptrbuf->posw, packetsize, __FILE__, __FUNCTION__);
        print_dbg(DBG_NONE, 1, "msgid(%X) directionl(%d) var1(%d) rtuid(%d)", headerdata.msgid, headerdata.directionl, headerdata.var1, headerdata.rtuid);

        parser_packet(&headerdata, ptrbuf->buf + (ptrbuf->posr + headersize));

        // 데이터버퍼 읽은 위치 이동
        ptrbuf->posr += packetsize;

        // 미처리된 데이터 처리
        if (ptrbuf->posw <= ptrbuf->posr)
        {
            ptrbuf->posr = 0;
            ptrbuf->posw = 0;
        }
        else
        {
            int srcpos, dstpos = 0;
            for (srcpos = ptrbuf->posr; srcpos < ptrbuf->posw;)
            {
                ptrbuf->buf[dstpos++] = ptrbuf->buf[srcpos];
                ptrbuf->buf[srcpos++] = 0;
            }
            ptrbuf->posr = 0;
            ptrbuf->posw = dstpos;
        }
    }//while ((datasize = ptrbuf->posw - ptrbuf->posr) > headersize)

    return ERR_SUCCESS;
}

static int send_packet(update_packet_header_t * ptrhead, void * ptrbody)
{
    if ( ptrhead == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null [%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_FAIL;
    }

    print_line();
    print_dbg( DBG_INFO, 1, "packet send socket(%d) start..... [%s:%s]", g_netsock.clntsock, __FILE__, __FUNCTION__ );

    if (g_netsock.clntsock != -1)
    {
        print_dbg(DBG_NONE, 1, "[SEND] TOTSIZE(%d) MSGID(%x) DIRECTIONL(%x) VAR1(%x) RTUID(%d) PAYSIZE(%d) "
            , ptrhead->totsize, ptrhead->msgid, ptrhead->directionl, ptrhead->var1, ptrhead->rtuid, ptrhead->paysize);

        g_update_packet_que.sendid = ((update_packet_header_t*)ptrhead)->msgid;
        g_update_packet_que.state = 1;
        net_packet_update_rule_make(&(g_update_packet_que));

        if (ptrbody != NULL) {
            send(g_netsock.clntsock, ptrhead, sizeof(update_packet_header_t), 0);
            send(g_netsock.clntsock, ptrbody, ptrhead->paysize, 0);
        } else {
            send(g_netsock.clntsock, ptrhead, sizeof(update_packet_header_t), 0);
        }
    }

    print_dbg( DBG_NONE, 1, "packet send end.....");

    return ERR_SUCCESS;
}


/* CONTROL module에  보낼 데이터(제어, 설정, 알람)가  있을 경우 호출 */
static int send_data_control( UINT16 inter_msg, void * ptrdata  )
{
	if ( g_ptrsend_data_control_func != NULL )
	{
		return g_ptrsend_data_control_func( inter_msg, ptrdata );
	}

	return ERR_RETURN;
}



/* control로 부터 sensor data를 받아 처리하는 함수 
 	이 함수는 샘플로 작성한 함수입니다. 이 함수를 참조하여
	필요한 함수를 만드시면 됩니다. 
 */
static int rtusvr_sns_data_res( unsigned short inter_msg, void * ptrdata )
{
	return ERR_SUCCESS;
}




static int rtusvr_evt_allim_msg(unsigned short inter_msg, void * ptrdata)
{
    inter_msg_t * ptrmsg = NULL;
    allim_msg_t * ptrbody = NULL;

    print_line();
    print_dbg( DBG_NONE,1,"rtusvr_evt_allim_msg[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );

    if ( ptrdata == NULL )
    {
        print_dbg( DBG_ERR, 1, "Is null ptrdata[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrmsg = ( inter_msg_t * )ptrdata;

    print_dbg( DBG_NONE,1,"clientId (%d) cmdId (%d)", ptrmsg->client_id, ptrmsg->msg);

    if (ptrmsg->ptrbudy == NULL)
    {
        print_dbg( DBG_ERR, 1, "Is null ptrmsg->ptrbudy[%s:%d:%s]",__FILE__, __LINE__,__FUNCTION__ );
        return ERR_RETURN;
    }

    ptrbody = ( allim_msg_t * ) ptrmsg->ptrbudy;

    switch (ptrbody->allim_msg)
    {
    case ALM_RTU_NET_CHG_NOTI_MSG: /* Control -> transfer(FMS, OP, Update ) */
        {
            // RTU장비 네트워크 아이피 또는 포트 변경이 일어난경우
            // 시스템 리부팅 해제처리 [10/22/2016 redIce]
        }
        break;
    case ALM_UPDATE_WRITE_OK_MSG: /* Control -> Update */
        {
            // 5초 후 리부팅
            send_update_finish_req();
        }
        break;
    }

    return 0;
}


/* CONTROL Module로 부터 수집 데이터, 알람, 장애  정보를 받는 내부 함수로
   등록된 proc function list 중에서 control에서  넘겨 받은 inter_msg와 동일한 msg가 존재할 경우
   해당  함수를 호출하도록 한다. 
 */
static int internal_recv_update_data( unsigned short inter_msg, void * ptrdata )
{
	int i;
	int proc_func_cnt = 0;
	
	proc_func  ptrtemp_proc_func = NULL;
	UINT16 list_msg = 0;

	if( ptrdata == NULL )
	{
		print_dbg( DBG_INFO, 1, "ptrdata is Null[%s:%d:%s]", __FILE__, __LINE__,__FUNCTION__);
		return ERR_RETURN;
	}

	/* proc function list에 등록된 개수 파악 */
	proc_func_cnt= ( sizeof( g_proc_func_list ) / sizeof( proc_func_t ));

	/* 등록된 proc function에서 inter_msg가 동일한 function 찾기 */
	for( i = 0 ; i < proc_func_cnt ; i++ )
	{    
		list_msg 			= g_proc_func_list[ i ].inter_msg; 	/* function list에 등록된 msg id */
		ptrtemp_proc_func 	= g_proc_func_list[ i ].ptrproc;	/* function list에 등록된 function point */

		if ( ptrtemp_proc_func != NULL && list_msg == inter_msg )
		{    
			ptrtemp_proc_func( inter_msg, ptrdata );
			break;
		}    
	} 

	return ERR_SUCCESS;
}

/* update_client module 초기화 내부 작업 */
static int internal_init( void )
{
    //int ret = ERR_SUCCESS;

	/* 데이터 동기화를 위한  세마포어를 생성한다 
		사용법 
		lock_sem( g_sem_id, g_sem_key );
		data 처리 
		unlock_sem( g_sem_id , g_sem_key );
	 */
	g_sem_id = create_sem( g_sem_key );
	print_dbg( DBG_INFO, 1, "UPDATE Client SEMA : %d", g_sem_id );
	/* warring을 없애기 위한 함수 호출 */
	send_data_control( 0, NULL );

    g_update_connect_req   = 0;
    g_update_internal_init = 0;

	print_dbg(DBG_INFO, 1, "Success of Update Client Initialize");

    //네트워크통신 버퍼 생성
    memset(&g_send_data_buffer, 0, sizeof(g_send_data_buffer));
    memset(&g_recv_data_buffer, 0, sizeof(g_recv_data_buffer));
    g_recv_data_buffer_init = 1;
    g_send_data_buffer_init = 1;

    net_packet_rule_init(&(g_update_packet_que));

#if 0 // 리부팅후 초기 interna init 에서는 버전정보 확인불가
    if ( ret == ERR_SUCCESS )
    {
        if ((ret = connect_upcsvr()) == ERR_SUCCESS)
        {
            ret = create_upc_thread();
        }
    }
#endif
	return ERR_SUCCESS;
}

/* update_client module loop idle 내부 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
static int internal_idle( void * ptr )
{
    int ret = ERR_SUCCESS;

	( void )ptr;

    if( g_release == 0 )
    {
        // 업데이트 서버연결 응답에 대한 서버연결
        if ((is_ver_checked_shm() == 1 && g_update_internal_init == 0) ||
            (g_update_connect_run == 1 && g_netsock.clntsock <= 0))
        {
            print_dbg(DBG_INFO, 1, "upc server connect retry !!!!");

            g_update_internal_init = 1;

            if ((ret = connect_upcsvr()) == ERR_SUCCESS)
            {
                ret = create_upc_thread( );
            }
        }

        // send pakcet time out check
        {
            net_packet_t * ptrque = &(g_update_packet_que);

            if (ptrque->state == 1 && net_packet_timeoutcheck(ptrque))
            {
                // timeout proc
                ptrque->state = 0;
                print_dbg(DBG_INFO, 1, "[PACKET TIMEOUT] PACKID(%d) [%s:%d:%s]", ptrque->sendid, __FILE__, __LINE__, __FUNCTION__);
                set_op_cmdctl_state(EUT_RTU_STATE_NORMAL, 0);
                connect_close();
                net_packet_rule_init(ptrque);
            }
        }
    }

    return ret;
}

/* update_client module의 release 내부 작업 */
static int internal_release( void )
{
    connect_close();

    g_release = 1;

	if ( g_sem_key > 0 )
	{
		destroy_sem( g_sem_id, g_sem_key );
		g_sem_key = -1;
	}

	print_dbg(DBG_INFO, 1, "Release of Update Client");
    //네트워크통신 버퍼 해제
    memset(&g_send_data_buffer, 0, sizeof(g_send_data_buffer));
    memset(&g_recv_data_buffer, 0, sizeof(g_recv_data_buffer));
    g_recv_data_buffer_init = 0;
    g_send_data_buffer_init = 0;

	return ERR_SUCCESS;
}

/*================================================================================================
외부  함수  정의 
================================================================================================*/
int init_update_client( send_data_func ptrsend_data )
{
	/* CONTROL module의 함수 포인트 등록 */
	g_ptrsend_data_control_func = ptrsend_data ;	/* CONTROL module로 설정, 제어, 알람 , 장애  데이터를 보낼 수 있는 함수 포인터 */

	/* 기타 초기화 작업 */
	internal_init();

	return ERR_SUCCESS;
}

/* update_client module loop idle 작업  
   1초 간격의로 메인에서 호출됨. 
   단, 100ms 이상 소요되는 작업을 하지 말것. 
 */
int idle_update_client( void * ptr )
{
	return internal_idle( ptr );
}

/* 동적 메모리, fd 등 resouce release함수 */
int release_update_client( void )
{
	/* release 작업  */
	return internal_release(); 
}

/* CONTROL Module로 부터 UPDATE 상태를 받는  외부 함수 */
int recv_cmd_data_update_client( unsigned short inter_msg, void * ptrdata )
{
	return internal_recv_update_data( inter_msg, ptrdata );
}

int update_connect_req(char* server_ip,  unsigned short port)
{
    print_line();
    print_dbg( DBG_ERR,1,"update_connect_req(%s,%d)[%s:%d:%s]", server_ip, port, __FILE__, __LINE__,__FUNCTION__ );

    set_update_server_net_shm(server_ip, port);

    connect_close();

    strcpy(g_update_server_ip, server_ip);
    g_update_server_port = port;
    g_update_connect_run = 1;
    g_update_connect_req = 1;

    return 0;
}
