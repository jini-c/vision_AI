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
typedef struct _FCLT_EVT_FaultAI // RTU설비 장애 발생 정보
{
	FMS_ElementFclt     tFcltElem;		// 설비까지 구분
	FMS_TransDBData     tDbTans;		// RTU-SVR 간 DB 트랜젝션 확인을 위한 정보

	// 장애 당시의 모니터링 결과
	int					nFcltSubCount;	// [2016/8/10 ykhong] : 그룹으로 전송할 모니터링 결과 갯수! (eFcltSub count)
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
	// [0] : Low, [1] : High
	float  fRange_Danger[2];			// 위험 : 센서 < 하위CR, 상위CR < 센서
	float  fRange_Warning[4];			// 경고 : 하위CR <= 센서 <= 하위MJ, 상위MJ <= 센서 <= 상위CR
	float  fRange_Attention[4];			// 주의 : 하위MJ <= 센서 <= 하위WN, 상위WN <= 센서 <= 상위MJ
	float  fRange_Normal[2];			// 정상 : 하위WN <= 센서 <= 상위WN
} FMS_ThrldDataAI; 

// ---------------------------------
// 센서 임계기준 설정 정보
typedef struct _FCLT_CTL_ThrldAI : public FMS_CTL_BASE // RTU설비-AI 기준설정정보 설정
{
	// FMS_CTL_BASE 에서 상속 -- AI 임계 제어
	// -------------------------
	FMS_ThrldDataAI     tThrldAI;

} FCLT_CTL_ThrldAI;



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
	// FMS_CTL_BASE 에서 상속 -- DI 전원 제어
} FCLT_CTRL_SNSROnOff;


//////////////////////////////////////////////////////////////////////////
#pragma pack() // end pack --------------------e
//////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX__PROTOCOL_RTUAI_H__B519295E_4B95_4516_B6C0_1046401655B9__INCLUDED_)
