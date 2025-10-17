
#include "stdafx.h"
#include "AsyncSckServerEQP.h"
#include "RmsCCsThread.h"
// #include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

UINT UM_CCSTASK = ::RegisterWindowMessage (_T("@@CCSRmsCAsyncSckServerEQP"));

CAsyncSckServerEQP::CAsyncSckServerEQP()
: m_pCRmsCCsThread(NULL), m_pImplDlg(NULL),m_lCliSckID(-1)
{
	m_nConnectedCount = 0;
}

CAsyncSckServerEQP::CAsyncSckServerEQP(CRmsCCsThread *pCRmsCCsThread, CWnd* pWnd)
: m_lCliSckID(-1)
{
	m_pCRmsCCsThread = pCRmsCCsThread;
	m_pImplDlg = pWnd;
	m_nConnectedCount = 0;
}

CAsyncSckServerEQP::~CAsyncSckServerEQP()
{

}

BEGIN_MESSAGE_MAP(CAsyncSckServerEQP, CWnd)
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_MESSAGE(CCSMSG_RemoveSocket, OnRemoveSocket)
END_MESSAGE_MAP()

int CAsyncSckServerEQP::Init()
{
	if(0 != SockInit())
	{
		return 0;
	}
	if(!CreateCCsSocket())
		return 0;

	return 1;
}

void CAsyncSckServerEQP::OnDestroy() 
{
	::WSACleanup();
	CWnd::OnDestroy();
}

LRESULT CAsyncSckServerEQP::OnRemoveSocket(WPARAM wParam, LPARAM lParam)
{
	int iOption = (int)wParam;
	int lChildId = (int)lParam;
	if(11 == iOption) {
		DeleteList_ConnectEntryList(); 
	} else {
		DeleteList_ConnectEntryList(lChildId);
	}
	return 0;
}

void CAsyncSckServerEQP::OnTimer(UINT nIDEvent) 
{
	CWnd::OnTimer(nIDEvent);
}

int CAsyncSckServerEQP::SockInit()
{
	WORD wversionRequested;
	WSADATA wsadata;
	int err; 
	wversionRequested = MAKEWORD( 2, 2 ); 
	err = ::WSAStartup( wversionRequested, &wsadata );
	if ( err != 0 ) 
	{
		return -1;
	} 

	if(LOBYTE( wsadata.wVersion ) != 2 ||
		HIBYTE( wsadata.wVersion ) != 2 ) 
	{
		return -1; 
	} 
	return 0;
}


BOOL CAsyncSckServerEQP::CreateCCsSocket()
{
	CAsyncSckServer::Init_CAsyncSckServer(this);

	CAsyncSckServer::SetLocalAddress((LPCTSTR)g_strLocalIP);
	CAsyncSckServer::SetLocalPort(g_nTcpPortEQP);

	if(!CAsyncSckServer::Listen())
	{
		AfxMessageBox("FMS_EQP Server Failed to Listen.", MB_ICONSTOP);
		return FALSE;
	}else{

	}
	return TRUE;
}

int CAsyncSckServerEQP::SendSck(LPCTSTR lpData, long nDataLen)
{
	int nRet = -1;
	if(1 > m_nConnectedCount)
		return -1;

	CRmsCCsThread::m_cs.Lock();
	{
		nRet = CAsyncSckServer::Send(m_lCliSckID, lpData, nDataLen);
	}
	CRmsCCsThread::m_cs.Unlock();
	return nRet;
}

void CAsyncSckServerEQP::FireOnError(LPCTSTR lpErrorDescription)
{
	CAsyncSckServer::FireOnError(lpErrorDescription);
}

void CAsyncSckServerEQP::FireOnClose(long lChildId)
{
	m_lCliSckID = -1;
	CAsyncSckServer::FireOnClose(lChildId);
	m_nConnectedCount--;
}

void CAsyncSckServerEQP::FireOnReceive(long lChildId, LPCTSTR lpData, long nDataLen)
{
	if(m_lCliSckID != lChildId){
		return;
	}
	CAsyncSckServer::FireOnReceive(lChildId, lpData, nDataLen);

	AnalysysRecvData(m_lpBuff, m_iReadedByte);
}

void CAsyncSckServerEQP::FireOnAccept(long lChildId)
{
	CAsyncSckServer::FireOnAccept(lChildId);
	m_lCliSckID = lChildId;
	m_nConnectedCount++;

	POSITION pos = NULL;
	CConntSckSvr* pConntSckSvr = NULL;
	if(NULL != (pConntSckSvr = GetConnectSObject(m_lCliSckID, pos)) )
	{
		DWORD value;
		int len = sizeof(value);
		pConntSckSvr->GetSockOpt(SO_SNDBUF, &value, &len);
		if (value < 65536)
		{
			value = 65536;
			pConntSckSvr->SetSockOpt(SO_SNDBUF, &value, sizeof(value));
		}
		pConntSckSvr->GetSockOpt(SO_RCVBUF, &value, &len);
		if (value < 65536)
		{
			value = 65536;
			pConntSckSvr->SetSockOpt(SO_RCVBUF, &value, sizeof(value));
		}
	}

	CString strRemoteIP;
	UINT nRemotePort;
	GetRemoteIPPort(m_lCliSckID, strRemoteIP, nRemotePort);

	SckTrace("connected client : %s, %d\n", strRemoteIP, nRemotePort);
}

void CAsyncSckServerEQP::AnalysysRecvData(LPCTSTR lpData, long nDataLen)
{
	eFT_EQPDAT_REQ;
	eFT_EQPDAT_ACK;
	eFT_EQPCTL_POWER;

	int nCmd = 0;
	HEADER *pHead = (HEADER *)lpData;
	BYTE *contents = (BYTE *)(lpData + sizeof(HEADER) );
	nCmd = pHead->shOPCode;
	ASSERT(OP_FMS_PROT == pHead->shProtocol);

	SckTrace("HEADER : 0x%X, OPCode=%d, ContentsSize=%d, Direction=%d",
		pHead->shProtocol,		// (2 byte) OP_FMS_PROT
		pHead->shOPCode,		// (2 byte) eCMD_RMS_EQPTATUS_REQ,, 
		pHead->dwContentsSize,  // (4 byte) 패킷헤더를 뺀 나머지 BODY 크기
		pHead->shDirection);	// (2 byte) enumFMS_SYSDIR

	FMS_ElementFclt *pElementFclt = (FMS_ElementFclt *)contents; // 참고용

	if(11 <= pElementFclt->eFclt && 98 >= pElementFclt->eFclt) { // 감시장비
		if(eFT_EQPCTL_POWER == nCmd) {

			FCLT_CTRL_EQPOnOff		*pEqpCtl = (FCLT_CTRL_EQPOnOff *)contents;
			m_eqpCtl = *pEqpCtl;

			SckTrace("eFT_EQPCTL_POWER -- nSiteID=%d, nFcltID=%d, eFclt=%d, sDBFCLT_TID=%s, timeT=%s, sUserID=%s, eCtlType=%d, eCtlCmd=%d, bAutoMode=%d", 
				m_eqpCtl.tFcltElem.nSiteID,
				m_eqpCtl.tFcltElem.nFcltID,
				m_eqpCtl.tFcltElem.eFclt,
				m_eqpCtl.tDbTans.sDBFCLT_TID,
				gfn_GetDBDateTime(m_eqpCtl.tDbTans.timeT),
				m_eqpCtl.sUserID,
				m_eqpCtl.eCtlType,
				m_eqpCtl.eCtlCmd,
				m_eqpCtl.bAutoMode);

			if(m_pImplDlg)
				m_pImplDlg->PostMessage(CCSMSG_AsyncSckServerEQP, 11, 0);

		} else if(eFT_EQPDAT_REQ == nCmd) {
			// 감시장비 모니터링 결과 요청을 받음
			FCLT_CTL_RequestRsltData  reqRsltDI;
			FCLT_CTL_RequestRsltData  *pReqRsltDI = (FCLT_CTL_RequestRsltData *)contents;
			reqRsltDI = *pReqRsltDI;

			SckTrace("eFT_EQPDAT_REQ -- nSiteID=%d, nFcltID=%d, eFclt=%d, timeT=%s, eCtlType=%d, eCtlCmd=%d", 
				reqRsltDI.tFcltElem.nSiteID,
				reqRsltDI.tFcltElem.nFcltID,
				reqRsltDI.tFcltElem.eFclt,
				gfn_GetDBDateTime(reqRsltDI.timeT),
				reqRsltDI.eCtlType,
				reqRsltDI.eCtlCmd);

			if(m_pImplDlg)
				m_pImplDlg->SetTimer(IDT_TEST_EQPDAT_REQ, 1000, NULL);

		} else if(eFT_EQPDAT_ACK == nCmd) {

			FCLT_AliveCheckAck	aliveChkAck;
			FCLT_AliveCheckAck	*pAliveChkAck = (FCLT_AliveCheckAck *)contents;
			aliveChkAck = *pAliveChkAck;
			
			SckTrace("eFT_EQPDAT_ACK -- nSiteID=%d, nFcltID=%d, eFclt=%d, timeT=%s", 
				aliveChkAck.tFcltElem.nSiteID,
				aliveChkAck.tFcltElem.nFcltID,
				aliveChkAck.tFcltElem.eFclt,
				gfn_GetDBDateTime(aliveChkAck.timeT) );
			
		// ---------------------------------------------------------------
		} else {
			ASSERT(0);
		}
	} else {
		ASSERT(0);
	}
}