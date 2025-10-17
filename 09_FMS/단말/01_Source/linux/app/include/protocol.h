#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_
#if defined(__linux__) || defined(__GNUC__)
#include "com_include.h"
#endif

/*====================================================================
 Protocol Message ID
=====================================================================*/

// FMS APP 패킷 TAG
#define EFT_ALLIM_MSG				100
#define EFT_ALLIM_WEB_REQ			101
#define EFT_ALLIM_WEB_NOTI		    EFT_ALLIM_WEB_REQ	

#define ALM_RTU_NET_CHG_NOTI_MSG	0x0001  /* Control -> transfer(FMS, OP, Update ) */
#define ALM_OP_NET_CHG_NOTI_MSG		0x0002	/* Control -> transfer( OP ) */	

#define ALM_FMS_NET_CHG_NOTI_MSG	0x0003	/* Control -> transfer( FMS ) */ 
#define ALM_FRE_NET_CHG_NOTI_MSG	0x0004	/* Control -> radio */ 
#define ALM_SRE_NET_CHG_NOTI_MSG	0x0005	/* Control -> radio */ 
#define ALM_FDE_NET_CHG_NOTI_MSG	0x0006	/* Control -> radio */ 

#define ALM_UPDATE_WRITE_OK_MSG		0x0007 	/* Control -> Update */ 
#define ALM_FORCE_PWR_END_MSG		0x0008 	/* Control -> transfer, Op */ 

#define ALM_UPDATE_DOWN_OK_MSG		0x0101	/* Update -> Control */  
#define ALM_OP_CLOSE_NOTI_MSG		0x0102	/* Op 	  -> Control */ 

#define ALM_FRE_NET_ADD_NOTI_MSG	0x0201	/* Control -> radio */ 
#define ALM_SRE_NET_ADD_NOTI_MSG	0x0202	/* Control -> radio */ 
#define ALM_FDE_NET_ADD_NOTI_MSG	0x0203	/* Control -> radio */ 

#define ALM_FRE_NET_DEL_NOTI_MSG	0x0204	/* Control -> radio */ 
#define ALM_SRE_NET_DEL_NOTI_MSG	0x0205	/* Control -> radio */ 
#define ALM_FDE_NET_DEL_NOTI_MSG	0x0206	/* Control -> radio */ 

#define ALM_UPS_NET_CHG_NOTI_MSG	0x0207	/* Control -> radio */ 
#define ALM_UPS_NET_ADD_NOTI_MSG	0x0208	/* Control -> radio */ 
#define ALM_UPS_NET_DEL_NOTI_MSG	0x0209	/* Control -> radio */ 

#define WEB_RTU_RESET_MSG			1
#define WEB_RTU_IP_CHG_MSG			2
#define WEB_RTU_SITE_CHG_MSG		3
#define WEB_RTU_FIRMWARE_CHG_MSG	4
#define WEB_RTU_NTP_CHG_MSG			5
#define WEB_RTU_MODE_CHG_MSG		6

#define WEB_RTU_ADD_CLIENT			99
typedef enum _enumfms_cmd 
{
	ECMD_NONE = 2210,

	//################################################################s
	// == FMS_SVR 네트웍 접속 시 확인 절차 필요 == 
	EFT_NETWORK_CONFIRM_REQ = 2211,
	EFT_NETWORK_CONFIRM_RES,
	//################################################################S
	// == 환경감시 == 

	//*** RTU센서 모니터링 데이터 수집(일정 주기로 10분?)
	// RTU센서 모니터링 응답(결과) ----
	EFT_SNSRDAT_RES=2222,		// FMS_RTU -> FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
	EFT_SNSRDAT_RESACK,			// 수신 확인 : FMS_SVR -> FMS_RTU

	// =========================================================S
	//*** RTU센서 모니터링 데이터 실시간 수집(5초?)
	// ----- 상태 정보(실시간) 송신 프로세스 시작/종료
	EFT_SNSRCUDAT_REQ=2322,		// FMS_OP --> FMS_RTU
	EFT_SNSRCUDAT_RES,			//* 현재 데이터 : FMS_RTU -> FMS_OP

	// =========================================================S
	//*** 장애(FAULT)이벤트 --- 
	EFT_SNSREVT_FAULT=2422,		// FMS_RTU --> FMS_SVR, FMS_RTU --> FMS_OP
	EFT_SNSREVT_FAULTACK,		// 수신확인 : FMS_SVR --> FMS_RTU

	// =========================================================S
	//*** 전원제어 (ON/OFF) -- RTU설비(전등, 에어컨)
	// 전원제어 명령 ----
	EFT_SNSRCTL_POWER=2522,		// FMS_OP --> FMS_RTU, FMS_RTU --> FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
	EFT_SNSRCTL_POWERACK,		// 수신 확인 : FMS_SVR --> FMS_RTU
	// 전원제어에 대한 결과 ----
	EFT_SNSRCTL_RES,			// FMS_RTU -> FMS_SVR, FMS_OP
	EFT_SNSRCTL_RESACK,			// 수신 확인 : FMS_SVR --> FMS_RTU

	// =========================================================S
	//*** 임계기준 설정 
	// 종류1(ANALOG INPUT) : 온도, 습도
	// 종류2(DIGITAL INPUT) : 누수, 열, 연기, 출입문

	// RTU센서 임계기준 설정 ---------------------------------------S
	EFT_SNSRCTL_THRLD=2622,		// FMS_OP --> FMS_RTU, FMS_RTU -> FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
	EFT_SNSRCTL_THRLDACK,		// 수신 확인 : FMS_SVR --> FMS_RTU
	// [2016/8/18 YKHONG] : 추가
	EFT_SNSRCTLCOMPLETED_THRLD,	// FMS_RTU -> FMS_OP : 임계기준 수정 완료를 OP에게 알려줌.

	// *********************************************************************S
	// == 감시장비 연계 == 

	//*** 감시장비 모니터링 데이터 ------------
	EFT_EQPDAT_REQ=3122,		// FMS_RTU --> RMS_EQP
	EFT_EQPDAT_RES,				// 응답 : RMS_EQP --> FMS_RTU, FMS_RTU -> FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
	EFT_EQPDAT_RESACK,			//  수신확인 : FMS_SVR -> FMS_RTU

	// 장비 : RTU가 감시장비 모니터링 결과 수신 했음을 확인
	EFT_EQPDAT_ACK,				// ACK 신호 : FMS_RTU --> RMS_EQP

	//*** 상태 정보(실시간) 송신 프로세스 -----
	// 상태 정보(실시간) 송신 프로세스 시작/종료
	EFT_EQPCUDAT_REQ=3322,		// FMS_OP --> FMS_RTU
	EFT_EQPCUDAT_RES,			//* 현재 데이터 : FMS_RTU -> FMS_OP

	//*** 장애(FAULT)이벤트 -------------------
	EFT_EQPEVT_FAULT=3422,		// FMS_RTU --> FMS_SVR, FMS_OP
	EFT_EQPEVT_FAULTACK,		// FMS_SVR --> FMS_RTU

	//*** 감시장비 전원제어 (ON/OFF)
	EFT_EQPCTL_POWER=3522,		// RMS_EQP --> FMS_RTU : 자동 전원 ON/OFF 시작!!!
	// FMS_OP --> FMS_RTU, FMS_RTU --> RMS_EQP, FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
	EFT_EQPCTL_POWERACK,		// 수신 확인 : FMS_SVR --> FMS_RTU
	// 전원제어에 대한 진행상태 정보 ---------
	EFT_EQPCTL_RES,				// RMS_EQP -> FMS_RTU, RMS_RTU -> FMS_SVR, FMS_OP
	EFT_EQPCTL_RESACK,			// 수신확인 : RMS_SVR -> FMS_RTU

	// [2016/11/8 ykhong] : 추가
	EFT_EQPCTL_POWEROPT_OFF=3526,	// 감시장비 전원제어 시 세부 제어 옵션 : 센서간 딜레이, 감시장비 응답 대기 시간 지정 등
	EFT_EQPCTL_POWEROPT_OFFRES,
	EFT_EQPCTL_POWEROPT_ON,			// 응답 확인 하지 않음
	EFT_EQPCTL_POWEROPT_ONRES,
	// [2016/11/14 ykhong] : 추가
	EFT_EQPCTL_STATUS_REQ=3530,    // FMS-OP -> FMS_RTU, 감시장비 전원제어 현재 진행 상태를 요청
	EFT_EQPCTL_STATUS_RES,
	EFT_EQPCTL_CNNSTATUS_REQ=3535, // FMS-OP -> FMS_RTU, 감시장비 네트웍 연결상태 요청
	EFT_EQPCTL_CNNSTATUS_RES,	   // FMS_RTU -> FMS-OP, 감시장비 네트웍 연결상태 결과

	//*** 감시장비 항목별 임계 설정 ----------
	EFT_EQPCTL_THRLD=3622,		// FMS_OP --> FMS_RTU, FMS_RTU -> FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
	EFT_EQPCTL_THRLDACK,		// 수신 확인 : FMS_SVR --> FMS_RTU
	// [2016/8/18 YKHONG] : 추가
	EFT_EQPCTLCOMPLETED_THRLD,	// FMS_RTU -> FMS_OP : 임계기준 수정 완료를 OP에게 알려줌.
	////////////////////////////////////////////////////// ----E

	// -------------------
	// 기타 - 시간동기화
	// -------------------
	// 
	// 시간동기화 : RTU - SVR - OP 간 시간 동기화
	EFT_TIMEDAT_REQ=7822,		// FMS_RTU --> FMS_SVR, FMS_FOP
	EFT_TIMEDAT_RES,			// FMS_SVR --> FMS_RTU, FMS_FOP --> FMS_RTU
    EFT_EVT_BASEDATUPDATE_RESET,     // [2016/10/7 YKHONG] : FMS_SVR 내부 프로세스에서 사용 -- 기본 정보를 다시 로드하여 초기화 수행한다

	// ---------------------------------------------------------S
	// 공통으로 사용하는 DB 기본테이블(RTU, 감시장비, 센서, 사용자, 공통코드) 등이 변경된 경우 RTU 에게 알려준다.
	EFT_EVT_BASEDATUPDATE_REQ=8822,		// FMS_OP, FMS_SVR --> FMS_RTU
	EFT_EVT_BASEDATUPDATE_RES,			// FMS_RTU -> FMS_OP, FMS_SVR : DB테이블 변경을 인지 했다는 의미의 응답

    // ---------------------------------------------------------S
    // FMS-OP <--> FMS-SVR, FMS-RTU <--> FMS-SVR 간 네트웍 통신 확인
    EFT_NETALIVECHK_REQ=8900,   // Alive check request
    EFT_NETALIVECHK_RES,    // Alive check response

    // FMS_EQP --> FMS_RTU, FMS_RTU --> FMS_EQP
    EFT_FCLTNETINFO_REQ=8990,           // 설비(고정감시 운영SW 가 구동되는 PC) IP/PORT 정보를 요청
    EFT_FCLTNETINFO_RES,
	// ---------------------------------------------------------S
	// 기타
	//	EFT_SVRSYS_ERR=5822,		// FMS_SVR --> FMS_RTU : FMS-SVR 내부 원인(DB 오라틀장애, 기타)에 의해 RTU데이터 수신 불가 상황, ??!!??
	//	EFT_SVRREOK_RES,			// FMS_RTU --> FMS_SVR : 서버장애상황 해제되면..서버장애 해제 이벤트 생성하여 서버 전송**, ??!!??
	// ------------------------

	EFT_EVT_FIRMWAREUPDATE_REQ = 9100,// 업데이트서버 2바이트 포트넘버
	EFT_EVT_FIRMWAREUPDATE_NOTI,

	// [2016/11/14 YKHONG] : 추가
	EFT_FIRMWARE_VERSION_REQ=9110,	// FMS_FOP -> FMS_RTU : RTU 펌웨어 버젼 정보 요청
	EFT_FIRMWARE_VERSION_RES,		// FMS_RTU -> FMS_FOP
	// ----------------------------
	EFT_RTUSETTING_INFO_REQ=9200,	// FMS_FOP -> FMS_RTU : RTU에 하드코딩된 IP, SITE 정보를 요청한다.
	EFT_RTUSETTING_INFO_RES,		// FMS_RTU -> FMS_FOP
	
    EFT_EVT_FCLTNETWORK_ERR = 9900, // [2016/10/17 YKHONG] : 설비
    EFT_EVT_FCLTNETWORK_ERRACK,
	ECMD_MaxCount

} enumfms_cmd;

// Tag System Direction : Tag 전송/수신에 대한 Source/Destination 표기
typedef enum _enumfms_sysdir
{
	ESYSD_NONE = -1,
	ESYSD_RTUEQP,       // FMS_RTU --> RMS_EQP(감시장비)
	ESYSD_RTUFOP,       // FMS_RTU --> RMS_FOP
	ESYSD_RTUSVR,       // FMS_RTU --> RMS_SVR

	ESYSD_EQPRTU,		// FMS_EQP --> RMS_RTU 

	ESYSD_FOPRTU,		// FMS_OP --> FMS_RTU
	ESYSD_FOPSVR,       // FMS_OP --> FMS_SVR

	ESYSD_SVRRTU,		// FMS_SVR --> FMS_RTU
	ESYSD_SVRFOP,		// FMS_SVR --> FMS_FOP
	// -----------------------
	ESYSD_UPDRTU,        // UPD_SVR --> FMS_RTU 
	ESYSD_RTUUPD,        // FMS_RTU --> UPD_SVR
	ESYSD_UPDSVR,        // UPD_SVR --> fms_svr

    ESYSD_SVR2RSTRDB,  	// [2016/10/7 YKHONG] : FMS_SVR -> FMS_RST(RESTORERTU-DB.EXE) 
    ESYSD_RSTRDB2SVR, 
    ESYSD_SVR2DBRPLC,  	// [2016/10/7 YKHONG] : FMS_SVR -> FMS_DBR(FMSDBREPLICATIONMGR.EXE : RTU-DB동기화) 
    ESYSD_DBRPLC2SVR,

} enumfms_sysdir;

// ----------------------------------------------
// 장비 장애상태 등급 구분
// NORMAL(정상), WARNING(주의), MAJOR(경고), CRITICAL(위험) ??
typedef enum _enumfclt_grade // fms 장애등급 enum
{
	EFCLTGRADE_NONE = -1,
	EFCLTGRADE_NORMAL,       // 정상(NORMAL)
	EFCLTGRADE_CAUTION,		 // 주위(CAUTION)
	EFCLTGRADE_WARNING,      // 경고(WARNING)
	EFCLTGRADE_DANGER,       // 위험(DANGER)
	// ----------------
	EFCLTGRADE_MAXCOUNT,
	EFCLTGRADE_UNKNOWN = 99, // 장애등급을 결정할수 없는경우.
} enumfclt_grade;

// ----------------------------------------------
// 설비구분 (RMS감시장비, RMS운영SW, 환경센서,,,, )
typedef enum _enumfclt_type // fms 설비 구분 enum
{  
	EFCLT_NONE = -1,
	// -----------------------------
	EFCLT_RTU=1,			// RTU 설비
	EFCLT_DV,		
	EFCLT_RTUFIRMWUPSVR,

    EFCLT_FMS_SERVER = 4,
	/* 2017-03-22 */
	EFCLT_UPS	= 5, /* APC UPS */
    // -----------------------------
	// 11 ~ 98 : 감시장비
	EFCLT_EQP01=11,			//감시장비 11 -- FRE(고정감시 장비)
	                        //감시장비 12 -- FRS(고정감시 운영SW)
	                        //감시장비 13 -- SRE(준고정 장비)
	                        //감시장비 14 -- FDE(고정방탐 장비)
	// -----------------------------
	EFCLT_FCLTALL	= 99,	// 모든 설비
	// -----------------------------
	// 기타
	EFCLT_TIME		= 100,	// [2016/8/18 YKHONG] : 시간설정 
	EFCLT_DBSVR		= 101,	// 
	// TODO:
	// -----------------------------
	EFCLT_MAXCOUNT
} enumfclt_type;

// SNSR_TYPE	00	AI	Y
// SNSR_TYPE	01	DI	Y
// SNSR_TYPE	02	DO	Y
// SNSR_TYPE	03	감시장비	Y
typedef enum _enumsnsr_type // fms 설비 - 센서타입(ai , di, do, 감시 장비)
{
	ESNSRTYP_NONE = -1,
	ESNSRTYP_AI,
	ESNSRTYP_DI,
	ESNSRTYP_DO,
    ESNSRTYP_UNKNOWN=9
} enumsnsr_type;

// 세부 센서구분 (측정기#1, 신호처리보드, 온도센서,,,,
// db 테이블의 SNSR_SUB_TYPE 과 동일 개념
typedef enum _enumsnsr_sub // fms 설비 세부 구분 enum
{
	ESNSRSUB_NONE = -1,
	// -----------------------------------------
	// 센서타입1(DI) : 열, 연기, 누수, 문, 출입문 등
	// 100 ~ 298 DI 스타일의 RTU 센서를 의미
	ESNSRSUB_DI_ALL=99,			// DI 센서 ALL
	ESNSRSUB_DI_HEAT=100,		// 열 ON/OFF 상태정보
	ESNSRSUB_DI_SMOKE,			// 연기 ON/OFF 상태정보
	ESNSRSUB_DI_WATER,			// 누수 ON/OFF 상태정보 -- A WATER LEAKAGE
	ESNSRSUB_DI_DOOR,			// 출입문 ON/OFF 상태정보
	ESNSRSUB_DI_RECVIDEO,		// 영상녹화 ON/OFF 상태 #
	ESNSRSUB_DI_POWERGROUND,	// 접지(지락)

	/* APC UPS */
	ESNSRSUB_DI_UPS_LOW_INPUT,	/* 106 UPS INPUT Low Voltage */

	// -----------------------------------------
	// 센서타입2(DO) : 에어컨, 전등, 감시장비ONOFF
	// 200 ~ 298 DI 스타일의 RTU 센서를 의미
	ESNSRSUB_DO_AIRCON=200,		// 내난방기(에어컨) ON/OFF 상태정보, ON/OFF 제어 *************
	ESNSRSUB_DO_LIGHT,			// 전등 ON/OFF 상태정보, ON/OFF 제어 ***************

	ESNSRSUB_DO_POWERRTU=210, 	// RTU 자체 전원제어 센서종류 
	ESNSRSUB_DO_POWERELECCTL, 	// 전원제어기
	ESNSRSUB_DO_COMMON_POWERONOFF, // 212 (냉방기, 난방기 ) 전원 제어  
	// -----------------------------------------
	// 센서타입2(AI) : 온도, 습도, 전원감시 
	// 300 ~ 398 AI 스타일의 RTU 센서를 의미
	ESNSRSUB_AI_ALL=299,		// AI 센서 ALL
	ESNSRSUB_AI_TEMPERATURE=300,// 온도(섭씨)
	ESNSRSUB_AI_HUMIDITY,		// 습도

	// -----------------------------------------
	ESNSRSUB_AI_POWERVR=302,	// 상전압VR
	ESNSRSUB_AI_POWERVS,		// 상전압VS
	ESNSRSUB_AI_POWERVT,		// 상전압VT
	ESNSRSUB_AI_POWERFREQ,		// 주파수
   

	ESNSRSUB_AI_UPS_BATTERY_CHARGE,		// 306 Charge Of UPS Battery 
	ESNSRSUB_AI_UPS_BATTERY_REMAIN_TIME,// 307 Remaining Time  Of UPS Battery 

    ESNSRSUB_AI_RTUFIRMWUPSVR=398, //RTU 펌웨어 업데이트 관련
	// --------------감시장비
	// 400 ~ 498 DI 스타일의 감시장비 센서를 의미
	ESNSRSUB_EQP_ALL=399,		// 감시장비 ALL
	// ----------------------
	// (1) 고정감시
	ESNSRSUB_EQPFRE_POWER=400,	// 고정감시 전원 ON/OFF 상태정보, ON/OFF 제어 ***************
	ESNSRSUB_EQPFRE_RX=401,		// (측정기#1, #2, ,, )
	// ...						// (ESNSRSUB_EQP_RX + 1), (ESNSRSUB_EQP_RX + 2), (ESNSRSUB_EQP_RX + 3), (ESNSRSUB_EQP_RX + N)
	// ----------------------
	// (2) 고정감시운용SW
	ESNSRSUB_EQPFRS_RX=420,		// 고정감시운영상태
	// ...
	// ----------------------
	// (3) 준고정감시
	ESNSRSUB_EQPSRE_POWER=440,	// 준고정 전원 ON/OFF 상태정보, ON/OFF 제어 ***************
	ESNSRSUB_EQPSRE_RX=441,		// 측정수신기
	// ...
	// ----------------------
	// (4) 고정방탐
	ESNSRSUB_EQPFDE_POWER=460,	// 고정방탐 전원 ON/OFF 상태정보, ON/OFF 제어 ***************
	ESNSRSUB_EQPFDE_RX=461,		// 측정수신기
	// ...
	// ---------기타
	ESNSRSUB_TIME=699,			// [2016/8/18 YKHONG] : 시간설정 
    ESNSRSUB_UNKNOWN=999
} enumsnsr_sub;

// ----------------------------------
// 고정감시 제어단계 정보
// 전원제어 단계
// 고정감시장비, 전등, 에어컨
typedef enum _enumfcltonoffstep 		// 설비 전원제어 단계 enum
{
	EEQPCTL_STEP_NONE = 0,
	EEQPCTL_STEP1						// (eEQPCTL_STEP1 ~ MAX Step)
} enumfcltonoffstep;

// RTU, 고정감시장비, 전등, 에어컨 등의 제어, 각 임계 제어
// TFMS_COM_CD 의 CTL_TYPE_CD 로 정의
typedef enum _enumfcltctl_type // 설비 제어 명령 구분 enum
{
	// CTL_TYPE_CD	11	지역 설정			Y
	// CTL_TYPE_CD	12	설비 설정			Y
	// CTL_TYPE_CD	13	센서 설정			Y
	// CTL_TYPE_CD	14	AI임계치설정		Y
	// CTL_TYPE_CD	15	DI임계치설정		Y
	// CTL_TYPE_CD	16	DO제어				Y
	// CTL_TYPE_CD	17	장애DELAY설정		Y
	// CTL_TYPE_CD	18	실시간감시설정		Y
	// CTL_TYPE_CD	19	사용자설정			Y
	// CTL_TYPE_CD	20	메시지설정			Y
	// CTL_TYPE_CD	21	영상설정			Y
	// CTL_TYPE_CD	40	모니터링 데이터 요청Y

	EFCLTCTLTYP_NONE = -1,
	// CTL_TYPE_CD	18	실시간감시설정	Y
	EFCLTCTLTYP_CURRSLTREQ=18,		// 토폴로지맵에서 현재 데이터 요청(루프 시작/종료), 현재 데이터 요청(한번 쿼리 용)
	// CTL_TYPE_CD	14	AI임계치설정		Y
	// CTL_TYPE_CD	15	DI임계치설정		Y
	EFCLTCTLTYP_THRLDAI=14,			// AI 임계설정
	EFCLTCTLTYP_THRLDDI=15,			// DI 임계설정
	// ------------------
	// CTL_TYPE_CD	16	DO제어				Y
	EFCLTCTLTYP_DOCTRL=16,			// DO제어 (ON/OFF 등의 제어)
	// ------------------
	// CTL_TYPE_CD	40	모니터링 데이터 요청Y
	EFCLTCTLTYP_RSLTREQ=40,			// DO제어 (ON/OFF 등의 제어)
	// ------------------
	// TODO:
	// ------------------
    // CTL_TYPE_CD  50  RTU펌웨어업데이트
    EFCLTCTLTYP_RTUFIRMWUPSVR=50,
    EFCLTCTLTYP_BASEDATAUPDATE =60, // 기본정보 변경*  // [2016/10/21 YKHONG] : 추가 

	EFCLTCTLTYP_MAXCOUNT
} enumfcltctl_type;

// TFMS_COM_CD 의 CTL_CMD_CD 로 정의
typedef enum _enumfcltctl_cmd 			// 설비 제어 명령 구분 enum
{
	// CTL_CMD_CD	00	Off			Y
	// CTL_CMD_CD	01	On			Y
	// CTL_CMD_CD	10	추가		Y
	// CTL_CMD_CD	11	수정		Y
	// CTL_CMD_CD	12	삭제		Y
	// CTL_CMD_CD	13	요청		Y
	// CTL_CMD_CD	14	중지요청	Y
	// CTL_CMD_CD	15	강제종료	Y
	// CTL_CMD_CD	16	시작		Y
	// CTL_CMD_CD	17	종료		Y
	// CTL_CMD_CD	18	리셋		Y
	// CTL_CMD_CD	19	일시정지	Y
	EFCLTCTLCMD_NONE = -1,
	// ------------------
	EFCLTCTLCMD_OFF=0,			// DO제어 (OFF 제어)
	EFCLTCTLCMD_ON=1,			// DO제어 (ON 제어)
	// CTL_CMD_CD	11	수정		Y
	EFCLTCTLCMD_EDIT=11,		// 설정, 기존 것을 수정
	// ------------------
	EFCLTCTLCMD_QUERY=13,		// (현재) 모니터링 데이터를 요청한다. (한번 쿼리 용)
	// ------------------
	EFCLTCTLCMD_START=16,		// 실시간 감시 요청(루프 시작)  -- RTU 에서 OP로 : 10초 간견으로 모니터링 결과를 전송한다.
	EFCLTCTLCMD_END=17,			// 실시간 감시 요청(루프 종료)

    // 처리중(펌웨어 업데이트 중, 감시장비 전원 OFF 중, 감시장비 전원 ON 중..)
    EFCLTCTLCMD_ING=20,

    EFCLTCTLCMD_WORKREADY=30,   // 작업준비*  // [2016/10/21 YKHONG] 
    EFCLTCTLCMD_WORKREADYFAIL, // 작업준비-Fail : RTU 측에서 재 부팅중인 경우 FOP 에게 준비실패 cmd 전송 
    EFCLTCTLCMD_WORKREADYEND,   // 작업준비완료*      // ★  
    EFCLTCTLCMD_WORKRUN,        // 작업수행* 
    EFCLTCTLCMD_WORKCOMPLETE,   // 작업완료* 
	// ------------------
	// TODO:
	// ------------------
	EFCLTCTLCMD_MAXCOUNT
} enumfcltctl_cmd;

// 고정감시장비전원, 전등제어, 에어컨제어
typedef enum _enumfcltctlstatus 		// 설비 제어 상태값 enum
{
	EFCLTCTLST_NONE = 0,
	EFCLTCTLST_START,
	EFCLTCTLST_ING,
	EFCLTCTLST_END,
	EFCLTCTLST_MAXCOUNT
} enumfcltctlstatus;

// [2016/10/23 ykhong] : 추가
typedef enum _enumeqponoffrslt // 설비 제어단계에 대한 결과
{
    EEQPONOFFRSLT_OK = 1,
    EEQPONOFFRSLT_ERR_OVERLAPCMD, // 중복제어 에러임
    EEQPONOFFRSLT_UNKNOWN,
	// ------------------
	EEQPONOFFRSLT_EQPDISCONNECTED,  // 감시 장비 연결 상태 아님
	EEQPONOFFRSLT_RTUPWCTRLLR_NOTCNN,
	EEQPONOFFRSLT_EQPERROR,			// 감시 장비 에러 리턴
	EEQPONOFFRSLT_PROCTIMEOUTOVER,  // 전원제어 반자동/자동 프로세스 타임아웃
} enumeqponoffrslt;

// [2016/8/26 ykhong] : 
// DB Method
typedef enum _enumdbmethodtype // 설비 제어 상태값 enum
{
	EDBMETHOD_NONE = 0,
	EDBMETHOD_RSLT,		// 모니터링 서버 전송
	EDBMETHOD_EVENT,	// 장애 이벤트 서버 전송
	EDBMETHOD_CTRL,		// 전원제어(ON/OFF) 이력
	EDBMETHOD_THRLD,	// 항목별 임계 설정 전송
	EDBMETHOD_TIME,		// 시간설정 
	EDBMETHOD_MAXCOUNT
} enummethodtype;

// 임계 기준에 의한 장애 이외 기타 장애 발생에 대한 코드 정의
typedef enum _enumfclt_err
{
	EFCLTERR_NONE = 0,
	EFCLTERR_NOERROR		=   1, // [2016/8/25 YKHONG] : 추가
	EFCLTERR_NOTCONNET		=  -1,   // 센서(인식불가), 감시장비(네트웍 연결이 안됨)
	EFCLTERR_NORESPONSE		=  -2,   // 연결은 됐는데 응답이 없는 경우
	EFCLTERR_SVRTRANSERR	=  -70,   // 서버에 문제가 있어 정상적인 ACK 태그가 오지 않는 경우
	EFCLTERR_UNDEFINED		= -99,   // 장애등급을 결정할 수 없는경우.
} enumfclt_err;

// [2016/8/30 ykhong] : 센서(온도, 수신기Rx1,,) 등의 연결 상태
typedef enum _enumsnsrcnnstatus
{
	ESNSRCNNST_NONE = -1,
	// ------------------
	ESNSRCNNST_NORMAL, // 정상연결(센서 OR 네트웍 OR COM포트)
	ESNSRCNNST_ERROR,  // 연결 되어 있지 않음 : 센서(인식불가), 감시장비(네트웍 연결이 안됨)
	ESNSRCNNST_NORES,  // 응답없음(반응없음)
} enumsnsrcnnstatus;


/* 2016/08/22 ver3.0 반영 */
// 테이블(국소, RTU, 감시장비, 센서, 공통코드)
typedef enum _enumdbeditinfo
{
	EDBEDIT_ALL,
	EDBEDIT_SITE,
	EDBEDIT_RTU,
	EDBEDIT_EQP,
	EDBEDIT_SNSR,
	EDBEDIT_COMMONCD, // 공통코드
    EDBEDIT_DVR,
} enumdbeditinfo;

/* Update 진행 상태 */
typedef enum _enumfms_firmware_sts
{          
	EFRIMWATEUPDATE_NONE = -1,
	EFRIMWATEUPDATE_READY,          /* UPDATE 준비 요청 */
	EFRIMWATEUPDATE_START,          /* UPDATE 시작 상태 */
	EFRIMWATEUPDATE_DOWNLOAD,       /* FIRMWARE FILE DOWNLOAD 중 상태 */
	EFRIMWATEUPDATE_DOWNLOAD_ERR,   /* FIRMWARE FILE DOWNLOAD 실패 상태 */
	EFRIMWATEUPDATE_WRITE,          /* FIRMWARE FILE ROM WRITE 중 상태 */
	EFRIMWATEUPDATE_WRITE_ERR,      /* FIRMWARE FILE ROM WRITE 실패 상태 */
	EFRIMWATEUPDATE_END,            /* UPDATE 정상 종료 상태 */        
}enumfms_firmware_sts;

// 네트웍 REMOTE 구분
typedef enum _enumfms_remote_id
{
  EFMSREMOTE_NONE = -1,
  EFMSREMOTE_RTU,
  EFMSREMOTE_FSVR,      // FMS SERVER
  EFMSREMOTE_FSVR_RSTRRTUDB,  // FMS-SVR 의 RESTORERTU-DB
  EFMSREMOTE_FSVR_RTUDBREPLIMGR,  // FMS-SVR 의 FMSDBREPLICATIONMGR.EXE
  EFMSREMOTE_FOP,
  EFMSREMOTE_FIXEDANTPC,    // FIXED ANTENNA CTROL PC
  EFMSREMOTE_EQP,       // 감시장비SBC 에서 상주 데몬(환경감시,제어)
  EFMSREMOTE_RTUFIRMWUPSVR,   // Update Server
} enumfms_remote_id;

/*#################################################################################################
  Protocol Body - Common 
##################################################################################################*/
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fms_packet_header 				// fms app 네트웍 태킷 헤더(8 BYTE)
{
	unsigned short         shprotocol;   // (2 BYTE) op_fms_prot
	unsigned short         shopcode;     // (2 BYTE) ecmd_rms_eqptatus_req,, 
	unsigned int  nsize;        // (4 BYTE) 패킷헤더를 뺀 나머지 body 크기
	short		  shdirection;  // (2 BYTE) enumfms_sysdir
} __attribute__ ((packed)) fms_packet_header_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fms_packet_header 				// fms app 네트웍 태킷 헤더(8 BYTE)
{
	short         shprotocol;   // (2 BYTE) op_fms_prot
	short         shopcode;     // (2 BYTE) ecmd_rms_eqptatus_req,, 
	unsigned int  nsize;        // (4 BYTE) 패킷헤더를 뺀 나머지 body 크기
	short		  shdirection;  // (2 BYTE) enumfms_sysdir [업데이트 서버 포트 넘버]
} fms_packet_header_t;
#pragma pack(pop)
#endif

/* DB TID */
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fms_transdbdata
{
	UINT32 		tdatetime;					/* 날짜 시간(2016년 12월 12일 23시 58분 52초 001) -- "20161212235852.001" */
	char		sdbfclt_tid[MAX_DBFCLT_TID];/* 서버에 전송 후 _ACK 수신 시 레코드 ID */
} __attribute__ ((packed)) fms_transdbdata_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fms_transdbdata
{
	UINT32 		tdatetime;					/* 날짜 시간(2016년 12월 12일 23시 58분 52초 001) -- "20161212235852.001" */
	char		sdbfclt_tid[MAX_DBFCLT_TID];/* 서버에 전송 후 _ACK 수신 시 레코드 ID */
} fms_transdbdata_t;
#pragma pack(pop)
#endif

/* 설비 */
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fms_elementfclt
{
	int					nsiteid;				// 원격국 id
	int					nfcltid;				// fclt id
	enumfclt_type		efclt;					// 설비구분(대분류 또는 시스템 구분)
} __attribute__ ((packed)) fms_elementfclt_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fms_elementfclt
{
	int					nsiteid;				// 원격국 id
	int					nfcltid;				// fclt id
	enumfclt_type		efclt;					// 설비구분(대분류 또는 시스템 구분)
} fms_elementfclt_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct _fms_collectrsltdido // di/do 스타일의 모니터링 결과
{
	enumsnsr_type		esnsrtype;		// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub		esnsrsub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxsub;			// 같은 센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	enumsnsrcnnstatus	ecnnstatus;		// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
	int					brslt;			// 모니터링 결과
} __attribute__((packed)) fms_collectrsltdido_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fms_collectrsltdido // di/do 스타일의 모니터링 결과
{
	enumsnsr_type		esnsrtype;		// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub		esnsrsub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxsub;			// 같은 센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	enumsnsrcnnstatus	ecnnstatus;		// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
	int					brslt;			// 모니터링 결과
} fms_collectrsltdido_t;
#pragma pack(pop)
#endif


#if defined(__linux__) || defined(__GNUC__)
typedef struct _fms_collectrslt_aidido // di/do 수집데이터
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분
	fms_transdbdata_t     tdbtans;		// rtu-svr 간 db 트랜젝션 확인을 위한 정보
	// ----------------------------------------------------
	int					nfcltsubcount;	// [2016/8/10 ykhong] : 그룹으로 전송할 모니터링 결과 갯수! (efcltsub count)
} __attribute__((packed)) fms_collectrslt_aidido_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fms_collectrslt_aidido // di/do 수집데이터
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분
	fms_transdbdata_t     tdbtans;		// rtu-svr 간 db 트랜젝션 확인을 위한 정보
	// ----------------------------------------------------
	int					nfcltsubcount;	// [2016/8/10 ykhong] : 그룹으로 전송할 모니터링 결과 갯수! (efcltsub count)
} fms_collectrslt_aidido_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_collectrslt_dido 
{
	fms_collectrslt_aidido_t aidido;
	fms_collectrsltdido_t	rslt[ MAX_GRP_RSLT_DI ];		// 최대 max_grp_rslt_di 가능 (nfcltsubcount 참조)

} __attribute__((packed)) fclt_collectrslt_dido_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_collectrslt_dido 
{
	fms_collectrslt_aidido_t aidido;
	fms_collectrsltdido_t	rslt[ MAX_GRP_RSLT_DI ];		// 최대 max_grp_rslt_di 가능 (nfcltsubcount 참조)

} fclt_collectrslt_dido_t;
#pragma pack(pop)
#endif


#if defined(__linux__) || defined(__GNUC__)
typedef struct _fms_collectrsltai
{
	enumsnsr_type		esnsrtype;	// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub		esnsrsub;	// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxsub;		// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	enumsnsrcnnstatus	ecnnstatus;	// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
	// ai 센서 감시 결과 : 온도, 습도, 전원-상전압vr, 전원-상전압vs, 전원-상전압vt, 전원-주파수 등
	float				frslt;		// ai 모니터링 결과
} __attribute__((packed)) fms_collectrsltai_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fms_collectrsltai
{
	enumsnsr_type		esnsrtype;	// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub		esnsrsub;	// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxsub;		// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	enumsnsrcnnstatus	ecnnstatus;	// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
	// ai 센서 감시 결과 : 온도, 습도, 전원-상전압vr, 전원-상전압vs, 전원-상전압vt, 전원-주파수 등
	float				frslt;		// ai 모니터링 결과
} fms_collectrsltai_t;
#pragma pack(pop)
#endif

// 제어 base : 원제어, 임계설정 제어 등
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fms_ctl_base_old
{
	fms_elementfclt_t		tfcltelem;		// 설비까지 구분
	enumsnsr_type			esnsrtype;		// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub			esnsrsub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int						idxsub;			// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	// ---------------------
	fms_transdbdata_t		tdbtans;		// rtu-svr 간 db 트랜젝션 확인을 위한 정보
	char					suserid[ MAX_USER_ID_OLD ]; // TFMS_CTLHIST_DTL insert 시 필요!
	enumfcltctl_type		ectltype;		// 제어 타입
	enumfcltctl_cmd			ectlcmd;		// 실제 제어 명령 : off 또는 on 명령, 임계제어
} __attribute__((packed)) fms_ctl_base_old_t;

typedef struct _fms_ctl_base
{
	fms_elementfclt_t		tfcltelem;		// 설비까지 구분
	enumsnsr_type			esnsrtype;		// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub			esnsrsub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int						idxsub;			// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	// ---------------------
	fms_transdbdata_t		tdbtans;		// rtu-svr 간 db 트랜젝션 확인을 위한 정보
	char					suserid[MAX_USER_ID]; // TFMS_CTLHIST_DTL insert 시 필요!
	unsigned int			op_ip;
	enumfcltctl_type		ectltype;		// 제어 타입
	enumfcltctl_cmd			ectlcmd;		// 실제 제어 명령 : off 또는 on 명령, 임계제어
} __attribute__((packed)) fms_ctl_base_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fms_ctl_base_old
{
	fms_elementfclt_t		tfcltelem;		// 설비까지 구분
	enumsnsr_type			esnsrtype;		// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub			esnsrsub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int						idxsub;			// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	// ---------------------
	fms_transdbdata_t		tdbtans;		// rtu-svr 간 db 트랜젝션 확인을 위한 정보
	char					suserid[ MAX_USER_ID _OLD ]; // TFMS_CTLHIST_DTL insert 시 필요!
	enumfcltctl_type		ectltype;		// 제어 타입
	enumfcltctl_cmd			ectlcmd;		// 실제 제어 명령 : off 또는 on 명령, 임계제어
} fms_ctl_base_old_t;

typedef struct _fms_ctl_base
{
	fms_elementfclt_t		tfcltelem;		// 설비까지 구분
	enumsnsr_type			esnsrtype;		// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub			esnsrsub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int						idxsub;			// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	// ---------------------
	fms_transdbdata_t		tdbtans;		// rtu-svr 간 db 트랜젝션 확인을 위한 정보
	char					suserid[MAX_USER_ID]; // TFMS_CTLHIST_DTL insert 시 필요!
	unsigned int			op_ip;
	enumfcltctl_type		ectltype;		// 제어 타입
	enumfcltctl_cmd			ectlcmd;		// 실제 제어 명령 : off 또는 on 명령, 임계제어
} fms_ctl_base_t;
#pragma pack(pop)
#endif

// 임계기준설정 데이터 (DI 스타일)
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fms_thrlddatadi		// 설비-di
{
	char			sname[SZ_MAX_NAME];			// 설비 이름
	int				bnotify;					// 통보 여/부
	enumfclt_grade  egrade;						// 경보 등급
	int				bnormalval;					// 정상값
	int				bfaultval;
	char			snormalname[SZ_MAX_NAME];	// ON 이름
	char			sfaultname[SZ_MAX_NAME];	// OFF 이름
} __attribute__((packed)) fms_thrlddatadi_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fms_thrlddatadi		// 설비-di
{
	char			sname[SZ_MAX_NAME];			// 설비 이름
	int				bnotify;					// 통보 여/부
	enumfclt_grade  egrade;						// 경보 등급
	int				bnormalval;					// 정상값
	int				bfaultval;
	char			snormalname[SZ_MAX_NAME];	// ON 이름 (1)
	char			sfaultname[SZ_MAX_NAME];	// OFF 이름(0)
} fms_thrlddatadi_t;
#pragma pack(pop)
#endif


#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_ctl_thrlddi 
{
	fms_ctl_base_t ctl_base;
	fms_thrlddatadi_t		tthrlddtdi;		// di 스타일의 임계기준 데이터
} __attribute__((packed)) fclt_ctl_thrlddi_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_ctl_thrlddi 
{
	fms_ctl_base_t ctl_base;
	fms_thrlddatadi_t		tthrlddtdi;		// di 스타일의 임계기준 데이터
} fclt_ctl_thrlddi_t;
#pragma pack(pop)
#endif


#if defined(__linux__) || defined(__GNUC__)
typedef struct _fms_faultdatadi // di 스타일의 장애정보 데이터
{
	enumsnsr_type		esnsrtype;	// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub		esnsrsub;	// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxsub;		// 같은 센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	// 판정기준 : di : boolen, di : 범위기준, all : 연결 안됨
	enumfclt_grade		egrade;			// 장비 장애판단 등급
	enumsnsrcnnstatus	ecnnstatus;		// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
	// 장애 발생 시의 모니터링 결과
	int					brslt;		// 모니터링 결과
    char    slabel01[ SZ_MAX_LABEL ]; // bRslt(0/1) 값에 대한 라벨 텍스트 
} __attribute__((packed)) fms_faultdatadi_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fms_faultdatadi // di 스타일의 장애정보 데이터
{
	enumsnsr_type		esnsrtype;	// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub		esnsrsub;	// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxsub;		// 같은 센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	// 판정기준 : di : boolen, di : 범위기준, all : 연결 안됨
	enumfclt_grade		egrade;			// 장비 장애판단 등급
	enumsnsrcnnstatus	ecnnstatus;		// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
	// 장애 발생 시의 모니터링 결과
	int					brslt;		// 모니터링 결과
    char    sLABEL01[ SZ_MAX_LABEL ]; // bRslt(0/1) 값에 대한 라벨 텍스트 
} fms_faultdatadi_t;
#pragma pack(pop)
#endif


#if defined(__linux__) || defined(__GNUC__)
typedef struct _fms_faultdataai
{
	enumsnsr_type		esnsrtype;		// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub		esnsrsub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxsub;			// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	// 판정기준 : di : boolen, ai : 범위기준
	enumfclt_grade		egrade;			// 장비 장애판단 등급
	enumsnsrcnnstatus	ecnnstatus;		// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
	// 장애 발생 시의 모니터링 결과 : 온도, 습도, 전원-상전압vr, 전원-상전압vs, 전원-상전압vt, 전원-주파수 등
	float				frslt;			// ai 모니터링 결과
} __attribute__((packed)) fms_faultdataai_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fms_faultdataai
{
	enumsnsr_type		esnsrtype;		// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub		esnsrsub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxsub;			// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	// 판정기준 : di : boolen, ai : 범위기준
	enumfclt_grade		egrade;			// 장비 장애판단 등급
	enumsnsrcnnstatus	ecnnstatus;		// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
	// 장애 발생 시의 모니터링 결과 : 온도, 습도, 전원-상전압vr, 전원-상전압vs, 전원-상전압vt, 전원-주파수 등
	float				frslt;			// ai 모니터링 결과
} fms_faultdataai_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct _fms_evt_faultaidi // 장애 발생 정보
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분
	fms_transdbdata_t     tdbtans;		// rtu-svr 간 db 트랜젝션 확인을 위한 정보
	// 장애 당시의 모니터링 결과
	int					nfcltsubcount;	// [2016/8/10 ykhong] : 그룹으로 전송할 모니터링 결과 갯수! (efcltsub count)
} __attribute__((packed)) fms_evt_faultaidi_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fms_evt_faultaidi // 장애 발생 정보
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분
	fms_transdbdata_t     tdbtans;		// rtu-svr 간 db 트랜젝션 확인을 위한 정보
	// 장애 당시의 모니터링 결과
	int					nfcltsubcount;	// [2016/8/10 ykhong] : 그룹으로 전송할 모니터링 결과 갯수! (efcltsub count)
} fms_evt_faultaidi_t;
#pragma pack(pop)
#endif


#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_evt_faultdi // 장애 발생 정보
{
	fms_evt_faultaidi_t fault_aidi;
	fms_faultdatadi_t		rslt[ MAX_GRP_RSLT_DI ];		// 최대 max_grp_rslt_di 가능 (nfcltsubcount 참조)
} __attribute__((packed)) fclt_evt_faultdi_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_evt_faultdi // 장애 발생 정보
{
	fms_evt_faultaidi_t fault_aidi;
	fms_faultdatadi_t		rslt[ MAX_GRP_RSLT_DI ];		// 최대 max_grp_rslt_di 가능 (nfcltsubcount 참조)
} fclt_evt_faultdi_t;
#pragma pack(pop)
#endif


// [2016/8/26 ykhong] : 모든 ack 태그는 fclt_svrtransactionack 구조체로 정리
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_svrtransactionack
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분
	fms_transdbdata_t     tdbtans;		// rtu-svr 간 db 트랜젝션 확인을 위한 정보
	// ----------------------------------------------------
	enummethodtype      emethodtype;    // db 트랜젝션 타입
	int                 iresult;		// db 트랜젝션 성공 여부에 대한 에러 코드 (성공 : 1, 실패 : 1보다 작은 error 코드)
} __attribute__((packed)) fclt_svrtransactionack_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_svrtransactionack
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분
	fms_transdbdata_t     tdbtans;		// rtu-svr 간 db 트랜젝션 확인을 위한 정보
	// ----------------------------------------------------
	enummethodtype      emethodtype;    // db 트랜젝션 타입
	int                 iresult;		// db 트랜젝션 성공 여부에 대한 에러 코드 (성공 : 1, 실패 : 1보다 작은 error 코드)
} fclt_svrtransactionack_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_ctl_requestrsltdata // fms 제어정보
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분 -- 전체 또는 일부 지정
	UINT32				timet;			// time_t  now; // "%y%m%d%h%m%s"
	// ---------------------
	enumfcltctl_type	ectltype;		// 제어 종류
	int                 ectlcmd;		// 제어명령(startloop, endloop, query,, )
} __attribute__((packed)) fclt_ctl_requestrsltdata_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_ctl_requestrsltdata // fms 제어정보
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분 -- 전체 또는 일부 지정
	UINT32				timet;			// time_t  now; // "%y%m%d%h%m%s"
	// ---------------------
	enumfcltctl_type	ectltype;		// 제어 종류
	int                 ectlcmd;		// 제어명령(startloop, endloop, query,, )
} fclt_ctl_requestrsltdata_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_reqtimedata
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분 -- 전체 또는 일부 지정
	UINT32				timet;			// time_t  now; // "%y%m%d%h%m%s"
	// ---------------------
} __attribute__((packed)) fclt_reqtimedata_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_reqtimedata
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분 -- 전체 또는 일부 지정
	UINT32				timet;			// time_t  now; // "%y%m%d%h%m%s"
	// ---------------------
} fclt_reqtimedata_t;
#pragma pack(pop)
#endif

// ### eft_timedat_res,			// fms_svr --> fms_rtu, fms_fop --> fms_rtu
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_timersltdata
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분 -- 전체 또는 일부 지정
	UINT32				timet;			// time_t  now; // "%y%m%d%h%m%s"
	// ---------------------
	UINT32				ttimerslt;		// time_t  now; // "%y%m%d%h%m%s"

} __attribute__((packed)) fclt_timersltdata_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_timersltdata
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분 -- 전체 또는 일부 지정
	UINT32				timet;			// time_t  now; // "%y%m%d%h%m%s"
	// ---------------------
	UINT32				ttimerslt;		// time_t  now; // "%y%m%d%h%m%s"

} fclt_timersltdata_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_ctlthrldcompleted // rtu설비-ai,di,감시장비,시간 등 기준설정정보 수신 완료
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분
	enumsnsr_type		esnsrtype;		// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub		esnsrsub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxsub;			// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	// -------------------------
	int					iresult;		// rtu 가 성공적으로 수신 받았는지 여부에 대한 에러 코드 (성공 : 1, 실패 : 1보다 작은 error 코드)
} __attribute__((packed)) fclt_ctlthrldcompleted_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_ctlthrldcompleted // rtu설비-ai,di,감시장비,시간 등 기준설정정보 수신 완료
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분
	enumsnsr_type		esnsrtype;		// 센서 타입(ai, di, do, eqp)
	enumsnsr_sub		esnsrsub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxsub;			// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	// -------------------------
	int					iresult;		// rtu 가 성공적으로 수신 받았는지 여부에 대한 에러 코드 (성공 : 1, 실패 : 1보다 작은 error 코드)
} fclt_ctlthrldcompleted_t;
#pragma pack(pop)
#endif

/* 20156 11 14 */
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_firmware_version
{
	int		nsiteid;	// 원격국 id
} __attribute__((packed)) fclt_firmware_version_t;

// ### eft_firmware_version_res,		// fms_rtu -> fms_fop
typedef struct _fclt_firmware_versionrslt
{
	int		nsiteid;			// 원격국 id
	char    szversion[ MAX_VERSION_SIZE ];		// 펌웨어 버젼 정보
} __attribute__((packed)) fclt_firmware_versionrslt_t;


typedef struct _fclt_rtusetting_info
{
	int		nsiteid;		// 원격국 id (fop가 갈고있는 site_id)
} __attribute__((packed)) fclt_rtusetting_info_req_t;

// ### eft_rtusetting_info_res,		// fms_rtu -> fms_fop
typedef struct _fclt_rtusetting_inforslt
{
	int		nsiteid;		// 원격국 id(fop에서 지정한 site_id)
	char    szip[ MAX_IP_SIZE ];
	char    szsubnetmask[ MAX_IP_SIZE ];
	char    szgateway[ MAX_IP_SIZE ];
	int		nsiteid_rslt;	// 원격국 id (rtu에 하드코딩된 site_id 정보)
} __attribute__((packed)) fclt_rtusetting_inforslt_t;

#elif defined(_WIN32)
#pragma pack(push, 1)

typedef struct _fclt_firmware_version
{
	int		nsiteid;	// 원격국 id
} fclt_firmware_version_t;

// ### eft_firmware_version_res,		// fms_rtu -> fms_fop

typedef struct _fclt_firmware_versionrslt
{
	int		nsiteid;			// 원격국 id
	char    szversion[ MAX_VERSION_SIZE ];		// 펌웨어 버젼 정보
} fclt_firmware_versionrslt_t;

typedef struct _fclt_rtusetting_info
{
	int		nsiteid;		// 원격국 id (fop가 갈고있는 site_id)
} fclt_rtusetting_info_req_t;

// ### eft_rtusetting_info_res,		// fms_rtu -> fms_fop

typedef struct _fclt_rtusetting_inforslt
{
	int		nsiteid;		// 원격국 id(fop에서 지정한 site_id)
	char    szip[ MAX_IP_SIZE ];
	char    szsubnetmask[ MAX_IP_SIZE ];
	char    szgateway[ MAX_IP_SIZE ];
	int		nsiteid_rslt;	// 원격국 id (rtu에 하드코딩된 site_id 정보)

} fclt_rtusetting_inforslt_t;

#pragma pack(pop)
#endif


#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_evt_dbdataupdate
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분
    enumfcltctl_type ectltype;          // 명령 타입 : eFCLTCtlTYP_BaseDataUpdate 
                                        // 세부 명령 : eFCLTCtlCMD_WorkReady or eFCLTCtlCMD_WorkRun or eFCLTCtlCMD_WorkComplete 
    enumfcltctl_cmd  ectlcmd;
                                        // 업데이트 발생한 테이블 정보(OP, SVR -> RTU) : enumDBEditInfo 참조 
    enumdbeditinfo  edbeditinfo; 

} __attribute__((packed)) fclt_evt_dbdataupdate_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_evt_dbdataupdate
{
	fms_elementfclt_t     tfcltelem;		// 설비까지 구분
	// -------------------------
                                        // [2016/10/19 ykhong] : 추가 ★★★★★★★★★★ 
    enumfcltctl_type ectltype;          // 명령 타입 : eFCLTCtlTYP_BaseDataUpdate 
                                        // 세부 명령 : eFCLTCtlCMD_WorkReady or eFCLTCtlCMD_WorkRun or eFCLTCtlCMD_WorkComplete 
    enumfcltctl_cmd  ectlcmd;
                                        // 업데이트 발생한 테이블 정보(OP, SVR -> RTU) : enumDBEditInfo 참조 
    enumdbeditinfo  edbeditinfo; 
    
	// 또는 rtu 응답 코드(0 : 실패, 1 : 성공)
} fclt_evt_dbdataupdate_t;
#pragma pack(pop)
#endif

/* 펌웨어 Update */
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_firmwareupdate
{
	fms_elementfclt_t		tfcltelem;   // 설비까지 구분 -- 전체 또는 일부 지정
	UINT32                    timet;       // time_t  now; // "%y%m%d%h%m%s"    
	int                     update_site_id;   // time_t  now; // "%y%m%d%h%m%s"    
	enumfms_firmware_sts	estatus;     // 펌웨어 업데이트 상태 
} __attribute__((packed)) fclt_firmwareupdate_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_firmwareupdate
{
	fms_elementfclt_t		tfcltelem;   // 설비까지 구분 -- 전체 또는 일부 지정
	UINT32                    timet;       // time_t  now; // "%y%m%d%h%m%s"    
	int                     update_site_id;   // time_t  now; // "%y%m%d%h%m%s"    
	enumfms_firmware_sts	estatus;     // 펌웨어 업데이트 상태 
} fclt_firmwareupdate_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_network_confirm
{
  int         nsiteid;    // 원격국 id
  enumfms_remote_id eremoteid;    // 네트웍 최초 접속 시 network_confirm id
  int         iresult;

} __attribute__((packed)) fclt_network_confirm_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_network_confirm
{
  int         nsiteid;    // 원격국 id
  enumfms_remote_id eremoteid;    // 네트웍 최초 접속 시 network_confirm id
  int         iresult;

} fclt_network_confirm_t;

#pragma pack(pop)
#endif


#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_fmssvr_alivecheckack
{
    enumfms_remote_id eremoteid;    // 네트웍 접속에 따른 id 구분
    int         iresult;
} __attribute__((packed)) fclt_fmssvr_alivecheckack_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_fmssvr_alivecheckack
{
    enumfms_remote_id eremoteid;    // 네트웍 접속에 따른 id 구분
    int         iresult;
} fclt_fmssvr_alivecheckack_t;
#pragma pack(pop)
#endif

// 고정감시 운영SW IP/PORT 정보 요청
//
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_evt_fcltnetworkinfo_req
{
    fms_elementfclt_t     tfcltelem;      // 설비까지 구분 -- 전체 또는 일부 지정
    UINT32              timet;          // time_t  now; // "%y%m%d%h%m%s"
} __attribute__((packed)) fclt_evt_fcltnetworkinfo_req_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_evt_fcltnetworkinfo_req
{
    fms_elementfclt_t     tfcltelem;      // 설비까지 구분 -- 전체 또는 일부 지정
    UINT32              timet;          // time_t  now; // "%y%m%d%h%m%s"
} fclt_evt_fcltnetworkinfo_req_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_evt_fcltnetworkinfo_res
{
    fms_elementfclt_t     tfcltelem;      // 설비까지 구분 -- 전체 또는 일부 지정
    UINT32                timet;          // time_t  now; // "%y%m%d%h%m%s"
    char                sip[ MAX_IP_SIZE ];
    int                 nport;
} __attribute__((packed)) fclt_evt_fcltnetworkinfo_res_t;

#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_evt_fcltnetworkinfo_res
{
    fms_elementfclt_t     tfcltelem;      // 설비까지 구분 -- 전체 또는 일부 지정
    UINT32                timet;          // time_t  now; // "%y%m%d%h%m%s"
    char                sip[ MAX_IP_SIZE ];
    int                 nport;
} fclt_evt_fcltnetworkinfo_res_t;

#pragma pack(pop)
#endif

/*#################################################################################################
  Protocol Body - RTU 
##################################################################################################*/
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_collectrslt_ai 
{
	fms_collectrslt_aidido_t aidido;
	fms_collectrsltai_t		rslt[MAX_GRP_RSLT_AI];		// 최대 max_grp_rslt_ai 가능 (nfcltsubcount 참조)
} __attribute__((packed)) fclt_collectrslt_ai_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_collectrslt_ai 
{
	fms_collectrslt_aidido_t aidido;
	fms_collectrsltai_t		rslt[MAX_GRP_RSLT_AI];		// 최대 max_grp_rslt_ai 가능 (nfcltsubcount 참조)
} fclt_collectrslt_ai_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_evt_faultai 
{
	fms_evt_faultaidi_t	fault_aidi;
	fms_faultdataai_t		rslt[ MAX_GRP_RSLT_AI ];		// 최대 max_grp_rslt_ai 가능 (nfcltsubcount 참조)
} __attribute__((packed)) fclt_evt_faultai_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_evt_faultai 
{
	fms_evt_faultaidi_t	fault_aidi;
	fms_faultdataai_t		rslt[ MAX_GRP_RSLT_AI ];		// 최대 max_grp_rslt_ai 가능 (nfcltsubcount 참조)
} fclt_evt_faultai_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct _fms_thrlddataai // rtu설비-ai 기준설정정보
{
	char	sname[SZ_MAX_NAME];			// 설비 이름
	int		bnotify;					// 통보 여/부
	float	foffsetval;					// 판정할때 오프셋 마진으로 계산
	float	fmin, fmax;					// 센서 입력 범위 : 최저, 최고

	// 경보여부 및 경보등급 판단기준
	// Dager(위험:DA), Warning(경고:WA), Caution(주위:CA), Normal(정상:NR)

	// 위험 : 센서 < 하위DA, 상위DA < 센서
	// 경고 : 하위DA <= 센서 < 하위WA, 상위WA <= 센서 < 상위DA
	// 주의 : 하위WA <= 센서 < 하위CA, 상위CA < 센서 <= 상위WA
	// 정상 : 하위CA < 센서 <= 상위CA

	// (0)하위DA, (1)하위WA, (2)하위CA, (3)상위CA, (4)상위WA, (5)상위DA
	float  frange[6];
} __attribute__((packed)) fms_thrlddataai_t; 
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fms_thrlddataai // rtu설비-ai 기준설정정보
{
	char	sname[SZ_MAX_NAME];			// 설비 이름
	int		bnotify;					// 통보 여/부
	float	foffsetval;					// 판정할때 오프셋 마진으로 계산
	float	fmin, fmax;					// 센서 입력 범위 : 최저, 최고

	// 경보여부 및 경보등급 판단기준
	// Dager(위험:DA), Warning(경고:WA), Caution(주위:CA), Normal(정상:NR)

	// 위험 : 센서 < 하위DA, 상위DA < 센서
	// 경고 : 하위DA <= 센서 < 하위WA, 상위WA <= 센서 < 상위DA
	// 주의 : 하위WA <= 센서 < 하위CA, 상위CA < 센서 <= 상위WA
	// 정상 : 하위CA < 센서 <= 상위CA

	// (0)하위DA, (1)하위WA, (2)하위CA, (3)상위CA, (4)상위WA, (5)상위DA
	float  frange[6];
} fms_thrlddataai_t; 
#pragma pack(pop)
#endif

// 센서 임계기준 설정 정보
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_ctl_thrldai 
{
	fms_ctl_base_t ctl_base;
	fms_thrlddataai_t     tthrlddtai;
} __attribute__((packed)) fclt_ctl_thrldai_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_ctl_thrldai 
{
	fms_ctl_base_t ctl_base;
	fms_thrlddataai_t     tthrlddtai;
} fclt_ctl_thrldai_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_ctrl_snsronoff 
{
	fms_ctl_base_t ctl_base;
	int		isnsrhwproperty;  // "하드웨어 종류" *************
} __attribute__((packed)) fclt_ctrl_snsronoff_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_ctrl_snsronoff 
{
	fms_ctl_base_t ctl_base;
	int		isnsrhwproperty;  // "하드웨어 종류" *************
} fclt_ctrl_snsronoff_t;
#pragma pack(pop)
#endif

/*#################################################################################################
  PROTOCOL BODY - EQP 
##################################################################################################*/
// ACK 신호 : FMS_RTU --> RMS_EQP  // RTU가 감시장비 모니터링 결과 수신 확인
// ### EFT_EQPDAT_ACK,		// FMS_RTU --> RMS_EQP
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_alivecheckack
{
	fms_elementfclt_t     tfcltelem;	// 설비까지 구분 -- 전체 또는 일부 지정
	UINT32				  timet;		// time_t  now; // "%y%m%d%h%m%s"
} __attribute__((packed)) fclt_alivecheckack_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_alivecheckack
{
	fms_elementfclt_t     tfcltelem;	// 설비까지 구분 -- 전체 또는 일부 지정
	UINT32				  timet;		// time_t  now; // "%y%m%d%h%m%s"
} fclt_alivecheckack_t;
#pragma pack(pop)
#endif

//*** 전원제어 (on/off) -- 전파감시장비 --------------s
// ## eft_eqpctl_power,			// rms_eqp --> fms_rtu : 자동 전원 on/off 시작!!!
// fms_op --> fms_rtu, fms_rtu --> rms_eqp, fms_svr, (서버 장애인 경우 fms-op 로 전송하지 않음.)
// 감시장비 전원제어 명령(on/off)
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_ctrl_eqponoff
{
	fms_ctl_base_t ctl_base;
	int			bautomode;		// 고정감시 운영sw 측에서 자동으로 off->on 하고자 하는 경우 ★★★
	int			isnsrhwproperty;  // "하드웨어 종류" *************
} __attribute__((packed)) fclt_ctrl_eqponoff_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_ctrl_eqponoff
{
	fms_ctl_base_t ctl_base;
	int			bautomode;		// 고정감시 운영sw 측에서 자동으로 off->on 하고자 하는 경우 ★★★
	int			isnsrhwproperty;  // "하드웨어 종류" *************
} fclt_ctrl_eqponoff_t;
#pragma pack(pop)
#endif


// 감시장비 전원제어 상태
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_ctrl_eqponoffrslt_old
{
	fms_ctl_base_old_t ctl_base;
	enumfcltonoffstep   emaxstep;       // 전원제어 프로세스가 거치는 max 단계
	enumfcltonoffstep   ecurstep;		// 현재단계: 1) 감시장비sbc 상주데몬 off, 2) 안테나제어pc os 종료, , 
	enumfcltctlstatus   ecurstatus;		// 단계별 처리 상태 : start, ing, end 중에 한가지 상태
    enumeqponoffrslt    ersult;
} __attribute__((packed)) fclt_ctrl_eqponoffrslt_old_t;

typedef struct _fclt_ctrl_eqponoffrslt
{
	fms_ctl_base_t ctl_base;
	enumfcltonoffstep   emaxstep;       // 전원제어 프로세스가 거치는 max 단계
	enumfcltonoffstep   ecurstep;		// 현재단계: 1) 감시장비sbc 상주데몬 off, 2) 안테나제어pc os 종료, , 
	enumfcltctlstatus   ecurstatus;		// 단계별 처리 상태 : start, ing, end 중에 한가지 상태
    enumeqponoffrslt    ersult;
} __attribute__((packed)) fclt_ctrl_eqponoffrslt_t;

#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_ctrl_eqponoffrslt_old
{
	fms_ctl_base_old_t ctl_base;
	enumfcltonoffstep   emaxstep;       // 전원제어 프로세스가 거치는 max 단계
	enumfcltonoffstep   ecurstep;		// 현재단계: 1) 감시장비sbc 상주데몬 off, 2) 안테나제어pc os 종료, , 
	enumfcltctlstatus   ecurstatus;		// 단계별 처리 상태 : start, ing, end 중에 한가지 상태
    enumeqponoffrslt    ersult;
} fclt_ctrl_eqponoffrslt_old_t;

typedef struct _fclt_ctrl_eqponoffrslt
{
	fms_ctl_base_t ctl_base;
	enumfcltonoffstep   emaxstep;       // 전원제어 프로세스가 거치는 max 단계
	enumfcltonoffstep   ecurstep;		// 현재단계: 1) 감시장비sbc 상주데몬 off, 2) 안테나제어pc os 종료, , 
	enumfcltctlstatus   ecurstatus;		// 단계별 처리 상태 : start, ing, end 중에 한가지 상태
    enumeqponoffrslt    ersult;
} fclt_ctrl_eqponoffrslt_t;
#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
// 전원제어 현재 진행 상태를 요청한다.
typedef struct _fclt_ctlstatus_eqponoff
{
	fms_elementfclt_t		tfcltelem;		// 설비까지 구분
	// ---------------------
	enumfcltctl_type		ectltype;		// 제어 타입 -- efcltctltyp_doctrl
	enumfcltctl_cmd			ectlcmd;		// (현재) 모니터링 데이터를 요청한다. (한번 쿼리 용) -- efcltctlcmd_query=13
} __attribute__((packed)) fclt_ctlstatus_eqponoff_t;

typedef struct _fclt_ctlstatus_eqponoffrslt
{
	fms_elementfclt_t		tfcltelem;		// 설비까지 구분
	// ---------------------
	// on / off 프로세스 구분
	// efcltctlcmd_off or efcltctlcmd_on
	enumfcltctl_cmd		ectlcmd;		// 실제 제어 명령 : off 또는 on 명령, 임계제어
	// ---------------------
	enumfcltonoffstep   emaxstep;       // 전원제어 프로세스가 거치는 max 단계
	enumfcltonoffstep   ecurstep;		// 현재단계: 1) 감시장비sbc 상주데몬 off, 2) 안테나제어pc os 종료, , 

	enumfcltctlstatus   ecurstatus;		// 단계별 처리 상태 : start, ing, end 중에 한가지 상태

	enumeqponoffrslt	ersult;			// 예1) ecurstatus 가 start 중인데 ersult 에 에러 결과를 리턴 할수 있음.
} __attribute__((packed)) fclt_ctlstatus_eqponoffrslt_t;

typedef struct _fclt_cnnstatus_eqp
{
	fms_elementfclt_t		tfcltelem;		// 설비까지 구분
	// ---------------------
} __attribute__((packed)) fclt_cnnstatus_eqp_t;

// ## eft_eqpctl_cnnstatus_res,    // fms_rtu -> fms-op, 감시장비 전원제어 : 네트웍 연결상태 결과

// 감시장비 네트웍 연결 상태 반환
typedef struct _fclt_cnnstatus_eqprslt
{
	fms_elementfclt_t		tfcltelem;		// 설비까지 구분
	// ---------------------
	enumsnsrcnnstatus		ecnnstatus;		// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
	int 					nfopcnncount;
} __attribute__((packed)) fclt_cnnstatus_eqprslt_t;

// [2016/11/24 ykhong] : 추가 ★★★★★★ 
// 장비 전원 제어 프로세스 옵션 정보 조회한다. 
// RTU는 응답으로..FCLT_CTRL_EQPOnOff_OptionOFF 또는 FCLT_CTRL_EQPOnOff_OptionON 반환 
typedef struct _fclt_ctrl_eqponoff_optionquery 
{ 
	// efcltctlcmd_off or efcltctlcmd_on 
	enumfcltctl_cmd   ectlcmd;  // 실제 제어 명령 : off 또는 on 명령, 임계제어 
	// 0 : get(rtu->fop), 1 : set(fop->rtu) 
	int      bgetorset;  // 0 : get(rtu->fop) 으로 지정 
} __attribute__((packed)) fclt_ctrl_eqponoff_optionquery_t;

typedef struct _fclt_ctrl_eqponoff_optionoff
{
	// efcltctlcmd_off or efcltctlcmd_on
	enumfcltctl_cmd			ectlcmd;		// 실제 제어 명령 : off 또는 on 명령, 임계제어
	int      bgetorset;  // 0 : get(rtu->fop), 1 : set(fop->rtu)  ★★★★★★  
	// 전원 스위치별 제어 딜레이 시간 지정
	int idelay_powerswitch_ms[ MAX_PWR_SWITCH_CNT ]; // 0번째 idelay 는 첫번째 스위치제어하고 기다리는 시간임.
	// 준 고정형은 0 번만 사용
	// 감시장비 off 명령(eft_eqpctl_power) eqp 로 송신 후
	// 처리step1 완료 상태(eft_eqpctl_res) 수신 시 까지 대기 시간 지정
	int iwaittime_step1_ms; // 디풀트 15초
	// 감시장비 os 종료 대기시간 지정 : 10초(디폴트)
	// (rtu는 핑 테스트를 통해 감시장비sbc 종료 확인)
	int iwaittime_eqposend_ms; // 10초(디폴트)
} __attribute__((packed)) fclt_ctrl_eqponoff_optionoff_t;

// 시간 단위 밀리초
typedef struct _fclt_ctrl_eqponoff_optionon
{
	// efcltctlcmd_off or efcltctlcmd_on
	enumfcltctl_cmd			ectlcmd;		// 실제 제어 명령 : off 또는 on 명령, 임계제어
	int      bgetorset;  // 0 : get(rtu->fop), 1 : set(fop->rtu)  ★★★★★★  
	// 전원 스위치별 제어 딜레이 시간 지정
	int idelay_powerswitch_ms[ MAX_PWR_SWITCH_CNT ]; // 0번째 idelay 는 첫번째 스위치제어하고 기다리는 시간임.
	//  - 감시장비 os 부팅확인(핑테스트) 대기시간 지정 : 20초
	// 부팅 확인 대기시간동안 필히 대기하고 다음(fmseqpdeamon.exe 에 네트웍 접속 시도) 진행
	int iwaittime_eqposstart_ms; // 20초(디폴트)
	// fmseqpdeamon.exe 에 네트웍 접속 시도 대기 시간 지정
	// 이 시간 동안 retryconnect 를 시도한다, 연결되면 다음을 진행
	int iwaittime_eqpdmncnn_ms; // 디풀트 10초
	// 감시장비 on 명령(eft_eqpctl_power) eqp 로 송신 후
	// 처리step1 완료 상태(eft_eqpctl_res) 수신 시 까지 대기 시간 지정
	int iwaittime_step1_ms; // 디풀트 15초
	// 처리step1 완료 상태 확인 후
	// 처리step2 완료 상태(eft_eqpctl_res) 수신 시 까지 대기 시간 지정
	int iwaittime_step2_ms; // 디풀트 15초
} __attribute__((packed)) fclt_ctrl_eqponoff_optionon_t;

#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_ctlstatus_eqponoff
{
	fms_elementfclt_t		tfcltelem;		// 설비까지 구분
	// ---------------------
	enumfcltctl_type		ectltype;		// 제어 타입 -- efcltctltyp_doctrl
	enumfcltctl_cmd			ectlcmd;		// (현재) 모니터링 데이터를 요청한다. (한번 쿼리 용) -- efcltctlcmd_query=13
} fclt_ctlstatus_eqponoff_t;

typedef struct _fclt_ctlstatus_eqponoffrslt
{
	fms_elementfclt_t		tfcltelem;		// 설비까지 구분
	// ---------------------
	// on / off 프로세스 구분
	// efcltctlcmd_off or efcltctlcmd_on
	enumfcltctl_cmd		ectlcmd;		// 실제 제어 명령 : off 또는 on 명령, 임계제어
	// ---------------------
	enumfcltonoffstep   emaxstep;       // 전원제어 프로세스가 거치는 max 단계
	enumfcltonoffstep   ecurstep;		// 현재단계: 1) 감시장비sbc 상주데몬 off, 2) 안테나제어pc os 종료, , 

	enumfcltctlstatus   ecurstatus;		// 단계별 처리 상태 : start, ing, end 중에 한가지 상태

	enumeqponoffrslt	ersult;			// 예1) ecurstatus 가 start 중인데 ersult 에 에러 결과를 리턴 할수 있음.
} fclt_ctlstatus_eqponoffrslt_t;

typedef struct _fclt_cnnstatus_eqp
{
	fms_elementfclt_t		tfcltelem;		// 설비까지 구분
	// ---------------------
} fclt_cnnstatus_eqp_t;

// ## eft_eqpctl_cnnstatus_res,    // fms_rtu -> fms-op, 감시장비 전원제어 : 네트웍 연결상태 결과

// 감시장비 네트웍 연결 상태 반환
typedef struct _fclt_cnnstatus_eqprslt
{
	fms_elementfclt_t		tfcltelem;		// 설비까지 구분
	// ---------------------
	enumsnsrcnnstatus		ecnnstatus;		// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
	int 					nfopcnncount;
} fclt_cnnstatus_eqprslt_t;
// [2016/11/24 ykhong] : 추가 ★★★★★★ 
// 장비 전원 제어 프로세스 옵션 정보 조회한다. 
// RTU는 응답으로..FCLT_CTRL_EQPOnOff_OptionOFF 또는 FCLT_CTRL_EQPOnOff_OptionON 반환 
typedef struct _fclt_ctrl_eqponoff_optionquery 
{ 
	// efcltctlcmd_off or efcltctlcmd_on 
	enumfcltctl_cmd   ectlcmd;  // 실제 제어 명령 : off 또는 on 명령, 임계제어 
	// 0 : get(rtu->fop), 1 : set(fop->rtu) 
	int      bgetorset;  // 0 : get(rtu->fop) 으로 지정 
} fclt_ctrl_eqponoff_optionquery_t;


typedef struct _fclt_ctrl_eqponoff_optionoff
{
	// efcltctlcmd_off or efcltctlcmd_on
	enumfcltctl_cmd			ectlcmd;		// 실제 제어 명령 : off 또는 on 명령, 임계제어
	int      bgetorset;  // 0 : get(rtu->fop), 1 : set(fop->rtu)  ★★★★★★  
	// 전원 스위치별 제어 딜레이 시간 지정
	int idelay_powerswitch_ms[ MAX_PWR_SWITCH_CNT ]; // 0번째 idelay 는 첫번째 스위치제어하고 기다리는 시간임.
	// 준 고정형은 0 번만 사용
	// 감시장비 off 명령(eft_eqpctl_power) eqp 로 송신 후
	// 처리step1 완료 상태(eft_eqpctl_res) 수신 시 까지 대기 시간 지정
	int iwaittime_step1_ms; // 디풀트 15초
	// 감시장비 os 종료 대기시간 지정 : 10초(디폴트)
	// (rtu는 핑 테스트를 통해 감시장비sbc 종료 확인)
	int iwaittime_eqposend_ms; // 10초(디폴트)
} fclt_ctrl_eqponoff_optionoff_t;

// 시간 단위 밀리초
typedef struct _fclt_ctrl_eqponoff_optionon
{
	// efcltctlcmd_off or efcltctlcmd_on
	enumfcltctl_cmd			ectlcmd;		// 실제 제어 명령 : off 또는 on 명령, 임계제어
	int      bgetorset;  // 0 : get(rtu->fop), 1 : set(fop->rtu)  ★★★★★★  
	// 전원 스위치별 제어 딜레이 시간 지정
	int idelay_powerswitch_ms[ MAX_PWR_SWITCH_CNT ]; // 0번째 idelay 는 첫번째 스위치제어하고 기다리는 시간임.
	//  - 감시장비 os 부팅확인(핑테스트) 대기시간 지정 : 20초
	// : 부팅 확인 대기시간동안 필히 대기하고 다음(fmseqpdeamon.exe 에 네트웍 접속 시도) 진행
	int iwaittime_eqposstart_ms; // 20초(디폴트)
	// fmseqpdeamon.exe 에 네트웍 접속 시도 대기 시간 지정
	// 이 시간 동안 retryconnect 를 시도한다, 연결되면 다음을 진행
	int iwaittime_eqpdmncnn_ms; // 디풀트 10초
	// 감시장비 on 명령(eft_eqpctl_power) eqp 로 송신 후
	// 처리step1 완료 상태(eft_eqpctl_res) 수신 시 까지 대기 시간 지정
	int iwaittime_step1_ms; // 디풀트 15초
	// 처리step1 완료 상태 확인 후
	// 처리step2 완료 상태(eft_eqpctl_res) 수신 시 까지 대기 시간 지정
	int iwaittime_step2_ms; // 디풀트 15초
} fclt_ctrl_eqponoff_optionon_t;

#pragma pack(pop)
#endif

// 설비 네트웍 장애 발생 정보 
#if defined(__linux__) || defined(__GNUC__)
typedef struct _fclt_evt_faultfclt 
{ 
    fms_elementfclt_t  tfcltelem;  // 설비까지 구분 -- 전체 또는 일부 지정 
    fms_transdbdata_t  tdbtans;     // rtu-svr 간 db 트랜젝션 확인을 위한 정보 
    enumsnsrcnnstatus  ecnnstatus; // 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
} __attribute__((packed)) fclt_evt_faultfclt_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct _fclt_evt_faultfclt 
{ 
    fms_elementfclt_t   tfcltelem;  // 설비까지 구분 -- 전체 또는 일부 지정 
    fms_transdbdata_t  tdbtans;  // rtu-svr 간 db 트랜젝션 확인을 위한 정보 
    enumsnsrcnnstatus   ecnnstatus;     // 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
} fclt_evt_faultfclt_t;

#pragma pack(pop)
#endif

/*#################################################################################################
  protocol body - rtu 전용 body 
##################################################################################################*/

/* local network */
#if defined(__linux__) || defined(__GNUC__)
typedef struct net_conf_data_
{
	char my_ip[ MAX_IP_SIZE ];              	/* ip */
	char my_gateway[ MAX_IP_SIZE ];         	/* gateway */
	char my_netmask[ MAX_IP_SIZE ];         	/* netmask */
} __attribute__ ((packed)) net_conf_data_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct net_conf_data_
{
	char my_ip[ MAX_IP_SIZE ];              	/* ip */
	char my_gateway[ MAX_IP_SIZE ];         	/* gateway */
	char my_netmask[ MAX_IP_SIZE ];         	/* netmask */
} net_conf_data_t;
#pragma pack(pop)
#endif

/* transfer와 control layer간 통신 시 사용  */
#if defined(__linux__) || defined(__GNUC__)
typedef struct inter_msg_
{
	int client_id;		/* 999 : 전체 */
	unsigned short msg;	/* protocl cmd id */
	void * ptrbudy;		/* protocol body point */
    char szdata[ MAX_DATA_SIZE ];           /* 시스템 네트워크 장애시 ip */
    int ndata;                              /* 시스템 네트워크 장애시 port */
    int fault_snsr_id[ MAX_GRP_RSLT_DI ];  /* threshold 장애 발생 시 센서의 id를 넣어준다 */
} inter_msg_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct inter_msg_
{
	int client_id;		/* 999 : 전체 */
	unsigned short msg;	/* protocl cmd id */
	void * ptrbudy;		/* protocol body point */
    char szdata[ MAX_DATA_SIZE ];
    int ndata;
    int fault_snsr_id[ MAX_GRP_RSLT_DI ];
} inter_msg_t;

#pragma pack(pop)
#endif

#if defined(__linux__) || defined(__GNUC__)
typedef struct allim_msg_
{
	unsigned short allim_msg;
	int ndata;
	void * ptrdata;
}allim_msg_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct allim_msg_
{
	unsigned short allim_msg;
	int ndata;
	void * ptrdata;
}allim_msg_t;

#pragma pack(pop)
#endif

/*====================================================================
 Protocol Message ID - Update
=====================================================================*/

typedef enum _enumupdate_cmd
{

	EUT_FIRMWARE_WRITE_STATE	= 0x3001, //Firmware Write 상태 - noti

    EUT_SERVER_CONNECT			= 0xFFFF, //Update 연결 요청 0x01 , 응답 0x00
	EUT_VERSION_INFO_REQ		= 0x0001, //Update 버전 요청 0x02
	EUT_VERSION_INFO_RSP		= 0x1001, //Update 버전 응답 0x00

	EUT_DOWNLOAD_DATA_REQ		= 0x0002, //단말 Firmware Download Data 요청 0x02 
	EUT_DOWNLOAD_DATA_RSP		= 0x1002, //단말 Firmware Download Data 응답 0x00

	EUT_DOWNLOAD_FINISH_REQ		= 0x0003, //단말 Firmware Download완료 요청 0x02 
	EUT_DOWNLOAD_FINISH_RSP		= 0x1003, //단말 Firmware Download완료 응답 0x00

	EUT_UPDATE_FORCED_REQ		= 0x0004, //Update 강제 종료 요청 0x01 , 0x02 
	EUT_UPDATE_FORCED_RSP		= 0x1004, //Update 강제 종료 응답 0x01 , 0x02 

	EUT_UPDATE_FINISH_REQ		= 0x0005, //Firmware Update 완료 0x02 
	EUT_UPDATE_FINISH_RSP		= 0x1005, //Firmware Update 응답 0x00

} enumupdate_cmd;

#if defined(__linux__) || defined(__GNUC__)
typedef struct update_packet_header_ {
  short	totsize;
  short	msgid;
  BYTE	directionl;
  BYTE	var1;
  int	rtuid;
  short paysize;
} __attribute__ ((packed)) update_packet_header_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct update_packet_header_ {
	short	totsize;
	short	msgid;
	BYTE	directionl;
	BYTE	var1;
	int	    rtuid;
	short   paysize;
}  update_packet_header_t;
#pragma pack(pop)
#endif


//9.1	Notification packet
//update_packet_header_t
// RUC -> UDS -> FMS

//9.2.1	Update 서버 연결 요청 packet
//update_packet_header_t
// ROS <- UDC 


//9.2.2	Update 서버 연결 응답 packet
//update_packet_header_t
// ROS -> UDC 
typedef enum _enumrtusystem_state
{
	EUT_RTU_STATE_NORMAL	= 0x00,	// 정상
	EUT_RTU_STATE_CONTROL	= 0x01,	// 제어 상태
    EUT_RTU_STATE_ERROR		= 0x02, // 장애 상태
    EUT_RTU_STATE_UPDATE	= 0x03, // 업데이트 진행 상태
} enumrtusystem_state;

//9.2.3	Update Version요청 packet
//update_packet_header_t
// RUC -> UDS 

//9.2.4	Update Version 응답 packet
//update_packet_header_t
// RUC <- UDS 
#if defined(__linux__) || defined(__GNUC__)
typedef struct update_version_data_ {
	BYTE	code;	//enumfirmware_code 참조
    BYTE	bVer[MAX_VER_SIZE];//0xHHLL (HH : 상위 버전,LL  : 하위 버전)
} __attribute__ ((packed)) update_version_data_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct update_version_data_ {
	BYTE	code;	//enumfirmware_code 참조
	BYTE	bVer[MAX_VER_SIZE];//0xHHLL (HH : 상위 버전,LL  : 하위 버전)
} update_version_data_t;
#pragma pack(pop)
#endif

typedef enum _enumfirmware_code
{
	EUT_FMCODE_EMBEDDE	= 0x01,	// Embedde PC //fms_logger
	EUT_FMCODE_MCU		= 0x02, // MCU //fms_mcu
	EUT_FMCODE_REMOTE	= 0x03,	// 리모컨 //fms_remote
	EUT_FMCODE_TEMPER   = 0x04,	// 온습도 //fms_temp 
	EUT_FMCODE_POWER	= 0x05,	// 전원제어기 //fsms_power
	EUT_FMCODE_RESERVED	= 0x06,	// 0x06 ~ : Reserved

    EUT_FMCODE_EMBEDDE_OBSER = 0x99, // Embedde PC obser //fms_observer

    EUT_FMCODE_MAX_COUNT = 7,

} enumfirmware_code;

//9.2.5	단말 Firmware Download 요청 packet
//update_packet_header_t
// RUC -> UDS 
#if defined(__linux__) || defined(__GNUC__)
typedef struct update_download_req_ {
	BYTE	code;	//enumfirmware_code 참조
} __attribute__ ((packed)) update_download_req_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct update_download_req_ {
	BYTE	code;	//enumfirmware_code 참조
} update_download_req_t;
#pragma pack(pop)
#endif

//9.2.6	단말 Firmware Download 응답 packet
//update_packet_header_t
// RUC <- UDS 
/*
- 단말이 요청한 Firmware File을 전송한다.
- 서버는 Firmware File은 한번에 최대 1024byte까지 전송하고 전송시 시퀀스 번호를 부여한다.
  단말은 시퀀스 번호가 일치하지 않을 때 Download 실패 정보를 포함하여
  단말 Update 강제 종료 요청 Packet을 전송한다
- 서버는 새로운 펌웨어 데이터를 보낼 경우 Index는 다시 0으로 시작한다
- 서버는 단말에서 Firmware Download 응답 ACK Packet이 오면 다음 시퀀스를 보낸다
  만약 ACK가 5초 이내 오지 않으면 Firmware Downlode 과정을 중단하고 실패 처리
*/
#if defined(__linux__) || defined(__GNUC__)
typedef struct update_download_rsp_ {
	int		indx;		//1~ 1000000 	펌웨어 파일 전송 순번
	int		totsize;	//펌웨어 전체 사이즈 (8 바이트 해쉬길이 포함)
	short	sendsize;	//현재 전송하는 펌웨어 사이즈
	BYTE	data[1024]; //현재 전송하는 펌웨어 데이터
} __attribute__ ((packed)) update_download_rsp_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct update_download_rsp_ {
	int		indx;		//1~ 1000000 	펌웨어 파일 전송 순번
	int		totsize;	//펌웨어 전체 사이즈 (8 바이트 해쉬길이 포함)
	short	sendsize;	//현재 전송하는 펌웨어 사이즈
	BYTE	data[1024]; //현재 전송하는 펌웨어 데이터 (전체데이터의 8바이트 해쉬값 포함해야한다)
} update_download_rsp_t;
#pragma pack(pop)
#endif

//9.2.7	단말 Firmware Download완료 요청 packet
//update_packet_header_t
// RUC -> UDS 

//9.2.8	단말 Firmware Download완료 응답 packet
//update_packet_header_t
// RUC <- UDS 


//9.2.9	Update 강제 종료 요청 packet
//update_packet_header_t
// RUC <- UDS 
#if defined(__linux__) || defined(__GNUC__)
typedef struct update_forced_ {
	BYTE rslt; //0x01~0x0A	0x01 : Firmware 다운로드 실패 , 0x02 : Firmware Write 실패
} __attribute__ ((packed)) update_forced_t;
#elif defined(_WIN32)
#pragma pack(push, 1)
typedef struct update_forced_ {
	BYTE rslt; //enumfirmware_upstate 참조
} update_forced_t;
#pragma pack(pop)
#endif

typedef enum _enumfirmware_upstate
{
	EUT_FMRSLT_DOWNLOAD_FAIL	= 0x01,	// Firmware 다운로드 실패
	EUT_FMRSLT_WRITE_FAIL		= 0x02, // Firmware Write 실패
} enumfirmware_upstate;

//9.2.10	Update 강제 종료 요청 응답 packet
//update_packet_header_t
// RUC -> UDS 

//9.2.11	Firmware Update 완료 요청 packet
//update_packet_header_t
// RUC -> UDS 

//9.2.12	Firmware Update 완료 요청 응답 packet
//update_packet_header_t
// RUC <- UDS -> FMS 

#endif

