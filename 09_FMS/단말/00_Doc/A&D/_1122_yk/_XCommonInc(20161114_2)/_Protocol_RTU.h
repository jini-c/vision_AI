// _Protocol_RtuAI.h: interface for the C_Protocol_RtuAI class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX__PROTOCOL_RTUAI_H__B519295E_4B95_4516_B6C0_1046401655B9__INCLUDED_)
#define AFX__PROTOCOL_RTUAI_H__B519295E_4B95_4516_B6C0_1046401655B9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "_Protocol_Define.h"
#include "_Protocol_Common.h"

//////////////////////////////////////////////////////////////////////////
#pragma pack(1) // start pack --------------------s
//////////////////////////////////////////////////////////////////////////


// #############################################################s
// RTU 수집데이터

////============== AI ==================================================////

// ---------------------------------
// RTU(데이터수집장치 - 온도, 습도 등)

// ### eFT_SNSRDAT_RES    : AI
// ### eFT_SNSRCUDAT_RES  : AI //* 현재 데이터 : FMS_RTU -> FMS_OP

// AI 개별 or 그룹 모니터링 결과

typedef struct _FCLT_CollectRslt_AI : public FMS_CollectRslt_AIDIDO
{
	// FMS_CollectRslt_AI 에서 상속
	// -------------------------
	FMS_CollectRsltAI		rslt[1];		// 최대 MAX_GRP_RSLT_AI 가능 (nFcltSubCount 참조)
} FCLT_CollectRslt_AI;


// =========================================================s
//*** 장애(fault)이벤트 --- AI
// ### eFT_SNSREVT_FAULT,		// --- AI, FMS_RTU --> FMS_SVR, FMS_RTU --> FMS_OP

// 시설(FACILITY -- FCLT) 장애 발생 정보(이벤트)
// RTU -> SVR

typedef struct _FCLT_EVT_FaultAI : public FMS_EVT_FaultAIDI // 장애 발생 정보
{
	// FMS_EVT_FaultAIDI 에서 상속
	FMS_FaultDataAI		rslt[1];		// 최대 MAX_GRP_RSLT_AI 가능 (nFcltSubCount 참조)
} FCLT_EVT_FaultAI;


// ======================================================
// RTU센서 임계기준 설정 
// ### eFT_SNSRCTL_THRLD,		// -- AI : FMS_OP --> FMS_RTU, FMS_RTU -> FMS_SVR, FMS_OP

typedef struct _FMS_ThrldDataAI // RTU설비-AI 기준설정정보
{
	char	sName[SZ_MAX_NAME];			// 설비 이름
	int		bNotify;					// 통보 여/부
	float	fOffsetVal;					// 판정할때 오프셋 마진으로 계산
	float	fMin, fMax;					// 센서 입력 범위 : 최저, 최고

	// 경보여부 및 경보등급 판단기준
	// Dager(위험:DA), Warning(경고:WA), Caution(주위:CA), Normal(정상:NR)

	// 위험 : 센서 < 하위DA, 상위DA < 센서
	// 경고 : 하위DA <= 센서 < 하위WA, 상위WA <= 센서 < 상위DA
	// 주의 : 하위WA <= 센서 < 하위CA, 상위CA < 센서 <= 상위WA
	// 정상 : 하위CA < 센서 <= 상위CA

	// (0)하위DA, (1)하위WA, (2)하위CA, (3)상위CA, (4)상위WA, (5)상위DA
	float  fRange[6];

} FMS_ThrldDataAI; 

// ---------------------------------
// 센서 임계기준 설정 정보
typedef struct _FCLT_CTL_ThrldAI : public FMS_CTL_BASE // RTU설비-AI 기준설정정보 설정
{
	// FMS_CTL_BASE 에서 상속 -- AI 임계 제어
	// -------------------------
	FMS_ThrldDataAI     tThrldDtAI;

} FCLT_CTL_ThrldAI;

/*  [2016/10/28 ykhong] : 
FOP 에서 임계 수정하여 RTU 에 패킷(eFT_SNSRCTL_THRLD) 전송 시 
   FCLT_CTL_ThrldDI::FMS_TransDBData::sDBFCLT_TID
   FCLT_CTL_ThrldAI::FMS_TransDBData::sDBFCLT_TID
   항목 24 자리중 
   마지막 4자리를 센서 ID 로 채워서 전송 하기로 함.
/**/



////============== DI ==================================================////

// ===================================================
// RTU DI 모니터링 결과 -------------
// RTU (데이터수집장치 - 열, 연기, 출입문, 누수, 
//                     전등 On/Off 상태, 냉난방기 On/Off 상태, 전원 On/Off 상태, 감시장비와 네트웍 연결상태,
//                     영상녹화 On/Off 상태)
// -------------------------------

// =========================================================
// 전원제어(ON/OFF) 정보 

//*** eFT_SNSRCTL_POWER 전원제어 (ON/OFF) -- RTU설비(전등, 에어컨) --------------s

// RTU설비-DO(전등, 에어컨,,)
typedef struct _FCLT_CTRL_SNSROnOff : public FMS_CTL_BASE // 설비 전원ON/OFF 제어!
{
	// SNSR_HW_CD	01	외부전원제어기	Y
	// SNSR_HW_CD	02	내부전원제어기	Y
	int		iSnsrHWProperty;  // "하드웨어 종류(특성)" *************

	// FMS_CTL_BASE 에서 상속 -- DI 전원 제어
} FCLT_CTRL_SNSROnOff;


//////////////////////////////////////////////////////////////////////////
#pragma pack() // end pack --------------------e
//////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX__PROTOCOL_RTUAI_H__B519295E_4B95_4516_B6C0_1046401655B9__INCLUDED_)
