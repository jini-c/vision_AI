// _Protocol_Define.h: interface for the C_Protocol_Define class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX__Protocol_Define_H__260BA2E1_0E49_4E08_9954_360D03834161__INCLUDED_)
#define AFX__Protocol_Define_H__260BA2E1_0E49_4E08_9954_360D03834161__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


// [2016/11/8 ykhong] : �˼��� 2�� ���� �غ� ���� ��� ����
// #define _ForTest_2Cha_Solution1		1







//////////////////////////////////////////////////////////////////////////
// ��� ��Ģ
// FACILITY(����) ==> FCLT
// SENSOR(FMS ȯ�漾��) ==> SNSR

// --------------------------------------
#define OP_FMS_PROT				0xE2D8

#define SZ_MAX_DATE             8        // "20161212"
#define SZ_MAX_TIME             10       // "235852.001"

#define SZ_MAX_NAME             20       // ���� �̸�


// -----------------------------------------?????
// FCLT_TID : ���������� Unique ID ����
// FCLT_TID �÷� ���� ����


// 4 4 7 4 2 4 = 24 ����
// �⵵(4) + ��¥(4) + �ð�(��)(6) + ����ID(4) + SNSR_TYPE(2) + ������(4)

#define MAX_DBFCLT_TID			(24+1)
#define MAX_USER_ID				(10+1)

#define MAX_GRP_RSLT_DI			16		// �����ְ� 16�� ���� ���
#define MAX_GRP_RSLT_AI			6		// �����ְ� 6�� ���� ���


// ###############################################################################s
/* [2016 �⵵ FMS ��ü ��� ��Ʈ�� ��Ŷ ����]
2.1 ��� ������ ��Ŷ ����
    ��) ��Ŷ ��� : [PROTOCOL_ID (2)] + [OP CODE (2)] + [PACKET SIZE (4)] + [(2 byte) enumFMS_SYSDIR ]
    ��) ��Ŷ BODY : [��Ŷ BODY (?) ]
	��) ���� : [��Ŷ ���] + [��Ŷ BODY]

2.1.2 ��Ŷ��� ���� (10 Byte)
    ��) [PROTOCOL_ID (2)] : ��Ŷ�� �������� �������� ����� ���
        ���� ����� : 0xE2D8

    ��) [OP CODE (2)] : ��Ŷ OPERATION(COMMAND) CODE, ��Ŷ ���� �� ID
        ��) _enumFMS_CMD : ��û�ڵ� ID, �����ڵ� ID .. 

    ��) [PACKET SIZE (4)] : ��Ŷ ����� �� ������ BODY ũ��
	��) [(2 byte) enumFMS_SYSDIR ] : ����/���ſ� ���� Source/Destination �� ���� ���� ǥ��

2.1.3 ��Ŷ BODY����
    [��Ŷ BODY ������] : ��Ŷ BODY ũ��� ��û/���� �ڵ忡 ���� ������
    
/**/
// ###############################################################################s

//////////////////////////////////////////////////////////////////////////
// ��Ʈ�� APP Command ���� (APP ��Ŷ �±� ����)

// �������İ������ == �����������

// FT : Fms network Tag (FT)
// THRLD : Threshold

// REQ : Request (��û)
// RES : Response (����)
// ACK : Acknowledge (Ȯ��)

// ----------�ý��� ���� ------s
// RTU : FMS-RTU -- Remote Terminal Unit(���� �ܸ� ��ġ)
// SVR : FMS-SVR
// FOP : FMS-OP
// FRE : Fixed-RMS-EQP  (1.   ������ �������)
// FRS : Fixed-RMS-SW   (1.1. ������ ���� �SW)
// SRE : Semi-RMS-EQP   (2.   �ذ����� �������)
// SRS : Semi-RMS-SW    (2.1. �ذ����� ���� �SW)
// FDE : Fixed-DFS-EQP  (3.   ������ ��Ž���)

// ###############################################################################s

typedef enum _enumFMS_CMD // FMS APP ��Ŷ TAG
{
    eCMD_None = 2210,

	//################################################################s
	// == FMS_SVR ��Ʈ�� ���� �� Ȯ�� ���� �ʿ� == 
	eFT_NETWORK_CONFIRM_REQ = 2211,
	eFT_NETWORK_CONFIRM_RES,

	//################################################################s
	// == ȯ�氨�� == 
	
	//*** RTU���� ����͸� ������ ����(���� �ֱ�� 10��?)
	// RTU���� ����͸� ����(���) ----
	eFT_SNSRDAT_RES=2222,		// FMS_RTU -> FMS_SVR, (���� ����� ��� FMS-OP �� �������� ����.)
	eFT_SNSRDAT_RESACK,			// ���� Ȯ�� : FMS_SVR -> FMS_RTU
	
	// =========================================================s
	//*** RTU���� ����͸� ������ �ǽð� ����(5��?)
	// ----- ���� ����(�ǽð�) �۽� ���μ��� ����/����
	eFT_SNSRCUDAT_REQ=2322,		// FMS_OP --> FMS_RTU
	eFT_SNSRCUDAT_RES,			//* ���� ������ : FMS_RTU -> FMS_OP

	// =========================================================s
	//*** ���(fault)�̺�Ʈ --- 
	eFT_SNSREVT_FAULT=2422,		// FMS_RTU --> FMS_SVR, FMS_RTU --> FMS_OP
	eFT_SNSREVT_FAULTACK,		// ����Ȯ�� : FMS_SVR --> FMS_RTU

	// =========================================================s
	//*** �������� (ON/OFF) -- RTU����(����, ������)
	// �������� ��� ----
	eFT_SNSRCTL_POWER=2522,		// FMS_OP --> FMS_RTU, FMS_RTU --> FMS_SVR, (���� ����� ��� FMS-OP �� �������� ����.)
	eFT_SNSRCTL_POWERACK,		// ���� Ȯ�� : FMS_SVR --> FMS_RTU
	// ������� ���� ��� ----
	eFT_SNSRCTL_RES,			// FMS_RTU -> FMS_SVR, FMS_OP
	eFT_SNSRCTL_RESACK,			// ���� Ȯ�� : FMS_SVR --> FMS_RTU

	// =========================================================s
	//*** �Ӱ���� ���� 
	// ����1(Analog Input) : �µ�, ����
	// ����2(Digital Input) : ����, ��, ����, ���Թ�

	// RTU���� �Ӱ���� ���� ---------------------------------------s
	eFT_SNSRCTL_THRLD=2622,		// FMS_OP --> FMS_RTU, FMS_RTU -> FMS_SVR, (���� ����� ��� FMS-OP �� �������� ����.)
	eFT_SNSRCTL_THRLDACK,		// ���� Ȯ�� : FMS_SVR --> FMS_RTU
	// [2016/8/18 ykhong] : �߰�
	eFT_SNSRCtlCompleted_THRLD,	// FMS_RTU -> FMS_OP : �Ӱ���� ���� �ϷḦ OP���� �˷���.
	
	
	// *********************************************************************s
    // == ������� ���� == 

    //*** ������� ����͸� ������ ------------
	eFT_EQPDAT_REQ=3122,		// FMS_RTU --> RMS_EQP
	eFT_EQPDAT_RES,				// ���� : RMS_EQP --> FMS_RTU, FMS_RTU -> FMS_SVR, (���� ����� ��� FMS-OP �� �������� ����.)
	eFT_EQPDAT_RESACK,			//  ����Ȯ�� : FMS_SVR -> FMS_RTU

	// ��� : RTU�� ������� ����͸� ��� ���� ������ Ȯ��
	eFT_EQPDAT_ACK,				// ACK ��ȣ : FMS_RTU --> RMS_EQP

	//*** ���� ����(�ǽð�) �۽� ���μ��� -----
	// ���� ����(�ǽð�) �۽� ���μ��� ����/����
	eFT_EQPCUDAT_REQ=3322,		// FMS_OP --> FMS_RTU
	eFT_EQPCUDAT_RES,			//* ���� ������ : FMS_RTU -> FMS_OP

	//*** ���(fault)�̺�Ʈ -------------------
	eFT_EQPEVT_FAULT=3422,		// FMS_RTU --> FMS_SVR, FMS_OP
	eFT_EQPEVT_FAULTACK,		// FMS_SVR --> FMS_RTU

	//*** ������� �������� (ON/OFF)
	eFT_EQPCTL_POWER=3522,		// RMS_EQP --> FMS_RTU : �ڵ� ���� On/Off ����!!!
								// FMS_OP --> FMS_RTU, FMS_RTU --> RMS_EQP, FMS_SVR, (���� ����� ��� FMS-OP �� �������� ����.)
	eFT_EQPCTL_POWERACK,		// ���� Ȯ�� : FMS_SVR --> FMS_RTU
	// ������� ���� ������� ���� ---------
	eFT_EQPCTL_RES,				// RMS_EQP -> FMS_RTU, RMS_RTU -> FMS_SVR, FMS_OP
	eFT_EQPCTL_RESACK,			// ����Ȯ�� : RMS_SVR -> FMS_RTU

	// [2016/11/8 ykhong] : �߰�
	eFT_EQPCTL_POWEROPT_OFF=3526,	// ������� �������� �� ���� ���� �ɼ� : ������ ������, ������� ���� ��� �ð� ���� ��
	eFT_EQPCTL_POWEROPT_ON,			// ���� Ȯ�� ���� ����

	
	//*** ������� �׸� �Ӱ� ���� ----------
	eFT_EQPCTL_THRLD=3622,		// FMS_OP --> FMS_RTU, FMS_RTU -> FMS_SVR, (���� ����� ��� FMS-OP �� �������� ����.)
	eFT_EQPCTL_THRLDACK,		// ���� Ȯ�� : FMS_SVR --> FMS_RTU
	// [2016/8/18 ykhong] : �߰�
	eFT_EQPCtlCompleted_THRLD,	// FMS_RTU -> FMS_OP : �Ӱ���� ���� �ϷḦ OP���� �˷���.
    ////////////////////////////////////////////////////// ----e

	// 5000 ~  5500 : ���� ��ü ��Ʈ�� �±׷� ���
	eFTSVR_EVT_BASEDATUPDATE_REQ=5000,

	// -------------------
	// ��Ÿ - �ð�����ȭ
	// -------------------
	// 
	// �ð�����ȭ : RTU - SVR - OP �� �ð� ����ȭ
	eFT_TIMEDAT_REQ=7822,		// FMS_RTU --> FMS_SVR, FMS_FOP
	eFT_TIMEDAT_RES,			// FMS_SVR --> FMS_RTU, FMS_FOP --> FMS_RTU

	// ---------------------------------------------------------s
	// �������� ����ϴ� DB �⺻���̺�(RTU, �������, ����, �����, �����ڵ�) ���� ����� ��� RTU ���� �˷��ش�.
	eFT_EVT_BASEDATUPDATE_REQ=8822,		// FMS_OP, FMS_SVR --> FMS_RTU
	eFT_EVT_BASEDATUPDATE_RES,			// FMS_RTU -> FMS_OP, FMS_SVR : DB���̺� ������ ���� �ߴٴ� �ǹ��� ����

	eFT_EVT_BASEDATUPDATE_RESET,     // [2016/10/7 ykhong] : FMS_SVR ���� ���μ������� ��� -- �⺻ ������ �ٽ� �ε��Ͽ� �ʱ�ȭ �����Ѵ�

	// [2016/10/3 ykhong] : �߰�, FMS-OP <--> FMS-SVR, FMS-RTU <--> FMS-SVR �� ��Ʈ�� ��� Ȯ��
	eFT_NETALIVECHK_REQ=8900,			// Alive check request
	eFT_NETALIVECHK_RES,				// Alive check response
	// ---------------------------------------------------------s

	// [2016/10/19 ykhong] : �������� SBC ������ �������� �SW�� ��Ʈ������ �����Ͽ� ���� ������ �����Ѵ�.
	// FMS_EQP --> FMS_RTU, FMS_RTU --> FMS_EQP
	eFT_FCLTNETINFO_REQ=8990,			// ����(�������� �SW �� �����Ǵ� PC) IP/PORT ������ ��û
	eFT_FCLTNETINFO_RES,

	// ��Ÿ
//	eFT_SVRSYS_ERR=5822,		// FMS_SVR --> FMS_RTU : FMS-SVR ���� ����(DB ����Ʋ���, ��Ÿ)�� ���� RTU������ ���� �Ұ� ��Ȳ, ??!!??
//	eFT_SVRREOK_RES,			// FMS_RTU --> FMS_SVR : ������ֻ�Ȳ �����Ǹ�..������� ���� �̺�Ʈ �����Ͽ� ���� ����**, ??!!??
    // ------------------------

	eFT_EVT_FIRMWAREUPDATE_REQ = 9100,
	eFT_EVT_FIRMWAREUPDATE_NOTI,

	eFT_EVT_FCLTNetwork_ERR = 9900, // [2016/10/17 ykhong] : ����
	eFT_EVT_FCLTNetwork_ERRACK,

    eCMD_MaxCount
} enumFMS_CMD; // 35 �� : [2016/8/26]


// [2016/8/10 ykhong] : �߰�
// Tag System Direction : Tag ����/���ſ� ���� Source/Destination ǥ��
typedef enum _enumFMS_SYSDIR
{
	eSYSD_None = -1,
	eSYSD_RTUEQP,       // FMS_RTU --> RMS_EQP(�������)
	eSYSD_RTUFOP,       // FMS_RTU --> RMS_FOP
	eSYSD_RTUSVR,       // FMS_RTU --> RMS_FOP

	eSYSD_EQPRTU,		// FMS_EQP --> RMS_RTU 

	eSYSD_FOPRTU,		// FMS_OP --> FMS_RTU
	eSYSD_FOPSVR,       // FMS_OP --> FMS_SVR

	eSYSD_SVRRTU,		// FMS_SVR --> FMS_RTU
	eSYSD_SVRFOP,		// FMS_SVR --> FMS_FOP
	// -----------------------
	eSYSD_UPDRTU,       // UPD_SVR --> FMS_RTU 
	eSYSD_RTUUPD,       // FMS_RTU --> UPD_SVR (���ε� ����)
	eSYSD_UPDSVR,       // UPD_SVR --> FMS_SVR
	eSYSD_SVRUPD,       // UPD_SVR --> UPD_SVR // [2016/10/12 ykhong] : 
	// ---------------
	eSYSD_SVR2RSTRDB,		// [2016/10/7 ykhong] : FMS_SVR -> FMS_RST(RestoreRTU-DB.exe)
	eSYSD_RSTRDB2SVR,
	eSYSD_SVR2DBRPLC,		// [2016/10/7 ykhong] : FMS_SVR -> FMS_DBR(FMSDBReplicationMgr.exe : RTU-DB����ȭ)
	eSYSD_DBRPLC2SVR,
	eSYSD_SVR2SVR,		// ���� ��ü���� ������ ���

} enumFMS_SYSDIR;

// ----------------------------------------------
// ��� ��ֻ��� ��� ����
// NORMAL(����), WARNING(����), MAJOR(���), CRITICAL(����) ??
typedef enum _enumFCLT_GRADE // FMS ��ֵ�� ENUM
{
    eFCLTGRADE_None = -1,
	eFCLTGRADE_Normal,       // ����(Normal)
    eFCLTGRADE_Caution,		 // ����(Caution)
    eFCLTGRADE_Warning,      // ���(Warning)
	eFCLTGRADE_Danger,       // ����(Danger)
    // ------------------
    eFCLTGRADE_MaxCount,
	eFCLTGRADE_UnKnown = 99, // ��ֵ���� �����Ҽ� ���°��.
} enumFCLT_GRADE;


// FCLT_CD	001	�����ͼ�����ġ	Y
// FCLT_CD	002	����			Y
// FCLT_CD	011	��������SBC 	Y
// FCLT_CD	012	�������ÿ��	Y
// FCLT_CD	013	�ذ���			Y
// FCLT_CD	014	������Ž		Y

// ----------------------------------------------
// ���񱸺� (RMS�������, RMS�SW, ȯ�漾��,,,, )
typedef enum _enumFCLT_TYPE // FMS ���� ���� ENUM
{
    eFCLT_None = -1,
	// -----------------------------
	eFCLT_RTU=1,			// RTU ����
	eFCLT_DV,				// ����
	eFCLT_RTUFIRMWUPSVR,	// RTU �߿��� ������Ʈ ���� 

	// -----------------------------
	// 11 ~ 98 : �������
	eFCLT_EQP01=11,			// ������� 11 -- FRE(�������� ���)
	eFCLT_EQP02=12,			// ������� 12 -- FRS(�������� �SW)
	eFCLT_EQP03=13,			// ������� 13 -- SRE(�ذ��� ���)
	eFCLT_EQP04=14,			// ������� 14 -- FDE(������Ž ���)
	// -----------------------------
	eFCLT_FcltAll	=99,	// ��� ����

	// -----------------------------
	// ��Ÿ
	eFCLT_TIME		=100,	// [2016/8/18 ykhong] : �ð����� 
	eFCLT_DBSVR		=101,	// 

    // TODO:
    // -----------------------------
    eFCLT_MaxCount
} enumFCLT_TYPE;


// SNSR_TYPE	00	AI		Y
// SNSR_TYPE	01	DI		Y
// SNSR_TYPE	02	DO		Y
// SNSR_TYPE	09	None	Y
typedef enum _enumSNSR_TYPE // FMS ���� - ����Ÿ��(AI , DI, DO, ���� ���)
{
    eSNSRTYP_None = -1,
	eSNSRTYP_AI,
	eSNSRTYP_DI,
	eSNSRTYP_DO,
	eSNSRTYP_Unknown=9, // [2016/10/18 ykhong] : �߰�, SNSR_TYPE �� �����Ҽ� ������ ���
} enumSNSR_TYPE;

// ----------------------------------------------
// ���� �������� (������#1, ��ȣó������, �µ�����,,,,
// db ���̺��� SNSR_SUB_TYPE �� ���� ����
typedef enum _enumSNSR_SUB // FMS ���� ���� ���� ENUM
{
    eSNSRSUB_None = -1,

	// -----------------------------------------
	// ����Ÿ��1(DI) : ��, ����, ����, ��, ���Թ� ��
	// 100 ~ 298 DI ��Ÿ���� RTU ������ �ǹ�
	eSNSRSUB_DI_ALL=99,			// DI ���� ALL
	eSNSRSUB_DI_HEAT=100,		// �� ON/OFF ��������
	eSNSRSUB_DI_SMOKE,			// ���� ON/OFF ��������
	eSNSRSUB_DI_WATER,			// ���� ON/OFF �������� -- a water leakage
	eSNSRSUB_DI_DOOR,			// ���Թ� ON/OFF ��������
	eSNSRSUB_DI_RECVIDEO,		// �����ȭ On/Off ���� #
	eSNSRSUB_DI_PowerGROUND,	// ����(����)

	// -----------------------------------------
	// ����Ÿ��2(DO) : ������, ����, �������OnOff
	// 200 ~ 298 DI ��Ÿ���� RTU ������ �ǹ�
	eSNSRSUB_DO_AIRCON=200,		// �������(������) ON/OFF ��������, ON/OFF ���� *************
	eSNSRSUB_DO_LIGHT,			// ���� ON/OFF ��������, ON/OFF ���� ***************

	// [2016/10/24 ykhong] : �߰�
	// ������������
	eSNSRSUB_DO_PowerRTU=210,	// RTU ��ü �������� ��������
	eSNSRSUB_DO_PowerElecCtl,	// ���������

	// -----------------------------------------
	// ����Ÿ��2(AI) : �µ�, ����, �������� 
	// 300 ~ 398 AI ��Ÿ���� RTU ������ �ǹ�
	eSNSRSUB_AI_ALL=299,		// AI ���� ALL
    eSNSRSUB_AI_TEMPERATURE=300,// �µ�(����)
	eSNSRSUB_AI_HUMIDITY,		// ����

	// -----------------------------------------
	eSNSRSUB_AI_PowerVR=302,	// ������Vr
	eSNSRSUB_AI_PowerVS,		// ������Vs
	eSNSRSUB_AI_PowerVT,		// ������Vt
	eSNSRSUB_AI_PowerFreq,		// ���ļ�


	eSNSRSUB_AI_RTUFIRMWUPSVR=398,

	// --------------�������
	// 400 ~ 498 DI ��Ÿ���� ������� ������ �ǹ�
	eSNSRSUB_EQP_ALL=399,		// ������� ALL
	// ----------------------
	// (1) ��������
	eSNSRSUB_EQPFRE_Power=400,	// �������� ���� ON/OFF ��������, ON/OFF ���� ***************
    eSNSRSUB_EQPFRE_RX=401,		// (������#1, #2, ,, )
	// ...						// (eSNSRSUB_EQP_RX + 1), (eSNSRSUB_EQP_RX + 2), (eSNSRSUB_EQP_RX + 3), (eSNSRSUB_EQP_RX + N)
	// ----------------------
	// (2) �������ÿ��SW
	eSNSRSUB_EQPFRS_RX=420,		// �������ÿ����
	// ...
	// ----------------------
	// (3) �ذ�������
	eSNSRSUB_EQPSRE_Power=440,	// �ذ��� ���� ON/OFF ��������, ON/OFF ���� ***************
	eSNSRSUB_EQPSRE_RX=441,		// �������ű�
	// ...
	// ----------------------
	// (4) ������Ž
	eSNSRSUB_EQPFDE_Power=460,	// ������Ž ���� ON/OFF ��������, ON/OFF ���� ***************
	eSNSRSUB_EQPFDE_RX=461,		// �������ű�
	// ...

	// ---------��Ÿ
	eSNSRSUB_TIME=699,			// [2016/8/18 ykhong] : �ð�����
    // ------------------

	eSNSRSUB_Unknown=999		// [2016/10/18 ykhong] : �߰�, SNSR_SUB �� �����Ҽ� ������ ���

} enumSNSR_SUB;

#define SNSR_ID__UNKNOWN   9999   // [2016/10/18 ykhong] : TFMS_EVENT_DTL ���̺��� SNSR_ID �׸��� ä�� �� ������ ���
#define SNSR_VAL__UNKNOWN  999999 // [2016/10/18 ykhong] : TFMS_EVENT_DTL ���̺��� SNSR_VAL �׸��� ä�� �� ������ ���

// ----------------------------------
// �������� ����ܰ� ����
/*
// ����������� OFF Step
	1) ���� ����SW OFF
	2) ���׳�����PC OS Stable shutdown 
	3) SBC OS Stable shutdown 
	* OFF Step �� ����
/**/ 

// �������� �ܰ�
// �����������, ����, ������
typedef enum _enumFCLTOnOffStep // ���� �������� �ܰ� ENUM
{
    eEQPCTL_STEP_None = 0,
	eEQPCTL_STEP1, // (eEQPCTL_STEP1 ~ MAX Step)
} enumFCLTOnOffStep;

// #######################################################################

// RTU, �����������, ����, ������ ���� ����, �� �Ӱ� ����
// TFMS_COM_CD �� CTL_TYPE_CD �� ����
typedef enum _enumFCLTCtl_TYPE // ���� ���� ��� ���� ENUM
{
/*
CTL_TYPE_CD	11	���� ����
CTL_TYPE_CD	12	���� ����
CTL_TYPE_CD	13	���� ����
CTL_TYPE_CD	14	AI�Ӱ�ġ����
CTL_TYPE_CD	15	DI�Ӱ�ġ����
CTL_TYPE_CD	16	DO����
CTL_TYPE_CD	17	���delay����
CTL_TYPE_CD	18	�ǽð����ü���
CTL_TYPE_CD	19	����ڼ���
CTL_TYPE_CD	20	�޽�������
CTL_TYPE_CD	21	������
CTL_TYPE_CD	40	����͸� ������ ��û
CTL_TYPE_CD	50	RTU�߿��������Ʈ
CTL_TYPE_CD	60	�⺻���� ����*  // [2016/10/21 ykhong] : �߰�
/**/
	eFCLTCtlTYP_None = -1,

	// ------------------
	// CTL_TYPE_CD	18	�ǽð����ü���	Y
	eFCLTCtlTYP_CurRsltReq=18,		// ���������ʿ��� ���� ������ ��û(���� ����/����), ���� ������ ��û(�ѹ� ���� ��)

	// CTL_TYPE_CD	14	AI�Ӱ�ġ����		Y
	// CTL_TYPE_CD	15	DI�Ӱ�ġ����		Y
	eFCLTCtlTYP_ThrldAI=14,			// AI �Ӱ輳��
	eFCLTCtlTYP_ThrldDI=15,			// DI �Ӱ輳��

	// ------------------
	// CTL_TYPE_CD	16	DO����				Y
	eFCLTCtlTYP_DoCtrl=16,			// DO���� (on/off ���� ����)

	// ------------------
	// CTL_TYPE_CD	40	����͸� ������ ��ûY
	eFCLTCtlTYP_RsltReq=40,			// DO���� (on/off ���� ����)

	// ------------------
	// CTL_TYPE_CD	50	RTU�߿��������Ʈ
	eFCLTCtlTYP_RTUFIRMWUPSVR=50,

	eFCLTCtlTYP_BaseDataUpdate =60,	// �⺻���� ����*  // [2016/10/21 ykhong] : �߰�
	// ------------------
	// TODO:	

	// ------------------
	eFCLTCtlTYP_MaxCount
} enumFCLTCtl_TYPE;


// TFMS_COM_CD �� CTL_CMD_CD �� ����
typedef enum _enumFCLTCtl_CMD  // ���� ���� ��� ���� ENUM
{
// CTL_CMD_CD	00	Off			Y
// CTL_CMD_CD	01	On			Y
// CTL_CMD_CD	10	�߰�		Y
// CTL_CMD_CD	11	����		Y
// CTL_CMD_CD	12	����		Y
// CTL_CMD_CD	13	��û		Y
// CTL_CMD_CD	14	������û	Y
// CTL_CMD_CD	15	��������	Y
// CTL_CMD_CD	16	����		Y
// CTL_CMD_CD	17	����		Y
// CTL_CMD_CD	18	����		Y
// CTL_CMD_CD	19	�Ͻ�����	Y
// CTL_CMD_CD	30	�۾��غ�*  // [2016/10/21 ykhong] : 
// CTL_CMD_CD	31	�۾�����*
// CTL_CMD_CD	32	�۾��Ϸ�*

	eFCLTCtlCMD_None = -1,

	// ------------------
	eFCLTCtlCMD_Off=0,			// DO���� (off ����)
	eFCLTCtlCMD_On=1,			// DO���� (on ����)

	// CTL_CMD_CD	10	�߰�		Y
	eFCLTCtlCMD_Add=10,			// �߰�

	// CTL_CMD_CD	11	����		Y
	eFCLTCtlCMD_Edit=11,		// ����, ���� ���� ����

	// ------------------
	eFCLTCtlCMD_Query=13,		// (����) ����͸� �����͸� ��û�Ѵ�. (�ѹ� ���� ��)

	// ------------------
	eFCLTCtlCMD_Start=16,		// �ǽð� ���� ��û(���� ����)  -- RTU ���� OP�� : 10�� �������� ����͸� ����� �����Ѵ�.
	eFCLTCtlCMD_End=17,			// �ǽð� ���� ��û(���� ����)

	// ------------------
	// TODO:
	eFCLTCtlCMD_Ing=20,			// [2016/10/12 ykhong] : ó����(�߿��� ������Ʈ ��, ������� ���� OFF ��, ������� ���� ON ��..)

	eFCLTCtlCMD_WorkReady=30,		// �۾��غ�*  // [2016/10/21 ykhong] : 
	eFCLTCtlCMD_WorkReadyFail,		// �۾��غ�-Fail : RTU ������ �� �������� ��� FOP ���� �غ���� cmd ����
	eFCLTCtlCMD_WorkReadyEnd,		// �۾��غ�Ϸ�*
	eFCLTCtlCMD_WorkRun,			// �۾�����*
	eFCLTCtlCMD_WorkComplete,		// �۾��Ϸ�*

	// ------------------
	eFCLTCtlCMD_MaxCount
} enumFCLTCtl_CMD;

// #######################################################################


/*
eFCLTCtlCMD_WorkReadyFail �� ���ְ�..
eFCLTCtlCMD_WorkReady �� ����� ���� ������ Ȯ��!!!  ����....

// enumFCLTCtl_CMD �� ���� ���� ��
typedef enum _enumFCLTCtlCMDRslt
{
	FCLTCtlCMDRslt_OK=23,
	FCLTCtlCMDRslt_OverlapCmd,
	FCLTCtlCMDRslt_Fail,
};
/**/



// ���������������, ��������, ����������
typedef enum _enumFCLTCtlStatus // ���� ���� ���°� ENUM
{
    eFCLTCtlSt_None = 0,
	eFCLTCtlSt_Start,
	eFCLTCtlSt_Ing,
	eFCLTCtlSt_End,
	// ------------------
	eFCLTCtlSt_MaxCount
} enumFCLTCtlStatus;

// [2016/10/23 ykhong] : 
typedef enum _enumEQPOnOffRslt // ���� ����ܰ迡 ���� ���
{
	eEQPOnOffRslt_OK = 1,
	eEQPOnOffRslt_ERR_OverlapCmd, // �ߺ����� ������
	eEQPOnOffRslt_UNKNOWN,
	// ------------------
	// [2016/11/8 ykhong] : �߰�
	eEQPOnOffRslt_EQPDisconnected,  // ���� ��� ���� ���� �ƴ�
	eEQPOnOffRslt_EQPError,			// ���� ��� ���� ����
	eEQPOnOffRslt_ProcTimeoutOver,  // �������� ���ڵ�/�ڵ� ���μ��� Ÿ�Ӿƿ�
	// ------------------
} enumEQPOnOffRslt;


// [2016/8/26 ykhong] : 
// DB Method
typedef enum _enumDBMethodType // ���� ���� ���°� ENUM
{
    eDBMethod_None = 0,
	eDBMethod_Rslt,		// ����͸� ���� ����
	eDBMethod_Event,	// ��� �̺�Ʈ ���� ����
	eDBMethod_Ctrl,		// ��������(ON/OFF) �̷�
	eDBMethod_Thrld,	// �׸� �Ӱ� ���� ����
	eDBMethod_Time,		// �ð����� 
	// ------------------
#ifdef _ForTest_2Cha_Solution1
	eDBMethod_BaseDt_Snsr,		// �������� ����
	eDBMethod_BaseDt_Fclt,		// �������� ����
	eDBMethod_BaseDt_Site,		// ����Ʈ���� ����
#endif
	// ==================
	eDBMethod_MaxCount
} enumMethodType;


// �Ӱ� ���ؿ� ���� ��� �̿� ��Ÿ ��� �߻��� ���� �ڵ� ����
typedef enum _enumFCLT_ERR
{
	eFCLTErr_None = 0,
	// ------------------
	eFCLTErr_NoERROR		=   1,    // [2016/8/25 ykhong] : �߰�

	eFCLTErr_NotConnet		=  -1,    // ����(�νĺҰ�), �������(��Ʈ�� ������ �ȵ�) ??
	eFCLTErr_NoResponse		=  -2,    // ������ �ƴµ� ������ ���� ��� ??
	// ------------------
	// ���� 
	eFCLTErr_SvrTransErr	=  -70,   // ������ ������ �־� �������� ACK �±װ� ���� �ʴ� ���
	// ------------------
	eFCLTErr_Undefined		= -99,    // ��ֵ���� ������ �� ���°��.
} enumFCLT_ERR;

// [2016/8/30 ykhong] : ����(�µ�, ���ű�Rx1,,) ���� ���� ����
typedef enum _enumSnsrCnnStatus
{
	eSnsrCnnST_None = -1,
	// ------------------
	eSnsrCnnST_Normal, // ���󿬰�(���� or ��Ʈ�� or COM��Ʈ)
	eSnsrCnnST_Error,  // ���� �Ǿ� ���� ���� : ����(�νĺҰ�), �������(��Ʈ�� ������ �ȵ�)
	eSnsrCnnST_NoRes,  // �������(��������)
} enumSnsrCnnStatus;


// [2016/10/15 ykhong] : �߰�
/*
===================
1. ���������� : 1, 0, 9(�˼�����)
2. ���Ⱘ������ : 
3. ���Թ����� : 
.........
------------------------
RX1 : �������ű� 1 ������� ���� : 1, 0, 9(�˼�����)
RX2 : �������ű� 2 ������� ����
...
/**/
typedef enum _enumSnsrDIValue
{
	// ------------------
	SnsrDIValue_FALSE = 0,
	SnsrDIValue_TRUE = 1,
	SnsrDIValue_UnKnown=9,  // �˼�����
} enumSnsrDIValue;


// [2016/8/19 ykhong] : ����� DB ���̺� ������ ���� ����
// ���̺�(����, RTU, �������, ����, �����ڵ� ��)
typedef enum _enumDBEditInfo
{
	eDBEdit_All,
	eDBEdit_Site,
	eDBEdit_Rtu,
	eDBEdit_Eqp, // �������� ����
	eDBEdit_Snsr,
	eDBEdit_CommonCD, // �����ڵ�
	eDBEdit_DVR,
	eDBEdit_User,
	eDBEdit_UserSession,
} enumDBEditInfo;


// ########################################################################################
// [2016/9/6 �˸�] : < �ű� �������� �߰� >

/* Update ���� ���� */
typedef enum _enumFMS_FIRMWARE_STS
{          
	eFRIMWATEUPDATE_NONE = -1,
	// start
	eFRIMWATEUPDATE_READY,          // Update �غ� ��û
	eFRIMWATEUPDATE_START,          // Update ���� ����
	eFRIMWATEUPDATE_DOWNLOAD,       // Firmware File Download �� ����
	eFRIMWATEUPDATE_DOWNLOAD_ERR,   // Firmware File Download ���� ����
	eFRIMWATEUPDATE_WRITE,          // Firmware File Rom Write �� ����
	eFRIMWATEUPDATE_WRITE_ERR,      // Firmware File Rom Write ���� ����
	eFRIMWATEUPDATE_END,            // Update ���� ���� ����
}enumFMS_FIRMWARE_STS;

/*
// start
eFRIMWATEUPDATE_READY,          // Update �غ� ��û 
eFRIMWATEUPDATE_START,          // Update ���� ���� 

// ing
eFRIMWATEUPDATE_DOWNLOAD,       // Firmware File Download �� ���� 
eFRIMWATEUPDATE_WRITE,          // Firmware File Rom Write �� ���� 

// fail
eFRIMWATEUPDATE_DOWNLOAD_ERR,   // Firmware File Download ���� ���� 
eFRIMWATEUPDATE_WRITE_ERR,      // Firmware File Rom Write ���� ���� 

// end
eFRIMWATEUPDATE_END,            // Update ���� ���� ����
/**/

//////////////////////////////////////////////////////////////////////////
// ��Ʈ�� REMOTE ����
typedef enum _enumFMS_REMOTE_ID
{
	eFmsREMOTE_None = -1,
	eFmsREMOTE_RTU,
	eFmsREMOTE_FSVR,			// Fms Server
	eFmsREMOTE_FSVR_RstrRtuDB,	// FMS-SVR �� RestoreRTU-DB
	eFmsREMOTE_FSVR_RtuDBRepliMgr,	// FMS-SVR �� FMSDBReplicationMgr.exe
	eFmsREMOTE_FOP,
	eFmsREMOTE_FixedANTPC,		// Fixed Antenna Ctrol PC
	eFmsREMOTE_EQP,				// �������SBC ���� ���� ����(ȯ�氨��,����)
	eFmsREMOTE_RTUFIRMWUPSVR,
} enumFMS_REMOTE_ID;


#endif // !defined(AFX__Protocol_Define_H__260BA2E1_0E49_4E08_9954_360D03834161__INCLUDED_)
