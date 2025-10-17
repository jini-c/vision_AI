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

typedef struct _FCLT_RtuEqp_AliveCheckAck
{
	FMS_ElementFclt     tFcltElem;	// 설비까지 구분 -- 전체 또는 일부 지정
	long				timeT;		// time_t  now; // "%Y%m%d%H%M%S"
} FCLT_RtuEqp_AliveCheckAck;


// ###############################################################################s

//*** 전원제어 (ON/OFF) -- 전파감시장비 --------------s
// ## eFT_EQPCTL_POWER,			// RMS_EQP --> FMS_RTU : 자동 전원 On/Off 시작!!!
								// FMS_OP --> FMS_RTU, FMS_RTU --> RMS_EQP, FMS_SVR, (서버 장애인 경우 FMS-OP 로 전송하지 않음.)
// 감시장비 전원제어 명령(on/off)
typedef struct _FCLT_CTRL_EQPOnOff : public FMS_CTL_BASE
{
	// FMS_CTL_BASE 에서 상속 -- 감시장비 전원 제어
	int			bAutoMode;		// 고정감시 운영SW 측에서 자동으로 OFF->ON 하고자 하는 경우 ★★★
	int			iSnsrHWProperty;  // "하드웨어 종류" *************
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
										
	enumEQPOnOffRslt	eRsult;			// 예1) eCurStatus 가 start 중인데 eRsult 에 에러 결과를 리턴 할수 있음.
										// 예2) eCurStatus 가 end 상태인데 eRsult 에 에러를 리턴 할수 있음.. 등등..
										// 예3) eEQPOnOffRslt_ERR_OverlapCmd : OP 에서 RTU에 중복제어를 시도 하면 RTU는 에러를 리턴
} FCLT_CTRL_EQPOnOffRslt;


// 감시장비 반자동/자동 전원제어 시 세부 제어 옵션 : 센서간 딜레이, 감시장비 응답 대기 시간 지정 등
// [2016/11/8 ykhong] : 추가
// ## eFT_EQPCTL_POWEROPT_OFF
// ## eFT_EQPCTL_POWEROPT_ON

// 장비 전원 제어 프로세스 옵션 구조체
typedef struct _FCLT_CTRL_EQPOnOff_OptionOFF
{
	// eFCLTCtlCMD_Off or eFCLTCtlCMD_On
	enumFCLTCtl_CMD			eCtlCmd;		// 실제 제어 명령 : Off 또는 On 명령, 임계제어
	
	// 전원 스위치별 제어 딜레이 시간 지정
    int iDelay_PowerSwitch_ms[5]; // 0번째 iDelay 는 첫번째 스위치제어하고 기다리는 시간임.
								  // 준 고정형은 0 번만 사용
    
    // 감시장비 OFF 명령(eFT_EQPCTL_POWER) EQP 로 송신 후
    // 처리STEP1 완료 상태(eFT_EQPCTL_RES) 수신 시 까지 대기 시간 지정
    int iWaitTime_STEP1_ms; // 디풀트 15초
    
    // 감시장비 OS 종료 대기시간 지정 : 10초(디폴트)
    // (RTU는 핑 테스트를 통해 감시장비SBC 종료 확인)
    int iWaitTime_EqpOSEnd_ms; // 10초(디폴트)
	
} FCLT_CTRL_EQPOnOff_OptionOFF;

// 시간 단위 밀리초
typedef struct _FCLT_CTRL_EQPOnOff_OptionON
{
	// eFCLTCtlCMD_Off or eFCLTCtlCMD_On
	enumFCLTCtl_CMD			eCtlCmd;		// 실제 제어 명령 : Off 또는 On 명령, 임계제어
	
	// 전원 스위치별 제어 딜레이 시간 지정
    int iDelay_PowerSwitch_ms[5]; // 0번째 iDelay 는 첫번째 스위치제어하고 기다리는 시간임.
    
    //  - 감시장비 OS 부팅확인(핑테스트) 대기시간 지정 : 20초
    // : 부팅 확인 대기시간동안 필히 대기하고 다음(FMSEqpDeamon.exe 에 네트웍 접속 시도) 진행
    int iWaitTime_EqpOSStart_ms; // 20초(디폴트)

    // FMSEqpDeamon.exe 에 네트웍 접속 시도 대기 시간 지정
    // 이 시간 동안 ReTryConnect 를 시도한다, 연결되면 다음을 진행
    int iWaitTime_EQPDmnCnn_ms; // 디풀트 10초
    
    // 감시장비 ON 명령(eFT_EQPCTL_POWER) EQP 로 송신 후
    // 처리STEP1 완료 상태(eFT_EQPCTL_RES) 수신 시 까지 대기 시간 지정
    int iWaitTime_STEP1_ms; // 디풀트 15초

    // 처리STEP1 완료 상태 확인 후
    // 처리STEP2 완료 상태(eFT_EQPCTL_RES) 수신 시 까지 대기 시간 지정
    int iWaitTime_STEP2_ms; // 디풀트 15초
	
} FCLT_CTRL_EQPOnOff_OptionON;



//////////////////////////////////////////////////////////////////////////
#pragma pack() // end pack --------------------e
//////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX__PROTOCOL_EQP_H__94CC2BD9_32B8_4E2B_8354_6566D337EB07__INCLUDED_)
