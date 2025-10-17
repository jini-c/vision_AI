//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ASYNCSCKPUBLIC_H__E03F5D44_F2AF_4635_A0CA_8DF604495DF9__INCLUDED_)
#define AFX_ASYNCSCKPUBLIC_H__E03F5D44_F2AF_4635_A0CA_8DF604495DF9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#define  HARMONIC_DEGREECNT 4
#define  SPURIOUS_OFFSETCNT 4

typedef enum _ASYNCSCK_CALLBACK_EVENT
{
	ASYNCSCK_CONNECT = 0,
	ASYNCSCK_CLOSE,
	ASYNCSCK_WRITE,
	ASYNCSCK_ERROR,
	ASYNCSCK_READ,
	ASYNCSCK_ACCEPT
} ASYNCSCK_CALLBACK_EVENT;

typedef enum _E_CCS_PACKET_TYPE
{
	emCCS_RMS_SCKCONNECT= 2049,
	emCCS_RMS_OPRTD_REQ=2200
} E_CCS_PACKET_TYPE;

typedef struct _StCallBackSckData
{
	void*		pMgrObj;
	long		iByteLen;
	LPCTSTR		lpData;
	LPCTSTR		lpErrorDescription;
} StCallBackSckData;


typedef void (*CALLBACK_SCKEVENT_CLI)(int event, StCallBackSckData *pData);

#define  CCSMSG_RemoveSocket			(WM_USER + 9605)
#define  CCSMSG_AsyncSckServerEQP		(WM_USER + 9606)

/*del
////////////////////////////////////////////////////////////////
typedef struct _TCCS_PACKET_HEAD
{
	int nPacket_Size;
	int nPacket_Type;
	int nTgt_Eqp_Id;
	int nOpr_Site_Id;
} TCCS_PACKET_HEAD;

#define MAX_TCCS_PACKETSIZE   1024

typedef struct _TCCS_PACKET_TMPDATA
{
	TCCS_PACKET_HEAD csPckHead;
	BYTE *pData;
} TCCS_PACKET_TMPDATA;

typedef struct _TCCS_PACKET_DATA
{
	TCCS_PACKET_HEAD csPckHead;
	BYTE pData[MAX_TCCS_PACKETSIZE];
} TCCS_PACKET_DATA;
/**/
//---------------------------------------------공통-측정 -------------------------------------e//



#endif 
