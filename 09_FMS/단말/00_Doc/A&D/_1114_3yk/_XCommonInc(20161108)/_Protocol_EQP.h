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
// ACK ��ȣ : FMS_RTU --> RMS_EQP  // RTU�� ������� ����͸� ��� ���� Ȯ��
// ### eFT_EQPDAT_ACK,		// FMS_RTU --> RMS_EQP

typedef struct _FCLT_RtuEqp_AliveCheckAck
{
	FMS_ElementFclt     tFcltElem;	// ������� ���� -- ��ü �Ǵ� �Ϻ� ����
	long				timeT;		// time_t  now; // "%Y%m%d%H%M%S"
} FCLT_RtuEqp_AliveCheckAck;


// ###############################################################################s

//*** �������� (ON/OFF) -- ���İ������ --------------s
// ## eFT_EQPCTL_POWER,			// RMS_EQP --> FMS_RTU : �ڵ� ���� On/Off ����!!!
								// FMS_OP --> FMS_RTU, FMS_RTU --> RMS_EQP, FMS_SVR, (���� ����� ��� FMS-OP �� �������� ����.)
// ������� �������� ���(on/off)
typedef struct _FCLT_CTRL_EQPOnOff : public FMS_CTL_BASE
{
	// FMS_CTL_BASE ���� ��� -- ������� ���� ����
	int			bAutoMode;		// �������� �SW ������ �ڵ����� OFF->ON �ϰ��� �ϴ� ��� �ڡڡ�
	int			iSnsrHWProperty;  // "�ϵ���� ����" *************
} FCLT_CTRL_EQPOnOff;


// ������� ������� ���� ������� ���� ----
// ## eFT_EQPCTL_RES,				// RMS_EQP -> FMS_RTU, RMS_RTU -> FMS_SVR, FMS_OP
// ������� �������� ����
typedef struct _FCLT_CTRL_EQPOnOffRslt : public FMS_CTL_BASE
{
	// FMS_CTL_BASE ���� ��� -- ������� ���� ���� ���

	enumFCLTOnOffStep   eMaxStep;       // �������� ���μ����� ��ġ�� MAX �ܰ�
	enumFCLTOnOffStep   eCurStep;		// ����ܰ�: 1) �������SBC ���ֵ��� OFF, 2) ���׳�����PC OS ����, , 

	enumFCLTCtlStatus   eCurStatus;		// �ܰ躰 ó�� ���� : start, ing, end �߿� �Ѱ��� ����
										
	enumEQPOnOffRslt	eRsult;			// ��1) eCurStatus �� start ���ε� eRsult �� ���� ����� ���� �Ҽ� ����.
										// ��2) eCurStatus �� end �����ε� eRsult �� ������ ���� �Ҽ� ����.. ���..
										// ��3) eEQPOnOffRslt_ERR_OverlapCmd : OP ���� RTU�� �ߺ���� �õ� �ϸ� RTU�� ������ ����
} FCLT_CTRL_EQPOnOffRslt;


// ������� ���ڵ�/�ڵ� �������� �� ���� ���� �ɼ� : ������ ������, ������� ���� ��� �ð� ���� ��
// [2016/11/8 ykhong] : �߰�
// ## eFT_EQPCTL_POWEROPT_OFF
// ## eFT_EQPCTL_POWEROPT_ON

// ��� ���� ���� ���μ��� �ɼ� ����ü
typedef struct _FCLT_CTRL_EQPOnOff_OptionOFF
{
	// eFCLTCtlCMD_Off or eFCLTCtlCMD_On
	enumFCLTCtl_CMD			eCtlCmd;		// ���� ���� ��� : Off �Ǵ� On ���, �Ӱ�����
	
	// ���� ����ġ�� ���� ������ �ð� ����
    int iDelay_PowerSwitch_ms[5]; // 0��° iDelay �� ù��° ����ġ�����ϰ� ��ٸ��� �ð���.
								  // �� �������� 0 ���� ���
    
    // ������� OFF ���(eFT_EQPCTL_POWER) EQP �� �۽� ��
    // ó��STEP1 �Ϸ� ����(eFT_EQPCTL_RES) ���� �� ���� ��� �ð� ����
    int iWaitTime_STEP1_ms; // ��ǮƮ 15��
    
    // ������� OS ���� ���ð� ���� : 10��(����Ʈ)
    // (RTU�� �� �׽�Ʈ�� ���� �������SBC ���� Ȯ��)
    int iWaitTime_EqpOSEnd_ms; // 10��(����Ʈ)
	
} FCLT_CTRL_EQPOnOff_OptionOFF;

// �ð� ���� �и���
typedef struct _FCLT_CTRL_EQPOnOff_OptionON
{
	// eFCLTCtlCMD_Off or eFCLTCtlCMD_On
	enumFCLTCtl_CMD			eCtlCmd;		// ���� ���� ��� : Off �Ǵ� On ���, �Ӱ�����
	
	// ���� ����ġ�� ���� ������ �ð� ����
    int iDelay_PowerSwitch_ms[5]; // 0��° iDelay �� ù��° ����ġ�����ϰ� ��ٸ��� �ð���.
    
    //  - ������� OS ����Ȯ��(���׽�Ʈ) ���ð� ���� : 20��
    // : ���� Ȯ�� ���ð����� ���� ����ϰ� ����(FMSEqpDeamon.exe �� ��Ʈ�� ���� �õ�) ����
    int iWaitTime_EqpOSStart_ms; // 20��(����Ʈ)

    // FMSEqpDeamon.exe �� ��Ʈ�� ���� �õ� ��� �ð� ����
    // �� �ð� ���� ReTryConnect �� �õ��Ѵ�, ����Ǹ� ������ ����
    int iWaitTime_EQPDmnCnn_ms; // ��ǮƮ 10��
    
    // ������� ON ���(eFT_EQPCTL_POWER) EQP �� �۽� ��
    // ó��STEP1 �Ϸ� ����(eFT_EQPCTL_RES) ���� �� ���� ��� �ð� ����
    int iWaitTime_STEP1_ms; // ��ǮƮ 15��

    // ó��STEP1 �Ϸ� ���� Ȯ�� ��
    // ó��STEP2 �Ϸ� ����(eFT_EQPCTL_RES) ���� �� ���� ��� �ð� ����
    int iWaitTime_STEP2_ms; // ��ǮƮ 15��
	
} FCLT_CTRL_EQPOnOff_OptionON;



//////////////////////////////////////////////////////////////////////////
#pragma pack() // end pack --------------------e
//////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX__PROTOCOL_EQP_H__94CC2BD9_32B8_4E2B_8354_6566D337EB07__INCLUDED_)
