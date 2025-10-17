// _Protocol_EQP.h: interface for the C_Protocol_EQP class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX__PROTOCOL_EQP_H__94CC2BD9_32B8_4E2B_8354_6566D337EB07__INCLUDED_)
#define AFX__PROTOCOL_EQP_H__94CC2BD9_32B8_4E2B_8354_6566D337EB07__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "_Protocol_Define.h"
#include "_Protocol_Common.h"

//////////////////////////////////////////////////////////////////////////
#pragma pack(1) // start pack --------------------s
//////////////////////////////////////////////////////////////////////////


// ####################################################################

// ---------------------------------
// ACK 신호 : FMS_RTU --> RMS_EQP  // RTU가 감시장비 모니터링 결과 수신 확인
// ### eFT_EQPDAT_ACK,		// FMS_RTU --> RMS_EQP

typedef struct _FCLT_AliveCheckAck
{
	FMS_ElementFclt     tFcltElem;	// 설비까지 구분 -- 전체 또는 일부 지정
	long				timeT;		// time_t  now; // "%Y%m%d%H%M%S"
} FCLT_AliveCheckAck;


// ###############################################################################s

//*** 전원제어 (ON/OFF) -- 전파감시장비 --------------s
// ## eFT_EQPCTL_POWER,			// RMS_EQP --> FMS_RTU : 자동 전원 On/Off 시작!!!
								// FMS_OP --> FMS_RTU, FMS_RTU --> RMS_EQP, FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
// 감시장비 전원제어 명령(on/off)
typedef struct _FCLT_CTRL_EQPOnOff : public FMS_CTL_BASE
{
	// FMS_CTL_BASE 에서 상속 -- 감시장비 전원 제어
	int					bAutoMode;		// 고정감시 운영SW 측에서 자동으로 OFF->ON 하고자 하는 경우 ★★★
} FCLT_CTRL_EQPOnOff;


// 감시장비 전원제어에 대한 진행상태 정보 ----
// ## eFT_EQPCTL_RES,				// RMS_EQP -> FMS_RTU, RMS_RTU -> FMS_SVR, FMS_OP
// 감시장비 전원제어 상태
typedef struct _FCLT_CTRL_EQPOnOffRslt : public FMS_CTL_BASE
{
	// FMS_CTL_BASE 에서 상속 -- 감시장비 전원 제어 결과

	enumFCLTOnOffStep   eMaxStep;       // 전원제어 프로세스가 거치는 MAX 단계
	enumFCLTOnOffStep   eCurStep;		// 현재단계: 1) 감시장비SBC 상주데몬 OFF, 2) 안테나제어PC OS 종료, , 

	enumFCLTCtlStatus   eCurStatus;		// 단계별 처리 상태 : start, ing, end 중에 한가지 상태
} FCLT_CTRL_EQPOnOffRslt;


//////////////////////////////////////////////////////////////////////////
#pragma pack() // end pack --------------------e
//////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX__PROTOCOL_EQP_H__94CC2BD9_32B8_4E2B_8354_6566D337EB07__INCLUDED_)
