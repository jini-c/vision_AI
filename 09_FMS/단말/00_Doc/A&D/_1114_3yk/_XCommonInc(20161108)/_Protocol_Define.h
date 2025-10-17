// _Protocol_Define.h: interface for the C_Protocol_Define class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX__Protocol_Define_H__260BA2E1_0E49_4E08_9954_360D03834161__INCLUDED_)
#define AFX__Protocol_Define_H__260BA2E1_0E49_4E08_9954_360D03834161__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


// [2016/11/8 ykhong] : 검수전 2차 버젼 준비를 위한 방안 제시
// #define _ForTest_2Cha_Solution1		1







//////////////////////////////////////////////////////////////////////////
// 약어 규칙
// FACILITY(설비) ==> FCLT
// SENSOR(FMS 환경센서) ==> SNSR

// --------------------------------------
#define OP_FMS_PROT				0xE2D8

#define SZ_MAX_DATE             8        // "20161212"
#define SZ_MAX_TIME             10       // "235852.001"

#define SZ_MAX_NAME             20       // 설비 이름


// -----------------------------------------?????
// FCLT_TID : 전국적으로 Unique ID 생성
// FCLT_TID 컬럼 문자 길이


// 4 4 7 4 2 4 = 24 문자
// 년도(4) + 날짜(4) + 시간(초)(6) + 국소ID(4) + SNSR_TYPE(2) + 시퀀스(4)

#define MAX_DBFCLT_TID			(24+1)
#define MAX_USER_ID				(10+1)

#define MAX_GRP_RSLT_DI			16		// 여유있게 16개 까지 허용
#define MAX_GRP_RSLT_AI			6		// 여유있게 6개 까지 허용


// ###############################################################################s
/* [2016 년도 FMS 교체 사업 네트웍 패킷 정의]
2.1 통신 데이터 패킷 정의
    가) 패킷 헤더 : [PROTOCOL_ID (2)] + [OP CODE (2)] + [PACKET SIZE (4)] + [(2 byte) enumFMS_SYSDIR ]
    나) 패킷 BODY : [패킷 BODY (?) ]
	다) 구성 : [패킷 헤더] + [패킷 BODY]

2.1.2 패킷헤더 정의 (10 Byte)
    가) [PROTOCOL_ID (2)] : 패킷이 깨졌는지 정상인지 조사용 상수
        고정 상수값 : 0xE2D8

    나) [OP CODE (2)] : 패킷 OPERATION(COMMAND) CODE, 패킷 구분 용 ID
        예) _enumFMS_CMD : 요청코드 ID, 응답코드 ID .. 

    다) [PACKET SIZE (4)] : 패킷 헤더를 뺀 나머지 BODY 크기
	라) [(2 byte) enumFMS_SYSDIR ] : 전송/수신에 대한 Source/Destination 에 대한 방향 표기

2.1.3 패킷 BODY정의
    [패킷 BODY ………] : 패킷 BODY 크기는 요청/응답 코드에 따라 가변적
    
/**/
// ###############################################################################s

//////////////////////////////////////////////////////////////////////////
// 네트웍 APP Command 정의 (APP 패킷 태그 정의)

// 고정전파감시장비 == 고정감시장비

// FT : Fms network Tag (FT)
// THRLD : Threshold

// REQ : Request (요청)
// RES : Response (응답)
// ACK : Acknowledge (확인)

// ----------시스템 구분 ------s
// RTU : FMS-RTU -- Remote Terminal Unit(원격 단말 장치)
// SVR : FMS-SVR
// FOP : FMS-OP
// FRE : Fixed-RMS-EQP  (1.   고정형 감시장비)
// FRS : Fixed-RMS-SW   (1.1. 고정형 감시 운영SW)
// SRE : Semi-RMS-EQP   (2.   준고정형 감시장비)
// SRS : Semi-RMS-SW    (2.1. 준고정형 감시 운영SW)
// FDE : Fixed-DFS-EQP  (3.   고정형 방탐장비)

// ###############################################################################s

typedef enum _enumFMS_CMD // FMS APP 패킷 TAG
{
    eCMD_None = 2210,

	//################################################################s
	// == FMS_SVR 네트웍 접속 시 확인 절차 필요 == 
	eFT_NETWORK_CONFIRM_REQ = 2211,
	eFT_NETWORK_CONFIRM_RES,

	//################################################################s
	// == 환경감시 == 
	
	//*** RTU센서 모니터링 데이터 수집(일정 주기로 10분?)
	// RTU센서 모니터링 응답(결과) ----
	eFT_SNSRDAT_RES=2222,		// FMS_RTU -> FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
	eFT_SNSRDAT_RESACK,			// 수신 확인 : FMS_SVR -> FMS_RTU
	
	// =========================================================s
	//*** RTU센서 모니터링 데이터 실시간 수집(5초?)
	// ----- 상태 정보(실시간) 송신 프로세스 시작/종료
	eFT_SNSRCUDAT_REQ=2322,		// FMS_OP --> FMS_RTU
	eFT_SNSRCUDAT_RES,			//* 현재 데이터 : FMS_RTU -> FMS_OP

	// =========================================================s
	//*** 장애(fault)이벤트 --- 
	eFT_SNSREVT_FAULT=2422,		// FMS_RTU --> FMS_SVR, FMS_RTU --> FMS_OP
	eFT_SNSREVT_FAULTACK,		// 수신확인 : FMS_SVR --> FMS_RTU

	// =========================================================s
	//*** 전원제어 (ON/OFF) -- RTU설비(전등, 에어컨)
	// 전원제어 명령 ----
	eFT_SNSRCTL_POWER=2522,		// FMS_OP --> FMS_RTU, FMS_RTU --> FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
	eFT_SNSRCTL_POWERACK,		// 수신 확인 : FMS_SVR --> FMS_RTU
	// 전원제어에 대한 결과 ----
	eFT_SNSRCTL_RES,			// FMS_RTU -> FMS_SVR, FMS_OP
	eFT_SNSRCTL_RESACK,			// 수신 확인 : FMS_SVR --> FMS_RTU

	// =========================================================s
	//*** 임계기준 설정 
	// 종류1(Analog Input) : 온도, 습도
	// 종류2(Digital Input) : 누수, 열, 연기, 출입문

	// RTU센서 임계기준 설정 ---------------------------------------s
	eFT_SNSRCTL_THRLD=2622,		// FMS_OP --> FMS_RTU, FMS_RTU -> FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
	eFT_SNSRCTL_THRLDACK,		// 수신 확인 : FMS_SVR --> FMS_RTU
	// [2016/8/18 ykhong] : 추가
	eFT_SNSRCtlCompleted_THRLD,	// FMS_RTU -> FMS_OP : 임계기준 수정 완료를 OP에게 알려줌.
	
	
	// *********************************************************************s
    // == 감시장비 연계 == 

    //*** 감시장비 모니터링 데이터 ------------
	eFT_EQPDAT_REQ=3122,		// FMS_RTU --> RMS_EQP
	eFT_EQPDAT_RES,				// 응답 : RMS_EQP --> FMS_RTU, FMS_RTU -> FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
	eFT_EQPDAT_RESACK,			//  수신확인 : FMS_SVR -> FMS_RTU

	// 장비 : RTU가 감시장비 모니터링 결과 수신 했음을 확인
	eFT_EQPDAT_ACK,				// ACK 신호 : FMS_RTU --> RMS_EQP

	//*** 상태 정보(실시간) 송신 프로세스 -----
	// 상태 정보(실시간) 송신 프로세스 시작/종료
	eFT_EQPCUDAT_REQ=3322,		// FMS_OP --> FMS_RTU
	eFT_EQPCUDAT_RES,			//* 현재 데이터 : FMS_RTU -> FMS_OP

	//*** 장애(fault)이벤트 -------------------
	eFT_EQPEVT_FAULT=3422,		// FMS_RTU --> FMS_SVR, FMS_OP
	eFT_EQPEVT_FAULTACK,		// FMS_SVR --> FMS_RTU

	//*** 감시장비 전원제어 (ON/OFF)
	eFT_EQPCTL_POWER=3522,		// RMS_EQP --> FMS_RTU : 자동 전원 On/Off 시작!!!
								// FMS_OP --> FMS_RTU, FMS_RTU --> RMS_EQP, FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
	eFT_EQPCTL_POWERACK,		// 수신 확인 : FMS_SVR --> FMS_RTU
	// 전원제어에 대한 진행상태 정보 ---------
	eFT_EQPCTL_RES,				// RMS_EQP -> FMS_RTU, RMS_RTU -> FMS_SVR, FMS_OP
	eFT_EQPCTL_RESACK,			// 수신확인 : RMS_SVR -> FMS_RTU

	// [2016/11/8 ykhong] : 추가
	eFT_EQPCTL_POWEROPT_OFF=3526,	// 감시장비 전원제어 시 세부 제어 옵션 : 센서간 딜레이, 감시장비 응답 대기 시간 지정 등
	eFT_EQPCTL_POWEROPT_ON,			// 응답 확인 하지 않음

	
	//*** 감시장비 항목별 임계 설정 ----------
	eFT_EQPCTL_THRLD=3622,		// FMS_OP --> FMS_RTU, FMS_RTU -> FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
	eFT_EQPCTL_THRLDACK,		// 수신 확인 : FMS_SVR --> FMS_RTU
	// [2016/8/18 ykhong] : 추가
	eFT_EQPCtlCompleted_THRLD,	// FMS_RTU -> FMS_OP : 임계기준 수정 완료를 OP에게 알려줌.
    ////////////////////////////////////////////////////// ----e

	// 5000 ~  5500 : 서버 자체 네트웍 태그로 사용
	eFTSVR_EVT_BASEDATUPDATE_REQ=5000,

	// -------------------
	// 기타 - 시간동기화
	// -------------------
	// 
	// 시간동기화 : RTU - SVR - OP 간 시간 동기화
	eFT_TIMEDAT_REQ=7822,		// FMS_RTU --> FMS_SVR, FMS_FOP
	eFT_TIMEDAT_RES,			// FMS_SVR --> FMS_RTU, FMS_FOP --> FMS_RTU

	// ---------------------------------------------------------s
	// 공통으로 사용하는 DB 기본테이블(RTU, 감시장비, 센서, 사용자, 공통코드) 등이 변경된 경우 RTU 에게 알려준다.
	eFT_EVT_BASEDATUPDATE_REQ=8822,		// FMS_OP, FMS_SVR --> FMS_RTU
	eFT_EVT_BASEDATUPDATE_RES,			// FMS_RTU -> FMS_OP, FMS_SVR : DB테이블 변경을 인지 했다는 의미의 응답

	eFT_EVT_BASEDATUPDATE_RESET,     // [2016/10/7 ykhong] : FMS_SVR 내부 프로세스에서 사용 -- 기본 정보를 다시 로드하여 초기화 수행한다

	// [2016/10/3 ykhong] : 추가, FMS-OP <--> FMS-SVR, FMS-RTU <--> FMS-SVR 간 네트웍 통신 확인
	eFT_NETALIVECHK_REQ=8900,			// Alive check request
	eFT_NETALIVECHK_RES,				// Alive check response
	// ---------------------------------------------------------s

	// [2016/10/19 ykhong] : 고정감시 SBC 에서는 고정감시 운영SW에 네트웍으로 연결하여 상태 정보를 수집한다.
	// FMS_EQP --> FMS_RTU, FMS_RTU --> FMS_EQP
	eFT_FCLTNETINFO_REQ=8990,			// 설비(고정감시 운영SW 가 구동되는 PC) IP/PORT 정보를 요청
	eFT_FCLTNETINFO_RES,

	// 기타
//	eFT_SVRSYS_ERR=5822,		// FMS_SVR --> FMS_RTU : FMS-SVR 내부 원인(DB 오라틀장애, 기타)에 의해 RTU데이터 수신 불가 상황, ??!!??
//	eFT_SVRREOK_RES,			// FMS_RTU --> FMS_SVR : 서버장애상황 해제되면..서버장애 해제 이벤트 생성하여 서버 전송**, ??!!??
    // ------------------------

	eFT_EVT_FIRMWAREUPDATE_REQ = 9100,
	eFT_EVT_FIRMWAREUPDATE_NOTI,

	eFT_EVT_FCLTNetwork_ERR = 9900, // [2016/10/17 ykhong] : 설비
	eFT_EVT_FCLTNetwork_ERRACK,

    eCMD_MaxCount
} enumFMS_CMD; // 35 개 : [2016/8/26]


// [2016/8/10 ykhong] : 추가
// Tag System Direction : Tag 전송/수신에 대한 Source/Destination 표기
typedef enum _enumFMS_SYSDIR
{
	eSYSD_None = -1,
	eSYSD_RTUEQP,       // FMS_RTU --> RMS_EQP(감시장비)
	eSYSD_RTUFOP,       // FMS_RTU --> RMS_FOP
	eSYSD_RTUSVR,       // FMS_RTU --> RMS_FOP

	eSYSD_EQPRTU,		// FMS_EQP --> RMS_RTU 

	eSYSD_FOPRTU,		// FMS_OP --> FMS_RTU
	eSYSD_FOPSVR,       // FMS_OP --> FMS_SVR

	eSYSD_SVRRTU,		// FMS_SVR --> FMS_RTU
	eSYSD_SVRFOP,		// FMS_SVR --> FMS_FOP
	// -----------------------
	eSYSD_UPDRTU,       // UPD_SVR --> FMS_RTU 
	eSYSD_RTUUPD,       // FMS_RTU --> UPD_SVR (업로드 서버)
	eSYSD_UPDSVR,       // UPD_SVR --> FMS_SVR
	eSYSD_SVRUPD,       // UPD_SVR --> UPD_SVR // [2016/10/12 ykhong] : 
	// ---------------
	eSYSD_SVR2RSTRDB,		// [2016/10/7 ykhong] : FMS_SVR -> FMS_RST(RestoreRTU-DB.exe)
	eSYSD_RSTRDB2SVR,
	eSYSD_SVR2DBRPLC,		// [2016/10/7 ykhong] : FMS_SVR -> FMS_DBR(FMSDBReplicationMgr.exe : RTU-DB동기화)
	eSYSD_DBRPLC2SVR,
	eSYSD_SVR2SVR,		// 서버 자체에서 보낼때 사용

} enumFMS_SYSDIR;

// ----------------------------------------------
// 장비 장애상태 등급 구분
// NORMAL(정상), WARNING(주의), MAJOR(경고), CRITICAL(위험) ??
typedef enum _enumFCLT_GRADE // FMS 장애등급 ENUM
{
    eFCLTGRADE_None = -1,
	eFCLTGRADE_Normal,       // 정상(Normal)
    eFCLTGRADE_Caution,		 // 주위(Caution)
    eFCLTGRADE_Warning,      // 경고(Warning)
	eFCLTGRADE_Danger,       // 위험(Danger)
    // ------------------
    eFCLTGRADE_MaxCount,
	eFCLTGRADE_UnKnown = 99, // 장애등급을 결정할수 없는경우.
} enumFCLT_GRADE;


// FCLT_CD	001	데이터수집장치	Y
// FCLT_CD	002	영상			Y
// FCLT_CD	011	고정감시SBC 	Y
// FCLT_CD	012	고정감시운용	Y
// FCLT_CD	013	준고정			Y
// FCLT_CD	014	고정방탐		Y

// ----------------------------------------------
// 설비구분 (RMS감시장비, RMS운영SW, 환경센서,,,, )
typedef enum _enumFCLT_TYPE // FMS 설비 구분 ENUM
{
    eFCLT_None = -1,
	// -----------------------------
	eFCLT_RTU=1,			// RTU 설비
	eFCLT_DV,				// 영상
	eFCLT_RTUFIRMWUPSVR,	// RTU 펌웨어 업데이트 서버 

	// -----------------------------
	// 11 ~ 98 : 감시장비
	eFCLT_EQP01=11,			// 감시장비 11 -- FRE(고정감시 장비)
	eFCLT_EQP02=12,			// 감시장비 12 -- FRS(고정감시 운영SW)
	eFCLT_EQP03=13,			// 감시장비 13 -- SRE(준고정 장비)
	eFCLT_EQP04=14,			// 감시장비 14 -- FDE(고정방탐 장비)
	// -----------------------------
	eFCLT_FcltAll	=99,	// 모든 설비

	// -----------------------------
	// 기타
	eFCLT_TIME		=100,	// [2016/8/18 ykhong] : 시간설정 
	eFCLT_DBSVR		=101,	// 

    // TODO:
    // -----------------------------
    eFCLT_MaxCount
} enumFCLT_TYPE;


// SNSR_TYPE	00	AI		Y
// SNSR_TYPE	01	DI		Y
// SNSR_TYPE	02	DO		Y
// SNSR_TYPE	09	None	Y
typedef enum _enumSNSR_TYPE // FMS 설비 - 센서타입(AI , DI, DO, 감시 장비)
{
    eSNSRTYP_None = -1,
	eSNSRTYP_AI,
	eSNSRTYP_DI,
	eSNSRTYP_DO,
	eSNSRTYP_Unknown=9, // [2016/10/18 ykhong] : 추가, SNSR_TYPE 를 결정할수 없을때 사용
} enumSNSR_TYPE;

// ----------------------------------------------
// 세부 센서구분 (측정기#1, 신호처리보드, 온도센서,,,,
// db 테이블의 SNSR_SUB_TYPE 과 동일 개념
typedef enum _enumSNSR_SUB // FMS 설비 세부 구분 ENUM
{
    eSNSRSUB_None = -1,

	// -----------------------------------------
	// 센서타입1(DI) : 열, 연기, 누수, 문, 출입문 등
	// 100 ~ 298 DI 스타일의 RTU 센서를 의미
	eSNSRSUB_DI_ALL=99,			// DI 센서 ALL
	eSNSRSUB_DI_HEAT=100,		// 열 ON/OFF 상태정보
	eSNSRSUB_DI_SMOKE,			// 연기 ON/OFF 상태정보
	eSNSRSUB_DI_WATER,			// 누수 ON/OFF 상태정보 -- a water leakage
	eSNSRSUB_DI_DOOR,			// 출입문 ON/OFF 상태정보
	eSNSRSUB_DI_RECVIDEO,		// 영상녹화 On/Off 상태 #
	eSNSRSUB_DI_PowerGROUND,	// 접지(지락)

	// -----------------------------------------
	// 센서타입2(DO) : 에어컨, 전등, 감시장비OnOff
	// 200 ~ 298 DI 스타일의 RTU 센서를 의미
	eSNSRSUB_DO_AIRCON=200,		// 내난방기(에어컨) ON/OFF 상태정보, ON/OFF 제어 *************
	eSNSRSUB_DO_LIGHT,			// 전등 ON/OFF 상태정보, ON/OFF 제어 ***************

	// [2016/10/24 ykhong] : 추가
	// 수동전원제어
	eSNSRSUB_DO_PowerRTU=210,	// RTU 자체 전원제어 센서종류
	eSNSRSUB_DO_PowerElecCtl,	// 전원제어기

	// -----------------------------------------
	// 센서타입2(AI) : 온도, 습도, 전원감시 
	// 300 ~ 398 AI 스타일의 RTU 센서를 의미
	eSNSRSUB_AI_ALL=299,		// AI 센서 ALL
    eSNSRSUB_AI_TEMPERATURE=300,// 온도(섭씨)
	eSNSRSUB_AI_HUMIDITY,		// 습도

	// -----------------------------------------
	eSNSRSUB_AI_PowerVR=302,	// 상전압Vr
	eSNSRSUB_AI_PowerVS,		// 상전압Vs
	eSNSRSUB_AI_PowerVT,		// 상전압Vt
	eSNSRSUB_AI_PowerFreq,		// 주파수


	eSNSRSUB_AI_RTUFIRMWUPSVR=398,

	// --------------감시장비
	// 400 ~ 498 DI 스타일의 감시장비 센서를 의미
	eSNSRSUB_EQP_ALL=399,		// 감시장비 ALL
	// ----------------------
	// (1) 고정감시
	eSNSRSUB_EQPFRE_Power=400,	// 고정감시 전원 ON/OFF 상태정보, ON/OFF 제어 ***************
    eSNSRSUB_EQPFRE_RX=401,		// (측정기#1, #2, ,, )
	// ...						// (eSNSRSUB_EQP_RX + 1), (eSNSRSUB_EQP_RX + 2), (eSNSRSUB_EQP_RX + 3), (eSNSRSUB_EQP_RX + N)
	// ----------------------
	// (2) 고정감시운용SW
	eSNSRSUB_EQPFRS_RX=420,		// 고정감시운영상태
	// ...
	// ----------------------
	// (3) 준고정감시
	eSNSRSUB_EQPSRE_Power=440,	// 준고정 전원 ON/OFF 상태정보, ON/OFF 제어 ***************
	eSNSRSUB_EQPSRE_RX=441,		// 측정수신기
	// ...
	// ----------------------
	// (4) 고정방탐
	eSNSRSUB_EQPFDE_Power=460,	// 고정방탐 전원 ON/OFF 상태정보, ON/OFF 제어 ***************
	eSNSRSUB_EQPFDE_RX=461,		// 측정수신기
	// ...

	// ---------기타
	eSNSRSUB_TIME=699,			// [2016/8/18 ykhong] : 시간설정
    // ------------------

	eSNSRSUB_Unknown=999		// [2016/10/18 ykhong] : 추가, SNSR_SUB 을 결정할수 없을때 사용

} enumSNSR_SUB;

#define SNSR_ID__UNKNOWN   9999   // [2016/10/18 ykhong] : TFMS_EVENT_DTL 테이블의 SNSR_ID 항목을 채울 수 없을때 사용
#define SNSR_VAL__UNKNOWN  999999 // [2016/10/18 ykhong] : TFMS_EVENT_DTL 테이블의 SNSR_VAL 항목을 채울 수 없을때 사용

// ----------------------------------
// 고정감시 제어단계 정보
/*
// 고정감시장비 OFF Step
	1) 상주 데몬SW OFF
	2) 안테나제어PC OS Stable shutdown 
	3) SBC OS Stable shutdown 
	* OFF Step 은 역순
/**/ 

// 전원제어 단계
// 고정감시장비, 전등, 에어컨
typedef enum _enumFCLTOnOffStep // 설비 전원제어 단계 ENUM
{
    eEQPCTL_STEP_None = 0,
	eEQPCTL_STEP1, // (eEQPCTL_STEP1 ~ MAX Step)
} enumFCLTOnOffStep;

// #######################################################################

// RTU, 고정감시장비, 전등, 에어컨 등의 제어, 각 임계 제어
// TFMS_COM_CD 의 CTL_TYPE_CD 로 정의
typedef enum _enumFCLTCtl_TYPE // 설비 제어 명령 구분 ENUM
{
/*
CTL_TYPE_CD	11	지역 설정
CTL_TYPE_CD	12	설비 설정
CTL_TYPE_CD	13	센서 설정
CTL_TYPE_CD	14	AI임계치설정
CTL_TYPE_CD	15	DI임계치설정
CTL_TYPE_CD	16	DO제어
CTL_TYPE_CD	17	장애delay설정
CTL_TYPE_CD	18	실시간감시설정
CTL_TYPE_CD	19	사용자설정
CTL_TYPE_CD	20	메시지설정
CTL_TYPE_CD	21	영상설정
CTL_TYPE_CD	40	모니터링 데이터 요청
CTL_TYPE_CD	50	RTU펌웨어업데이트
CTL_TYPE_CD	60	기본정보 변경*  // [2016/10/21 ykhong] : 추가
/**/
	eFCLTCtlTYP_None = -1,

	// ------------------
	// CTL_TYPE_CD	18	실시간감시설정	Y
	eFCLTCtlTYP_CurRsltReq=18,		// 토폴로지맵에서 현재 데이터 요청(루프 시작/종료), 현재 데이터 요청(한번 쿼리 용)

	// CTL_TYPE_CD	14	AI임계치설정		Y
	// CTL_TYPE_CD	15	DI임계치설정		Y
	eFCLTCtlTYP_ThrldAI=14,			// AI 임계설정
	eFCLTCtlTYP_ThrldDI=15,			// DI 임계설정

	// ------------------
	// CTL_TYPE_CD	16	DO제어				Y
	eFCLTCtlTYP_DoCtrl=16,			// DO제어 (on/off 등의 제어)

	// ------------------
	// CTL_TYPE_CD	40	모니터링 데이터 요청Y
	eFCLTCtlTYP_RsltReq=40,			// DO제어 (on/off 등의 제어)

	// ------------------
	// CTL_TYPE_CD	50	RTU펌웨어업데이트
	eFCLTCtlTYP_RTUFIRMWUPSVR=50,

	eFCLTCtlTYP_BaseDataUpdate =60,	// 기본정보 변경*  // [2016/10/21 ykhong] : 추가
	// ------------------
	// TODO:	

	// ------------------
	eFCLTCtlTYP_MaxCount
} enumFCLTCtl_TYPE;


// TFMS_COM_CD 의 CTL_CMD_CD 로 정의
typedef enum _enumFCLTCtl_CMD  // 설비 제어 명령 구분 ENUM
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
// CTL_CMD_CD	30	작업준비*  // [2016/10/21 ykhong] : 
// CTL_CMD_CD	31	작업수행*
// CTL_CMD_CD	32	작업완료*

	eFCLTCtlCMD_None = -1,

	// ------------------
	eFCLTCtlCMD_Off=0,			// DO제어 (off 제어)
	eFCLTCtlCMD_On=1,			// DO제어 (on 제어)

	// CTL_CMD_CD	10	추가		Y
	eFCLTCtlCMD_Add=10,			// 추가

	// CTL_CMD_CD	11	수정		Y
	eFCLTCtlCMD_Edit=11,		// 설정, 기존 것을 수정

	// ------------------
	eFCLTCtlCMD_Query=13,		// (현재) 모니터링 데이터를 요청한다. (한번 쿼리 용)

	// ------------------
	eFCLTCtlCMD_Start=16,		// 실시간 감시 요청(루프 시작)  -- RTU 에서 OP로 : 10초 간견으로 모니터링 결과를 전송한다.
	eFCLTCtlCMD_End=17,			// 실시간 감시 요청(루프 종료)

	// ------------------
	// TODO:
	eFCLTCtlCMD_Ing=20,			// [2016/10/12 ykhong] : 처리중(펌웨어 업데이트 중, 감시장비 전원 OFF 중, 감시장비 전원 ON 중..)

	eFCLTCtlCMD_WorkReady=30,		// 작업준비*  // [2016/10/21 ykhong] : 
	eFCLTCtlCMD_WorkReadyFail,		// 작업준비-Fail : RTU 측에서 재 부팅중인 경우 FOP 에게 준비실패 cmd 전송
	eFCLTCtlCMD_WorkReadyEnd,		// 작업준비완료*
	eFCLTCtlCMD_WorkRun,			// 작업수행*
	eFCLTCtlCMD_WorkComplete,		// 작업완료*

	// ------------------
	eFCLTCtlCMD_MaxCount
} enumFCLTCtl_CMD;

// #######################################################################


/*
eFCLTCtlCMD_WorkReadyFail 를 없애고..
eFCLTCtlCMD_WorkReady 에 결과를 별도 변수로 확인!!!  예정....

// enumFCLTCtl_CMD 에 대한 리턴 값
typedef enum _enumFCLTCtlCMDRslt
{
	FCLTCtlCMDRslt_OK=23,
	FCLTCtlCMDRslt_OverlapCmd,
	FCLTCtlCMDRslt_Fail,
};
/**/



// 고정감시장비전원, 전등제어, 에어컨제어
typedef enum _enumFCLTCtlStatus // 설비 제어 상태값 ENUM
{
    eFCLTCtlSt_None = 0,
	eFCLTCtlSt_Start,
	eFCLTCtlSt_Ing,
	eFCLTCtlSt_End,
	// ------------------
	eFCLTCtlSt_MaxCount
} enumFCLTCtlStatus;

// [2016/10/23 ykhong] : 
typedef enum _enumEQPOnOffRslt // 설비 제어단계에 대한 결과
{
	eEQPOnOffRslt_OK = 1,
	eEQPOnOffRslt_ERR_OverlapCmd, // 중복제어 에러임
	eEQPOnOffRslt_UNKNOWN,
	// ------------------
	// [2016/11/8 ykhong] : 추가
	eEQPOnOffRslt_EQPDisconnected,  // 감시 장비 연결 상태 아님
	eEQPOnOffRslt_EQPError,			// 감시 장비 에러 리턴
	eEQPOnOffRslt_ProcTimeoutOver,  // 전원제어 반자동/자동 프로세스 타임아웃
	// ------------------
} enumEQPOnOffRslt;


// [2016/8/26 ykhong] : 
// DB Method
typedef enum _enumDBMethodType // 설비 제어 상태값 ENUM
{
    eDBMethod_None = 0,
	eDBMethod_Rslt,		// 모니터링 서버 전송
	eDBMethod_Event,	// 장애 이벤트 서버 전송
	eDBMethod_Ctrl,		// 전원제어(ON/OFF) 이력
	eDBMethod_Thrld,	// 항목별 임계 설정 전송
	eDBMethod_Time,		// 시간설정 
	// ------------------
#ifdef _ForTest_2Cha_Solution1
	eDBMethod_BaseDt_Snsr,		// 센서정보 변경
	eDBMethod_BaseDt_Fclt,		// 설비정보 변경
	eDBMethod_BaseDt_Site,		// 사이트정보 변경
#endif
	// ==================
	eDBMethod_MaxCount
} enumMethodType;


// 임계 기준에 의한 장애 이외 기타 장애 발생에 대한 코드 정의
typedef enum _enumFCLT_ERR
{
	eFCLTErr_None = 0,
	// ------------------
	eFCLTErr_NoERROR		=   1,    // [2016/8/25 ykhong] : 추가

	eFCLTErr_NotConnet		=  -1,    // 센서(인식불가), 감시장비(네트웍 연결이 안됨) ??
	eFCLTErr_NoResponse		=  -2,    // 연결은 됐는데 응답이 없는 경우 ??
	// ------------------
	// 서버 
	eFCLTErr_SvrTransErr	=  -70,   // 서버에 문제가 있어 정상적인 ACK 태그가 오지 않는 경우
	// ------------------
	eFCLTErr_Undefined		= -99,    // 장애등급을 결정할 수 없는경우.
} enumFCLT_ERR;

// [2016/8/30 ykhong] : 센서(온도, 수신기Rx1,,) 등의 연결 상태
typedef enum _enumSnsrCnnStatus
{
	eSnsrCnnST_None = -1,
	// ------------------
	eSnsrCnnST_Normal, // 정상연결(센서 or 네트웍 or COM포트)
	eSnsrCnnST_Error,  // 연결 되어 있지 않음 : 센서(인식불가), 감시장비(네트웍 연결이 안됨)
	eSnsrCnnST_NoRes,  // 응답없음(반응없음)
} enumSnsrCnnStatus;


// [2016/10/15 ykhong] : 추가
/*
===================
1. 열감지센서 : 1, 0, 9(알수없음)
2. 연기감지센서 : 
3. 출입문센서 : 
.........
------------------------
RX1 : 측정수신기 1 장비진단 상태 : 1, 0, 9(알수없음)
RX2 : 측정수신기 2 장비진단 상태
...
/**/
typedef enum _enumSnsrDIValue
{
	// ------------------
	SnsrDIValue_FALSE = 0,
	SnsrDIValue_TRUE = 1,
	SnsrDIValue_UnKnown=9,  // 알수없음
} enumSnsrDIValue;


// [2016/8/19 ykhong] : 변경된 DB 테이블 공지를 위한 선언
// 테이블(국소, RTU, 감시장비, 센서, 공통코드 등)
typedef enum _enumDBEditInfo
{
	eDBEdit_All,
	eDBEdit_Site,
	eDBEdit_Rtu,
	eDBEdit_Eqp, // 감시정비 정보
	eDBEdit_Snsr,
	eDBEdit_CommonCD, // 공통코드
	eDBEdit_DVR,
	eDBEdit_User,
	eDBEdit_UserSession,
} enumDBEditInfo;


// ########################################################################################
// [2016/9/6 알림] : < 신규 프로토콜 추가 >

/* Update 진행 상태 */
typedef enum _enumFMS_FIRMWARE_STS
{          
	eFRIMWATEUPDATE_NONE = -1,
	// start
	eFRIMWATEUPDATE_READY,          // Update 준비 요청
	eFRIMWATEUPDATE_START,          // Update 시작 상태
	eFRIMWATEUPDATE_DOWNLOAD,       // Firmware File Download 중 상태
	eFRIMWATEUPDATE_DOWNLOAD_ERR,   // Firmware File Download 실패 상태
	eFRIMWATEUPDATE_WRITE,          // Firmware File Rom Write 중 상태
	eFRIMWATEUPDATE_WRITE_ERR,      // Firmware File Rom Write 실패 상태
	eFRIMWATEUPDATE_END,            // Update 정상 종료 상태
}enumFMS_FIRMWARE_STS;

/*
// start
eFRIMWATEUPDATE_READY,          // Update 준비 요청 
eFRIMWATEUPDATE_START,          // Update 시작 상태 

// ing
eFRIMWATEUPDATE_DOWNLOAD,       // Firmware File Download 중 상태 
eFRIMWATEUPDATE_WRITE,          // Firmware File Rom Write 중 상태 

// fail
eFRIMWATEUPDATE_DOWNLOAD_ERR,   // Firmware File Download 실패 상태 
eFRIMWATEUPDATE_WRITE_ERR,      // Firmware File Rom Write 실패 상태 

// end
eFRIMWATEUPDATE_END,            // Update 정상 종료 상태
/**/

//////////////////////////////////////////////////////////////////////////
// 네트웍 REMOTE 구분
typedef enum _enumFMS_REMOTE_ID
{
	eFmsREMOTE_None = -1,
	eFmsREMOTE_RTU,
	eFmsREMOTE_FSVR,			// Fms Server
	eFmsREMOTE_FSVR_RstrRtuDB,	// FMS-SVR 의 RestoreRTU-DB
	eFmsREMOTE_FSVR_RtuDBRepliMgr,	// FMS-SVR 의 FMSDBReplicationMgr.exe
	eFmsREMOTE_FOP,
	eFmsREMOTE_FixedANTPC,		// Fixed Antenna Ctrol PC
	eFmsREMOTE_EQP,				// 감시장비SBC 에서 상주 데몬(환경감시,제어)
	eFmsREMOTE_RTUFIRMWUPSVR,
} enumFMS_REMOTE_ID;


#endif // !defined(AFX__Protocol_Define_H__260BA2E1_0E49_4E08_9954_360D03834161__INCLUDED_)
