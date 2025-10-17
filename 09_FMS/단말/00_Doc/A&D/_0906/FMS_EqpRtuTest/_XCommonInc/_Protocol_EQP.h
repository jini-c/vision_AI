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

typedef struct _FCLT_AliveCheckAck
{
	FMS_ElementFclt     tFcltElem;	// ������� ���� -- ��ü �Ǵ� �Ϻ� ����
	long				timeT;		// time_t  now; // "%Y%m%d%H%M%S"
} FCLT_AliveCheckAck;


// ###############################################################################s

//*** �������� (ON/OFF) -- ���İ������ --------------s
// ## eFT_EQPCTL_POWER,			// RMS_EQP --> FMS_RTU : �ڵ� ���� On/Off ����!!!
								// FMS_OP --> FMS_RTU, FMS_RTU --> RMS_EQP, FMS_SVR, (���� ����� ��� FMS-OP �� �������� ����.)
// ������� �������� ���(on/off)
typedef struct _FCLT_CTRL_EQPOnOff : public FMS_CTL_BASE
{
	// FMS_CTL_BASE ���� ��� -- ������� ���� ����
	int					bAutoMode;		// �������� �SW ������ �ڵ����� OFF->ON �ϰ��� �ϴ� ��� �ڡڡ�
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
} FCLT_CTRL_EQPOnOffRslt;


//////////////////////////////////////////////////////////////////////////
#pragma pack() // end pack --------------------e
//////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX__PROTOCOL_EQP_H__94CC2BD9_32B8_4E2B_8354_6566D337EB07__INCLUDED_)
