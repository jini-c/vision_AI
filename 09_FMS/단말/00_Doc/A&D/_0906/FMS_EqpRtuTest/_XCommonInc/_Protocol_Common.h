// _Protocol_Common.h: interface for the _Protocol_Common.
//////////////////////////////////////////////////////////////////////
#if !defined(AFX__Protocol_Common_H__BFCD3552_D115_4DC0_9F24_974FF4217CF4__INCLUDED_)
#define AFX__Protocol_Common_H__BFCD3552_D115_4DC0_9F24_974FF4217CF4__INCLUDED_

// [2016/7/13 ykhong] : 2016�⵵ FMS ��ü��� ������ �ۼ� v1.0
// [2016/8/12 ykhong] : 2016�⵵ FMS ��ü��� ������ ���� v2.0
// [2016/8/19 ykhong] : 2016�⵵ FMS ��ü��� ������ ���� v3.0
// [2016/8/26 ykhong] : 2016�⵵ FMS ��ü��� ������ ���� v4.0


#if _MSC_VER > 1000
    #pragma once
#endif // _MSC_VER > 1000

#include "_Protocol_Define.h"

//////////////////////////////////////////////////////////////////////////
#pragma pack(1) // start pack --------------------s
//////////////////////////////////////////////////////////////////////////

// ###############################################################################s
typedef struct _FMS_PACKET_HEADER // FMS APP ��Ʈ�� ��Ŷ ���(8 byte)
{
    unsigned short  shProtocol;			// (2 byte) OP_FMS_PROT
	short			shOPCode;			// (2 byte) eCMD_RMS_EQPTATUS_REQ,, 
    unsigned int	dwContentsSize;     // (4 byte) ��Ŷ����� �� ������ BODY ũ��
	short			shDirection;		// (2 byte) enumFMS_SYSDIR
} FMS_PACKET_HEADER;

// ###############################################################################s

typedef struct _FMS_TransDBData
{
	long			timeT;		// time_t  now; // "%Y%m%d%H%M%S"
	char			sDBFCLT_TID[MAX_DBFCLT_TID];// ������ ���� �� _ACK ���� �� ���ڵ� ID
} FMS_TransDBData;

// ###############################################################################s

// ���� ���� ����
typedef struct _FMS_ElementFclt
{
	int					nSiteID;	// ���ݱ� ID
	int					nFcltID;	// FCLT_ID
	enumFCLT_TYPE		eFclt;		// ���񱸺�(��з� �Ǵ� �ý��� ����)
} FMS_ElementFclt;

// ###############################################################################s

typedef struct _FMS_CollectRsltDIDO // DI/DO ��Ÿ���� ����͸� ���
{
	enumSNSR_TYPE		eSnsrType;	// ���� Ÿ��(AI, DI, DO, EQP)
	enumSNSR_SUB		eSnsrSub;	// ���� ���� ����(��, ����, ����, ��, ���Թ� ��)
	int					idxSub;		// ���� ������ 3���� ��ġ�Ǿ� �ִٸ� index�� ����(0,1,,)
	enumSnsrCnnStatus	eCnnStatus;		// ����, (��Ʈ�� ���� �ȵ� or �����νĺҰ�), �������.
	int					bRslt;		// ����͸� ���
} FMS_CollectRsltDIDO;


// ### eFT_SNSRDAT_RES  -- DI, DO
// ### eFT_SNSRCUDAT_RES : DI, DO	//* ���� ������ : FMS_RTU -> FMS_OP

// ### eFT_EQPDAT_RES,			// ���� : RMS_EQP --> FMS_RTU, FMS_RTU -> FMS_SVR, FMS_OP
// ### eFT_EQPCUDAT_RES			//* ���� ������ : FMS_RTU -> FMS_OP

// ------------------------------------------------------
// ������� ���� ��� : DO ���� ��� ����п� ���� ����͸� ����� DB �����Ѵ�.
// ### eFT_SNSRCTL_RES,			// FMS_RTU -> FMS_SVR, FMS_OP
// FCLT_CollectRslt_DIDO �� ����Ͽ� DB ����

// DI/DO ��Ÿ���� ���� or �׷� ����͸� ������ ���� BODY

typedef struct _FMS_CollectRslt_AIDIDO // DI/DO ����������
{
	FMS_ElementFclt     tFcltElem;		// ������� ����
	FMS_TransDBData     tDbTans;		// RTU-SVR �� DB Ʈ������ Ȯ���� ���� ����
	// ----------------------------------------------------
	int					nFcltSubCount;	// [2016/8/10 ykhong] : �׷����� ������ ����͸� ��� ����! (eFcltSub count)
} FMS_CollectRslt_AIDIDO;

typedef struct _FCLT_CollectRslt_DIDO : public FMS_CollectRslt_AIDIDO // DI/DO ����������
{
	// FMS_CollectRslt_DIDO ���� ���
	FMS_CollectRsltDIDO	rslt[1];		// �ִ� MAX_GRP_RSLT_DI ���� (nFcltSubCount ����)

} FCLT_CollectRslt_DIDO;


// ###############################################################################s

typedef struct _FMS_CollectRsltAI
{
	enumSNSR_TYPE		eSnsrType;	// ���� Ÿ��(AI, DI, DO, EQP)
	enumSNSR_SUB		eSnsrSub;	// ���� ���� ����(��, ����, ����, ��, ���Թ� ��)
	int					idxSub;		// �µ������� 3���� ��ġ�Ǿ� �ִٸ� index�� ����(0,1,,)

	enumSnsrCnnStatus	eCnnStatus;	// ����, (��Ʈ�� ���� �ȵ� or �����νĺҰ�), �������.

	// AI ���� ���� ��� : �µ�, ����, ����-������Vr, ����-������Vs, ����-������Vt, ����-���ļ� ��
	float				fRslt;		// AI ����͸� ���
} FMS_CollectRsltAI;


// ###############################################################################s
// ���� base : ������, �Ӱ輳�� ���� ��

typedef struct _FMS_CTL_BASE
{
	FMS_ElementFclt			tFcltElem;		// ������� ����
	enumSNSR_TYPE			eSnsrType;		// ���� Ÿ��(AI, DI, DO, EQP)
	enumSNSR_SUB			eSnsrSub;		// ���� ���� ����(��, ����, ����, ��, ���Թ� ��)
	int						idxSub;			// �µ������� 3���� ��ġ�Ǿ� �ִٸ� index�� ����(0,1,,)
	
	// ---------------------
	FMS_TransDBData			tDbTans;		// RTU-SVR �� DB Ʈ������ Ȯ���� ���� ����
	char					sUserID[MAX_USER_ID]; // TFMS_CTLHIST_DTL insert �� �ʿ�!
	
	enumFCLTCtl_TYPE		eCtlType;		// ���� Ÿ��
	enumFCLTCtl_CMD			eCtlCmd;		// ���� ���� ��� : Off �Ǵ� On ���, �Ӱ�����
} FMS_CTL_BASE;


// ###############################################################################s
// �Ӱ���ؼ��� ������ (DI ��Ÿ��)

typedef struct _FMS_ThrldDataDI		// ����-DI
{
	char			sName[SZ_MAX_NAME];			// ���� �̸�
	int				bNotify;					// �뺸 ��/��
	enumFCLT_GRADE  eGrade;						// �溸 ���
	int				bNormalVal;					// ����
	int				bFaultVal;
	char			sNormalName[SZ_MAX_NAME];	// ON �̸�
	char			sFaultName[SZ_MAX_NAME];	// OFF �̸�

} FMS_ThrldDataDI;

// ################################################################

// RTU���� �Ӱ���� ���� ---- DI
// ### eFT_SNSRCTL_THRLD,		// DI : FMS_OP --> FMS_RTU, FMS_RTU -> FMS_SVR, (���� ����� ��� FMS-OP �� �������� ����.)

//*** ������� �׸� �Ӱ� ����
// ### eFT_EQPCTL_THRLD,		//      FMS_OP --> FMS_RTU, FMS_RTU -> FMS_SVR, (���� ����� ��� FMS-OP �� �������� ����.)

typedef struct _FCLT_CTL_ThrldDI : public FMS_CTL_BASE // RTU����-DI ���ؼ�������
{
	// FMS_CTL_BASE ���� ��� -- DI �Ӱ� (RTU, EQP) ���� ����
	FMS_ThrldDataDI		tThrldDtDI;		// DI ��Ÿ���� �Ӱ���� ������
} FCLT_CTL_ThrldDI;

// ###############################################################################s

typedef struct _FMS_FaultDataDI // DI ��Ÿ���� ������� ������
{
	enumSNSR_TYPE		eSnsrType;	// ���� Ÿ��(AI, DI, DO, EQP)
	enumSNSR_SUB		eSnsrSub;	// ���� ���� ����(��, ����, ����, ��, ���Թ� ��)
	int					idxSub;		// ���� ������ 3���� ��ġ�Ǿ� �ִٸ� index�� ����(0,1,,)

	// �������� : DI : Boolen, DI : ��������, ALL : ���� �ȵ�
	enumFCLT_GRADE		eGrade;			// ��� ����Ǵ� ���
	enumSnsrCnnStatus	eCnnStatus;		// ����, (��Ʈ�� ���� �ȵ� or �����νĺҰ�), �������.

	// ��� �߻� ���� ����͸� ���
	int					bRslt;		// ����͸� ���
} FMS_FaultDataDI;

// ###############################################################################s

typedef struct _FMS_FaultDataAI
{
	enumSNSR_TYPE		eSnsrType;		// ���� Ÿ��(AI, DI, DO, EQP)
	enumSNSR_SUB		eSnsrSub;		// ���� ���� ����(��, ����, ����, ��, ���Թ� ��)
	int					idxSub;			// �µ������� 3���� ��ġ�Ǿ� �ִٸ� index�� ����(0,1,,)

	// �������� : DI : Boolen, AI : ��������
	enumFCLT_GRADE		eGrade;			// ��� ����Ǵ� ���
	enumSnsrCnnStatus	eCnnStatus;		// ����, (��Ʈ�� ���� �ȵ� or �����νĺҰ�), �������.

	// ��� �߻� ���� ����͸� ��� : �µ�, ����, ����-������Vr, ����-������Vs, ����-������Vt, ����-���ļ� ��
	float				fRslt;			// AI ����͸� ���
} FMS_FaultDataAI;


// =========================================================s
//*** ���(fault)�̺�Ʈ --- DI, EQP
// ### eFT_SNEQPVT_FAULT,		// FMS_RTU --> FMS_SVR, FMS_OP
// ### eFT_EQPEVT_FAULT,		// 

typedef struct _FMS_EVT_FaultDI // ��� �߻� ����
{
	FMS_ElementFclt     tFcltElem;		// ������� ����
	FMS_TransDBData     tDbTans;		// RTU-SVR �� DB Ʈ������ Ȯ���� ���� ����
	
	// ��� ����� ����͸� ���
	int					nFcltSubCount;	// [2016/8/10 ykhong] : �׷����� ������ ����͸� ��� ����! (eFcltSub count)
} FMS_EVT_FaultDI;

typedef struct _FCLT_EVT_FaultDI : public FMS_EVT_FaultDI // ��� �߻� ����
{
	// FMS_EVT_FaultDI ���� ���
	FMS_FaultDataDI		rslt[1];		// �ִ� MAX_GRP_RSLT_DI ���� (nFcltSubCount ����)

} FCLT_EVT_FaultDI;


// =========================================================e

// ### eFT_SNSRDAT_RESACK  -- RUT : AI, DI
// ### eFT_SNSRCTL_POWERACK : RTU����(����, ������) �������� �̺�Ʈ ���� Ʈ������ Ack
// ### eFT_SNSRCTL_RESACK,		// ���� Ȯ�� : FMS_SVR --> FMS_RTU
// ### eFT_SNSRCTL_THRLDACK,	// AI,DI --  ���� Ȯ�� : FMS_SVR --> FMS_RTU

// ---------------------------------
// ��� ���� �������� Ȯ�� -- AI, DI, EQP
// ### eFT_SNSREVT_FAULTACK		// ���� Ȯ�� : FMS_SVR --> FMS_RTU
// ### eFT_EQPEVT_FAULTACK,		//

// ### eFT_EQPDAT_RESACK,		// ���� Ȯ�� : FMS_SVR -> FMS_RTU
// ### eFT_EQPCTL_POWERACK,		// ���� Ȯ�� : FMS_SVR --> FMS_RTU
// ### eFT_EQPCTL_RESACK,		// ���� Ȯ�� : RMS_SVR -> FMS_RTU
// ### eFT_EQPCTL_THRLDACK,		// ���� Ȯ�� : FMS_SVR --> FMS_RTU

// [2016/8/26 ykhong] : ��� ack �±״� FCLT_SvrTransactionAck ����ü�� ����
typedef struct _FCLT_SvrTransactionAck
{
	FMS_ElementFclt     tFcltElem;		// ������� ����
	FMS_TransDBData     tDbTans;		// RTU-SVR �� DB Ʈ������ Ȯ���� ���� ����
	// ----------------------------------------------------
	enumMethodType      eMethodType;    // DB Ʈ������ Ÿ��
	int                 iResult;		// DB Ʈ������ ���� ���ο� ���� ���� �ڵ� (���� : 1, ���� : 1���� ���� Error �ڵ�)
	
} FCLT_SvrTransactionAck;

// ###############################################################################s
// ���� ����͸� ������ ��û! -- �������� ���
// ------------##### s
// ### eFT_SNSRCUDAT_REQ,	// FMS_OP --> FMS_RTU
// ### eFT_EQPCUDAT_REQ,	// (�ǽð�) ���� ���� ��û  : FMS_OP --> FMS_RTU

// ������� ����͸� ����� ��û�Ѵ�. 
// ### eFT_EQPDAT_REQ,		// FMS_RTU --> RMS_EQP

// 
// ----- ���� ����(�ǽð�) �۽� ��û ��� ����ü (����/����, �ܼ���û)
// RTU �� ���� ����͸� ������ ��û�� �� �� �ִµ�, �̶� 10�� �������� ��� �������� ��û���� �ƴϸ�
// ���ϴ� ���� ���� �ѹ��� �������� ��û �� �� �ִ�.

typedef struct _FCLT_CTL_RequestRsltData // FMS ��������
{
	FMS_ElementFclt     tFcltElem;		// ������� ���� -- ��ü �Ǵ� �Ϻ� ����
	long				timeT;			// time_t  now; // "%Y%m%d%H%M%S"
	// ---------------------
	enumFCLTCtl_TYPE	eCtlType;		// ���� ����
	int                 eCtlCmd;		// ������(startLoop, endLoop, Query,, )
} FCLT_CTL_RequestRsltData;
// ------------##### e

// ###############################################################################s

// �ð�����ȭ : RTU - SVR - OP �� �ð� ����ȭ
// ### eFT_TIMEDAT_REQ,			// FMS_RTU --> FMS_SVR, FMS_FOP

typedef struct _FCLT_ReqTimeData
{
	FMS_ElementFclt     tFcltElem;		// ������� ���� -- ��ü �Ǵ� �Ϻ� ����
	long				timeT;			// time_t  now; // "%Y%m%d%H%M%S"
	// ---------------------
} FCLT_ReqTimeData;

// ### eFT_TIMEDAT_RES,			// FMS_SVR --> FMS_RTU, FMS_FOP --> FMS_RTU

typedef struct _FCLT_TimeRsltData
{
	FMS_ElementFclt     tFcltElem;		// ������� ���� -- ��ü �Ǵ� �Ϻ� ����
	long				timeT;			// time_t  now; // "%Y%m%d%H%M%S"
	// ---------------------
	long				tTimeRslt;		// time_t  now; // "%Y%m%d%H%M%S"

} FCLT_TimeRsltData;

// ###############################################################################s
// [2016/8/18 ykhong] : 
// -- (AI, DI) : FMS_RTU -> FMS_OP : ���� ���� �ϷḦ OP���� �˷���.
// ### eFT_SNSRCtlCompleted_THRLD,	// FMS_RTU -> FMS_OP : �Ӱ���� ���� �ϷḦ OP���� �˷���.
// ### eFT_EQPCtlCompleted_THRLD,	// FMS_RTU -> FMS_OP : �Ӱ���� ���� �ϷḦ OP���� �˷���.

typedef struct _FCLT_CtlThrldCompleted // RTU����-AI,DI,�������,�ð� �� ���ؼ������� ���� �Ϸ�
{
	FMS_ElementFclt     tFcltElem;		// ������� ����
	enumSNSR_TYPE		eSnsrType;		// ���� Ÿ��(AI, DI, DO, EQP)
	enumSNSR_SUB		eSnsrSub;		// ���� ���� ����(��, ����, ����, ��, ���Թ� ��)
	int					idxSub;			// �µ������� 3���� ��ġ�Ǿ� �ִٸ� index�� ����(0,1,,)

	// -------------------------
	int					iResult;		// RTU �� ���������� ���� �޾Ҵ��� ���ο� ���� ���� �ڵ� (���� : 1, ���� : 1���� ���� Error �ڵ�)
} FCLT_CtlThrldCompleted;


// ###############################################################################s
// [2016/8/19 ykhong] : 
// ### eFT_EVT_BASEDATUPDATE_REQ=8822,		// FMS_OP, FMS_SVR --> FMS_RTU
// ### eFT_EVT_BASEDATUPDATE_RES,			// FMS_RTU -> FMS_OP, FMS_SVR : DB���̺� ������ ���� �ߴٴ� �ǹ��� ����

typedef struct _FCLT_EVT_DBDataUpdate
{
	FMS_ElementFclt     tFcltElem;		// ������� ����
	// -------------------------
	int					iNotifyFlag;	// ������Ʈ �߻��� ���̺� ����(OP, SVR -> RTU) : enumDBEditInfo ����
										// �Ǵ� RTU ���� �ڵ�(0 : ����, 1 : ����)
} FCLT_EVT_DBDataUpdate;


//////////////////////////////////////////////////////////////////////////
#pragma pack() // end pack --------------------e
//////////////////////////////////////////////////////////////////////////

CString GetStr_FCLT_TYPE(enumFCLT_TYPE e);
CString GetStr_MethodType(enumMethodType e);
CString GetStr_FMS_CMD(enumFMS_CMD e);

CString gfn_GetDBDateTime(time_t tm_time);

#endif // !defined(AFX__Protocol_Common_H__BFCD3552_D115_4DC0_9F24_974FF4217CF4__INCLUDED_)

