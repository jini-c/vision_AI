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
// RTU ����������

////============== AI ==================================================////

// ---------------------------------
// RTU(�����ͼ�����ġ - �µ�, ���� ��)

// ### eFT_SNSRDAT_RES    : AI
// ### eFT_SNSRCUDAT_RES  : AI //* ���� ������ : FMS_RTU -> FMS_OP

// AI ���� or �׷� ����͸� ���

typedef struct _FCLT_CollectRslt_AI : public FMS_CollectRslt_AIDIDO
{
	// FMS_CollectRslt_AI ���� ���
	// -------------------------
	FMS_CollectRsltAI		rslt[1];		// �ִ� MAX_GRP_RSLT_AI ���� (nFcltSubCount ����)
} FCLT_CollectRslt_AI;


// =========================================================s
//*** ���(fault)�̺�Ʈ --- AI
// ### eFT_SNSREVT_FAULT,		// --- AI, FMS_RTU --> FMS_SVR, FMS_RTU --> FMS_OP

// �ü�(FACILITY -- FCLT) ��� �߻� ����(�̺�Ʈ)
// RTU -> SVR
typedef struct _FCLT_EVT_FaultAI // RTU���� ��� �߻� ����
{
	FMS_ElementFclt     tFcltElem;		// ������� ����
	FMS_TransDBData     tDbTans;		// RTU-SVR �� DB Ʈ������ Ȯ���� ���� ����

	// ��� ����� ����͸� ���
	int					nFcltSubCount;	// [2016/8/10 ykhong] : �׷����� ������ ����͸� ��� ����! (eFcltSub count)
	FMS_FaultDataAI		rslt[1];		// �ִ� MAX_GRP_RSLT_AI ���� (nFcltSubCount ����)

} FCLT_EVT_FaultAI;


// ======================================================
// RTU���� �Ӱ���� ���� 
// ### eFT_SNSRCTL_THRLD,		// -- AI : FMS_OP --> FMS_RTU, FMS_RTU -> FMS_SVR, FMS_OP

typedef struct _FMS_ThrldDataAI // RTU����-AI ���ؼ�������
{
	char	sName[SZ_MAX_NAME];			// ���� �̸�
	int		bNotify;					// �뺸 ��/��
	float	fOffsetVal;					// �����Ҷ� ������ �������� ���
	float	fMin, fMax;					// ���� �Է� ���� : ����, �ְ�

	// �溸���� �� �溸��� �Ǵܱ���
	// [0] : Low, [1] : High
	float  fRange_Danger[2];			// ���� : ���� < ����CR, ����CR < ����
	float  fRange_Warning[4];			// ��� : ����CR <= ���� <= ����MJ, ����MJ <= ���� <= ����CR
	float  fRange_Attention[4];			// ���� : ����MJ <= ���� <= ����WN, ����WN <= ���� <= ����MJ
	float  fRange_Normal[2];			// ���� : ����WN <= ���� <= ����WN
} FMS_ThrldDataAI; 

// ---------------------------------
// ���� �Ӱ���� ���� ����
typedef struct _FCLT_CTL_ThrldAI : public FMS_CTL_BASE // RTU����-AI ���ؼ������� ����
{
	// FMS_CTL_BASE ���� ��� -- AI �Ӱ� ����
	// -------------------------
	FMS_ThrldDataAI     tThrldAI;

} FCLT_CTL_ThrldAI;



////============== DI ==================================================////

// ===================================================
// RTU DI ����͸� ��� -------------
// RTU (�����ͼ�����ġ - ��, ����, ���Թ�, ����, 
//                     ���� On/Off ����, �ó���� On/Off ����, ���� On/Off ����, �������� ��Ʈ�� �������,
//                     �����ȭ On/Off ����)
// -------------------------------

// =========================================================
// ��������(ON/OFF) ���� 

//*** eFT_SNSRCTL_POWER �������� (ON/OFF) -- RTU����(����, ������) --------------s

// RTU����-DO(����, ������,,)
typedef struct _FCLT_CTRL_SNSROnOff : public FMS_CTL_BASE // ���� ����ON/OFF ����!
{
	// FMS_CTL_BASE ���� ��� -- DI ���� ����
} FCLT_CTRL_SNSROnOff;


//////////////////////////////////////////////////////////////////////////
#pragma pack() // end pack --------------------e
//////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX__PROTOCOL_RTUAI_H__B519295E_4B95_4516_B6C0_1046401655B9__INCLUDED_)
