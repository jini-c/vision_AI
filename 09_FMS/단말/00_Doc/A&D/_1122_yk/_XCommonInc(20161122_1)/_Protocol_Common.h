// _Protocol_Common.h: interface for the _Protocol_Common.
//////////////////////////////////////////////////////////////////////
#if !defined(AFX__Protocol_Common_H__BFCD3552_D115_4DC0_9F24_974FF4217CF4__INCLUDED_)
#define AFX__Protocol_Common_H__BFCD3552_D115_4DC0_9F24_974FF4217CF4__INCLUDED_

// [2016/7/13 ykhong] : 2016년도 FMS 교체사업 프로콜 작성 v1.0
// [2016/8/12 ykhong] : 2016년도 FMS 교체사업 프로콜 수정 v2.0
// [2016/8/19 ykhong] : 2016년도 FMS 교체사업 프로콜 수정 v3.0
// [2016/8/26 ykhong] : 2016년도 FMS 교체사업 프로콜 수정 v4.0


#if _MSC_VER > 1000
    #pragma once
#endif // _MSC_VER > 1000

#include "_Protocol_Define.h"

//////////////////////////////////////////////////////////////////////////
#pragma pack(1) // start pack --------------------s
//////////////////////////////////////////////////////////////////////////

// ###############################################################################s
typedef struct _FMS_PACKET_HEADER // FMS APP 네트웍 태킷 헤더(8 byte)
{
    unsigned short  shProtocol;			// (2 byte) OP_FMS_PROT
	short			shOPCode;			// (2 byte) eCMD_RMS_EQPTATUS_REQ,, 
    unsigned int	dwContentsSize;     // (4 byte) 패킷헤더를 뺀 나머지 BODY 크기
	short			shDirection;		// (2 byte) enumFMS_SYSDIR
} FMS_PACKET_HEADER;

// ###############################################################################s

typedef struct _FMS_TransDBData
{
	long			timeT;		// time_t  now; // "%Y%m%d%H%M%S"
	char			sDBFCLT_TID[MAX_DBFCLT_TID];// 서버에 전송 후 _ACK 수신 시 레코드 ID
} FMS_TransDBData;

// ###############################################################################s

// 설비 까지 구분
typedef struct _FMS_ElementFclt
{
	int					nSiteID;	// 원격국 ID
	int					nFcltID;	// FCLT_ID
	enumFCLT_TYPE		eFclt;		// 설비구분(대분류 또는 시스템 구분)
} FMS_ElementFclt;

// ###############################################################################s

typedef struct _FMS_CollectRsltDIDO // DI/DO 스타일의 모니터링 결과
{
	enumSNSR_TYPE		eSnsrType;		// 센서 타입(AI, DI, DO)
	enumSNSR_SUB		eSnsrSub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxSub;			// 같은 센서가 3개가 설치되어 있다면 index로 구분(0,1,,)

	enumSnsrCnnStatus	eCnnStatus;		// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
	int					bRslt;			// 모니터링 결과
} FMS_CollectRsltDIDO;


// ### eFT_SNSRDAT_RES  -- DI, DO
// ### eFT_SNSRCUDAT_RES : DI, DO	//* 현재 데이터 : FMS_RTU -> FMS_OP

// ### eFT_EQPDAT_RES,			// 응답 : RMS_EQP --> FMS_RTU, FMS_RTU -> FMS_SVR, FMS_OP
// ### eFT_EQPCUDAT_RES			//* 현재 데이터 : FMS_RTU -> FMS_OP

// ------------------------------------------------------
// 전원제어에 대한 결과 : DO 제어 결과 변경분에 대한 모니터링 결과를 DB 저장한다.
// ### eFT_SNSRCTL_RES,			// FMS_RTU -> FMS_SVR, FMS_OP
// FCLT_CollectRslt_DIDO 를 사용하여 DB 저장

// DI/DO 스타일의 개별 or 그룹 모니터링 데이터 전송 BODY

typedef struct _FMS_CollectRslt_AIDIDO // DI/DO 수집데이터
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분
	FMS_TransDBData     tDbTans;		// RTU-SVR 간 DB 트랜젝션 확인을 위한 정보
	// ----------------------------------------------------
	int					nFcltSubCount;	// [2016/8/10 ykhong] : 그룹으로 전송할 모니터링 결과 갯수! (eFcltSub count)
} FMS_CollectRslt_AIDIDO;

typedef struct _FCLT_CollectRslt_DIDO : public FMS_CollectRslt_AIDIDO // DI/DO 수집데이터
{
	// FMS_CollectRslt_DIDO 에서 상속
	FMS_CollectRsltDIDO	rslt[1];		// 최대 MAX_GRP_RSLT_DI 가능 (nFcltSubCount 참조)

} FCLT_CollectRslt_DIDO;


// ###############################################################################s

typedef struct _FMS_CollectRsltAI
{
	enumSNSR_TYPE		eSnsrType;	// 센서 타입(AI, DI, DO)
	enumSNSR_SUB		eSnsrSub;	// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxSub;		// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)

	enumSnsrCnnStatus	eCnnStatus;	// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.

	// AI 센서 감시 결과 : 온도, 습도, 전원-상전압Vr, 전원-상전압Vs, 전원-상전압Vt, 전원-주파수 등
	float				fRslt;		// AI 모니터링 결과
} FMS_CollectRsltAI;


// ###############################################################################s
// 제어 base : 원제어, 임계설정 제어 등

typedef struct _FMS_CTL_BASE
{
	FMS_ElementFclt			tFcltElem;		// 설비까지 구분
	enumSNSR_TYPE			eSnsrType;		// 센서 타입(AI, DI, DO)
	enumSNSR_SUB			eSnsrSub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int						idxSub;			// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	
	// ---------------------
	FMS_TransDBData			tDbTans;		// RTU-SVR 간 DB 트랜젝션 확인을 위한 정보
	char					sUserID[MAX_USER_ID]; // TFMS_CTLHIST_DTL insert 시 필요!
	
	enumFCLTCtl_TYPE		eCtlType;		// 제어 타입
	enumFCLTCtl_CMD			eCtlCmd;		// 실제 제어 명령 : Off 또는 On 명령, 임계제어
} FMS_CTL_BASE;


// ###############################################################################s
// 임계기준설정 데이터 (DI 스타일)

typedef struct _FMS_ThrldDataDI		// 설비-DI
{
	char			sName[SZ_MAX_NAME];			// 설비 이름
	int				bNotify;					// 통보 여/부
	enumFCLT_GRADE  eGrade;						// 경보 등급
	int				bNormalVal;					// 정상값
	int				bFaultVal;
	char			sNormalName[SZ_MAX_NAME];	// ON 이름
	char			sFaultName[SZ_MAX_NAME];	// OFF 이름

} FMS_ThrldDataDI;

// ################################################################

// RTU센서 임계기준 설정 ---- DI
// ### eFT_SNSRCTL_THRLD,		// DI : FMS_OP --> FMS_RTU, FMS_RTU -> FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)

//*** 감시장비 항목별 임계 설정
// ### eFT_EQPCTL_THRLD,		//      FMS_OP --> FMS_RTU, FMS_RTU -> FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)

typedef struct _FCLT_CTL_ThrldDI : public FMS_CTL_BASE // RTU설비-DI 기준설정정보
{
	// FMS_CTL_BASE 에서 상속 -- DI 임계 (RTU, EQP) 설정 제어
	FMS_ThrldDataDI		tThrldDtDI;		// DI 스타일의 임계기준 데이터
} FCLT_CTL_ThrldDI;

/*  [2016/10/28 ykhong] : 
FOP 에서 임계 수정하여 RTU 에 패킷(eFT_SNSRCTL_THRLD) 전송 시 
   FCLT_CTL_ThrldDI::FMS_TransDBData::sDBFCLT_TID
   FCLT_CTL_ThrldAI::FMS_TransDBData::sDBFCLT_TID
   항목 24 자리중 
   마지막 4자리를 센서 ID 로 채워서 전송 하기로 함.
/**/


// ###############################################################################s

typedef struct _FMS_FaultDataDI // DI 스타일의 장애정보 데이터
{
	enumSNSR_TYPE		eSnsrType;	// 센서 타입(AI, DI, DO)
	enumSNSR_SUB		eSnsrSub;	// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxSub;		// 같은 센서가 3개가 설치되어 있다면 index로 구분(0,1,,)
	
	// 판정기준 : DI : Boolen, DI : 범위기준, ALL : 연결 안됨
	enumFCLT_GRADE		eGrade;		// 장비 장애판단 등급
	enumSnsrCnnStatus	eCnnStatus;	// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.

	// 장애 발생 시의 모니터링 결과
	int					bRslt;		// 모니터링 결과

	// [2016/10/19 ykhong] : 추가
	char				sLABEL01[30]; // bRslt(0/1) 값에 대한 라벨 텍스트
} FMS_FaultDataDI;

// ###############################################################################s

typedef struct _FMS_FaultDataAI
{
	enumSNSR_TYPE		eSnsrType;		// 센서 타입(AI, DI, DO)
	enumSNSR_SUB		eSnsrSub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxSub;			// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)

	// 판정기준 : DI : Boolen, AI : 범위기준
	enumFCLT_GRADE		eGrade;			// 장비 장애판단 등급
	enumSnsrCnnStatus	eCnnStatus;		// 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.

	// 장애 발생 시의 모니터링 결과 : 온도, 습도, 전원-상전압Vr, 전원-상전압Vs, 전원-상전압Vt, 전원-주파수 등
	float				fRslt;			// AI 모니터링 결과
} FMS_FaultDataAI;


// =========================================================s
//*** 장애(fault)이벤트 --- DI, EQP
// ### eFT_SNEQPVT_FAULT,		// FMS_RTU --> FMS_SVR, FMS_OP
// ### eFT_EQPEVT_FAULT,		// 

typedef struct _FMS_EVT_FaultAIDI // 장애 발생 정보
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분
	FMS_TransDBData     tDbTans;		// RTU-SVR 간 DB 트랜젝션 확인을 위한 정보
	
	// 장애 당시의 모니터링 결과
	int					nFcltSubCount;	// [2016/8/10 ykhong] : 그룹으로 전송할 모니터링 결과 갯수! (eFcltSub count)
} FMS_EVT_FaultAIDI;

typedef struct _FCLT_EVT_FaultDI : public FMS_EVT_FaultAIDI // 장애 발생 정보
{
	// FMS_EVT_FaultAIDI 에서 상속
	FMS_FaultDataDI		rslt[1];		// 최대 MAX_GRP_RSLT_DI 가능 (nFcltSubCount 참조)

} FCLT_EVT_FaultDI;


// =========================================================e

// ### eFT_SNSRDAT_RESACK  -- RUT : AI, DI
// ### eFT_SNSRCTL_POWERACK : RTU설비(전등, 에어컨) 전원제어 이벤트 저장 트랜젝션 Ack
// ### eFT_SNSRCTL_RESACK,		// 수신 확인 : FMS_SVR --> FMS_RTU
// ### eFT_SNSRCTL_THRLDACK,	// AI,DI --  수신 확인 : FMS_SVR --> FMS_RTU

// ---------------------------------
// 장애 정보 서버저장 확인 -- AI, DI, EQP
// ### eFT_SNSREVT_FAULTACK		// 수신 확인 : FMS_SVR --> FMS_RTU
// ### eFT_EQPEVT_FAULTACK,		//

// ### eFT_EQPDAT_RESACK,		// 수신 확인 : FMS_SVR -> FMS_RTU
// ### eFT_EQPCTL_POWERACK,		// 수신 확인 : FMS_SVR --> FMS_RTU
// ### eFT_EQPCTL_RESACK,		// 수신 확인 : RMS_SVR -> FMS_RTU
// ### eFT_EQPCTL_THRLDACK,		// 수신 확인 : FMS_SVR --> FMS_RTU

// [2016/10/18 ykhong] : 추가
// ### eFT_EVT_FCLTNetwork_ERRACK  // 수신 확인 : FMS_SVR --> FMS_RTU

// [2016/8/26 ykhong] : 모든 ack 태그는 FCLT_SvrTransactionAck 구조체로 정리
typedef struct _FCLT_SvrTransactionAck
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분
	FMS_TransDBData     tDbTans;		// RTU-SVR 간 DB 트랜젝션 확인을 위한 정보
	// ----------------------------------------------------
	enumMethodType      eMethodType;    // DB 트랜젝션 타입
	int                 iResult;		// DB 트랜젝션 성공 여부에 대한 에러 코드 (성공 : 1, 실패 : 1보다 작은 Error 코드)
	
} FCLT_SvrTransactionAck;

// ###############################################################################s
// 현재 모니터링 데이터 요청! -- 공통으로 사용
// ------------##### s
// ### eFT_SNSRCUDAT_REQ,	// FMS_OP --> FMS_RTU
// ### eFT_EQPCUDAT_REQ,	// (실시간) 전송 시작 요청  : FMS_OP --> FMS_RTU

// 감시장비 모니터링 결과를 요청한다. 
// ### eFT_EQPDAT_REQ,		// FMS_RTU --> RMS_EQP

// 
// ----- 상태 정보(실시간) 송신 요청 명령 구조체 (시작/종료, 단순요청)
// RTU 에 현재 모니터링 데이터 요청을 할 수 있는데, 이때 10초 간격으로 계속 보내도록 요청할지 아니면
// 원하는 설비에 대해 한번만 보내도록 요청 할 수 있다.

typedef struct _FCLT_CTL_RequestRsltData // FMS 제어정보
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분 -- 전체 또는 일부 지정
	long				timeT;			// time_t  now; // "%Y%m%d%H%M%S"
	// ---------------------
	enumFCLTCtl_TYPE	eCtlType;		// 제어 종류
	int                 eCtlCmd;		// 제어명령(startLoop, endLoop, Query,, )
} FCLT_CTL_RequestRsltData;
// ------------##### e

// ###############################################################################s

// 시간동기화 : RTU - SVR - OP 간 시간 동기화
// ### eFT_TIMEDAT_REQ,			// FMS_RTU --> FMS_SVR, FMS_FOP

typedef struct _FCLT_ReqTimeData
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분 -- 전체 또는 일부 지정
	long				timeT;			// time_t  now; // "%Y%m%d%H%M%S"
	// ---------------------
} FCLT_ReqTimeData;

// ### eFT_TIMEDAT_RES,			// FMS_SVR --> FMS_RTU, FMS_FOP --> FMS_RTU

typedef struct _FCLT_TimeRsltData
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분 -- 전체 또는 일부 지정
	long				timeT;			// time_t  now; // "%Y%m%d%H%M%S"
	// ---------------------
	long				tTimeRslt;		// time_t  now; // "%Y%m%d%H%M%S"

} FCLT_TimeRsltData;

// ###############################################################################s
// [2016/8/18 ykhong] : 
// -- (AI, DI) : FMS_RTU -> FMS_OP : 기준 수정 완료를 OP에게 알려줌.
// ### eFT_SNSRCtlCompleted_THRLD,	// FMS_RTU -> FMS_OP : 임계기준 수정 완료를 OP에게 알려줌.
// ### eFT_EQPCtlCompleted_THRLD,	// FMS_RTU -> FMS_OP : 임계기준 수정 완료를 OP에게 알려줌.

typedef struct _FCLT_CtlThrldCompleted // RTU설비-AI,DI,감시장비,시간 등 기준설정정보 수신 완료
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분
	enumSNSR_TYPE		eSnsrType;		// 센서 타입(AI, DI, DO)
	enumSNSR_SUB		eSnsrSub;		// 세부 센서 구분(열, 연기, 누수, 문, 출입문 등)
	int					idxSub;			// 온도센서가 3개가 설치되어 있다면 index로 구분(0,1,,)

	// -------------------------
	int					iResult;		// RTU 가 성공적으로 수신 받았는지 여부에 대한 에러 코드 (성공 : 1, 실패 : 1보다 작은 Error 코드)
} FCLT_CtlThrldCompleted;


// ###############################################################################s
// [2016/8/19 ykhong] : 
// ### eFT_EVT_BASEDATUPDATE_REQ=8822,		// FMS_OP, FMS_SVR --> FMS_RTU
// ### eFT_EVT_BASEDATUPDATE_RES,			// FMS_RTU -> FMS_OP, FMS_SVR : DB테이블 변경을 인지 했다는 의미의 응답
// ### eFT_EVT_BASEDATUPDATE_RESET			// 서버 프로세스에서 사용

typedef struct _FCLT_EVT_DBDataUpdate
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분
	// -------------------------
	enumFCLTCtl_TYPE	eCtlType;		// 명령 타입 : eFCLTCtlTYP_BaseDataUpdate
	enumFCLTCtl_CMD		eCtlCmd;		// 세부 명령 : eFCLTCtlCMD_WorkReady or eFCLTCtlCMD_WorkRun or eFCLTCtlCMD_WorkComplete

	// [2016/10/25 ykhong] : 향후 반영!!
//	enumFCLTCtlCMDRslt  eRslt;		

	enumDBEditInfo		eDBEditInfo;    // 업데이트 발생한 테이블 정보(OP, SVR -> RTU) : enumDBEditInfo 참조
} FCLT_EVT_DBDataUpdate;


// ###############################################################################s
// ### eFT_EVT_FIRMWAREUPDATE_NOTI

/* 펌웨어 Update */
typedef struct _FCLT_FirmwareUpdate
{
	FMS_ElementFclt			tFcltElem;   // 설비까지 구분 -- 전체 또는 일부 지정
	long                    timeT;       // time_t  now; // "%Y%m%d%H%M%S"    
	int                     nUpdate_site_id; // (추가) 펌웨어 업데이트 대상 site  id   
	enumFMS_FIRMWARE_STS	eStatus;     // 펌웨어 업데이트 상태 
} FCLT_FirmwareUpdate;

// 주의 ! -- FMS-OP 에서는 APP_CTRL_FirmwareUpdate 구조체로 수신될 것임.


//################################################################s
// == FMS_SVR 네트웍 접속 시 확인 절차 필요 == 
// ### eFT_NETWORK_CONFIRM_REQ
// ### eFT_NETWORK_CONFIRM_RES 

// FOP 만 사용하도록 함.

typedef struct _FCLT_Network_Confirm
{
	int					nSiteID;		// 원격국 ID
	enumFMS_REMOTE_ID	eRemoteID;		// 네트웍 최초 접속 시 NETWORK_CONFIRM ID
	int					iResult;

} FCLT_Network_Confirm;

//################################################################s

// [2016/10/3 ykhong] : 추가, FMS-OP <--> FMS-SVR, FMS-RTU <--> FMS-SVR 간 네트웍 통신 확인
// ### eFT_NETALIVECHK_REQ=8900,	// Alive check request
// ### eFT_NETALIVECHK_RES,			// Alive check response

typedef struct _FCLT_FMSSVR_AliveCheckAck
{
	enumFMS_REMOTE_ID	eRemoteID;		// 네트웍 접속에 따른 ID 구분
	int					iResult;
} FCLT_FMSSVR_AliveCheckAck;

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

// DI 스타일의 개별 or 그룹 모니터링 데이터 전송 BODY
typedef struct _APP_CollectRslt_DIDO : public FMS_CollectRslt_AIDIDO // DI 수집데이터
{
	// FMS_CollectRslt_DIDO 에서 상속
	FMS_CollectRsltDIDO	rslt[20];		// 최대 MAX_GRP_RSLT_DI 가능 (nFcltSubCount 참조)

} APP_CollectRslt_DIDO;

// AI 스타일의 개별 or 그룹 모니터링 데이터 전송 BODY
typedef struct _APP_CollectRslt_AI : public FMS_CollectRslt_AIDIDO // DI 수집데이터
{
	// FMS_CollectRslt_DIDO 에서 상속
	FMS_CollectRsltAI	rslt[20];		// 최대 MAX_GRP_RSLT_DI 가능 (nFcltSubCount 참조)
	
} APP_CollectRslt_AI;


typedef struct _APP_EVT_FaultDI : public FMS_EVT_FaultAIDI // 장애 발생 정보
{
	// FMS_EVT_FaultDI 에서 상속
	FMS_FaultDataDI		rslt[20];		// 최대 MAX_GRP_RSLT_DI 가능 (nFcltSubCount 참조)
	
} APP_EVT_FaultDI;


typedef struct _APP_EVT_FaultAI : public FMS_EVT_FaultAIDI // 장애 발생 정보
{
	// FMS_EVT_FaultAI 에서 상속
	FMS_FaultDataAI		rslt[20];		// 최대 MAX_GRP_RSLT_DI 가능 (nFcltSubCount 참조)
	
} APP_EVT_FaultAI;


// [2016/10/12 ykhong] : 펌웨어 업데이트
typedef struct _APP_CTRL_FirmwareUpdate : public FMS_CTL_BASE
{
	// FMS_CTL_BASE 에서 상속 -- 펌웨어 업데이트 제어 결과(상태)
	int						nUpdate_site_id;
	enumFMS_FIRMWARE_STS	eStatus;     // 펌웨어 업데이트 상태 

} APP_CTRL_FirmwareUpdate;

// #####################################

// [2016/10/17 ykhong] : 추가 -- RTU, 감시장비, 영상장비 등 의 설비가 통신이 안되는 경우 에러 정보 전송

// ### eFT_EVT_FCLTNetwork_ERR = 9900, // [2016/10/17 ykhong] : 설비

// 설비 네트웍 장애 발생 정보
typedef struct _FCLT_EVT_FaultFCLT
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분 -- 전체 또는 일부 지정
	FMS_TransDBData     tDbTans;		// RTU-SVR 간 DB 트랜젝션 확인을 위한 정보
	enumSnsrCnnStatus	eCnnStatus;	    // 정상, (네트웍 연결 안됨 or 센서인식불가), 응답없음.
} FCLT_EVT_FaultFCLT;


// [2016/10/19 ykhong] : 고정감시 SBC 에서는 고정감시 운영SW에 네트웍으로 연결하여 상태 정보를 수집한다.

// FMS_EQP --> FMS_RTU, FMS_RTU --> FMS_EQP
// ### eFT_FCLTNETINFO_REQ=8990,			// 설비(고정감시 운영SW 가 구동되는 PC) IP/PORT 정보를 요청
// ### eFT_FCLTNETINFO_RES,


// 고정감시 운영SW IP/PORT 정보 요청
typedef struct _FCLT_EVT_FCLTNETWORKINFO_REQ
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분 -- 전체 또는 일부 지정
	long				timeT;			// time_t  now; // "%Y%m%d%H%M%S"
} FCLT_EVT_FCLTNETWORKINFO_REQ;

typedef struct _FCLT_EVT_FCLTNETWORKINFO_RES
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분 -- 전체 또는 일부 지정
	long				timeT;			// time_t  now; // "%Y%m%d%H%M%S"
	char                sIP[20];
	int                 nPort;
} FCLT_EVT_FCLTNETWORKINFO_RES;


#ifdef _ForTest_2Cha_Solution1 // 2차 수정 방안

// [2016/11/8 ykhong] : 기본정보(센서, 설비, 사이트) 변경에 대한 SVR 의 ACK 응답
typedef struct _FCLT_BASEDATA_SvrTransAck
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분
	FMS_TransDBData     tDbTans;		// RTU-SVR 간 DB 트랜젝션 확인을 위한 정보
	// ----------------------------------------------------
	enumMethodType      eMethodType;    // DB 트랜젝션 타입 (센서 or 설비 or 사이트 변경)
	enumFCLTCtl_CMD		eCtlCmd;		// 추가, 수정, 삭제
	int					iRecord_ID;		// SNSR_ID or FCLT_ID or SITE_ID
	// ----------------------------------------------------
	int                 iResult;		// DB 트랜젝션 성공 여부에 대한 에러 코드 (성공 : 1, 실패 : 1보다 작은 Error 코드)
} FCLT_BASEDATA_SvrTransAck;


typedef struct _FCLT_BASEDATA_Completed // RTU설비-AI,DI,감시장비,시간 등 기준설정정보 수신 완료
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분
	int					iRecord_ID;		// SNSR_ID or FCLT_ID or SITE_ID
	// -------------------------
	int					iResult;		// RTU 가 성공적으로 수신 받았는지 여부에 대한 에러 코드 (성공 : 1, 실패 : 1보다 작은 Error 코드)
} FCLT_BASEDATA_Completed;


#endif // end of #ifdef _ForTest_2Cha_Solution1


// -------------------------------s
//{
// [2016/11/14 ykhong] : 추가
// ### eFT_FIRMWARE_VERSION_REQ=9110,	// FMS_FOP -> FMS_RTU : RTU 펌웨어 버젼 정보 요청

typedef struct _FCLT_FIRMWARE_VERSION
{
	int		nSiteID;	// 원격국 ID
} FCLT_FIRMWARE_VERSION;

// ### eFT_FIRMWARE_VERSION_RES,		// FMS_RTU -> FMS_FOP

typedef struct _FCLT_FIRMWARE_VERSIONRslt
{
	int		nSiteID;			// 원격국 ID
	char    szVersion[20];		// 펌웨어 버젼 정보
} FCLT_FIRMWARE_VERSIONRslt;
//}
// -------------------------------e


// ### eFT_RTUSETTING_INFO_REQ=9200,	// FMS_FOP -> FMS_RTU : RTU에 하드코딩된 IP, SITE 정보를 요청한다.

typedef struct _FCLT_RTUSETTING_INFO
{
	int		nSiteID;		// 원격국 ID (FOP가 갈고있는 SITE_ID)
} FCLT_RTUSETTING_INFO_REQ;

// ### eFT_RTUSETTING_INFO_RES,		// FMS_RTU -> FMS_FOP

typedef struct _FCLT_RTUSETTING_INFORslt
{
	int		nSiteID;		// 원격국 ID(FOP에서 지정한 SITE_ID)
	char    szIP[20];
	char    szSubnetmask[20];
	char    szGateway[20];
	int		nSiteID_Rslt;	// 원격국 ID (RTU에 하드코딩된 SITE_ID 정보)

} FCLT_RTUSETTING_INFORslt;


//////////////////////////////////////////////////////////////////////////
#pragma pack() // end pack --------------------e
//////////////////////////////////////////////////////////////////////////

CString GetStr_FCLT_TYPE(enumFCLT_TYPE e);
CString GetStr_MethodType(enumMethodType e);
CString GetStr_FMS_CMD(enumFMS_CMD e);
CString GetStr_FMS_SYSDIR(enumFMS_SYSDIR e);

CString gfn_GetDBDateTime(time_t tm_time);

#endif // !defined(AFX__Protocol_Common_H__BFCD3552_D115_4DC0_9F24_974FF4217CF4__INCLUDED_)

