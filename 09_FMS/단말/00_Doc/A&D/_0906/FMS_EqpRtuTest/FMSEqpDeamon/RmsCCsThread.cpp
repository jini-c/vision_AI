
#include "stdafx.h"
#include "RmsCCsThread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



IMPLEMENT_DYNCREATE(CRmsCCsThread, CRmsCCsThreadB)

CRmsCCsThread::CRmsCCsThread()
: m_pCMainFrame(NULL)
{
	Init();
}

CRmsCCsThread::CRmsCCsThread(CWnd* pWnd)
: m_pCMainFrame(pWnd)
{
	Init();
}

void CRmsCCsThread::Init()
{
	m_pServerRTU = NULL;
	m_bAutoDelete = FALSE;
	m_pMainWnd = NULL;
}

CRmsCCsThread::~CRmsCCsThread()
{
}

CAsyncSckServerEQP*	CRmsCCsThread::GetPtrCAsyncSckServerEQP()
{ 
	return m_pServerRTU;
}

BOOL CRmsCCsThread::InitInstance()
{
	ASSERT(!m_pServerRTU);
	m_pServerRTU = new CAsyncSckServerEQP(this, m_pCMainFrame);
	m_pServerRTU->m_hWnd = NULL;
	if (!m_pServerRTU->CreateEx(0, AfxRegisterWndClass(0),
		_T("FMS_EQP_TEST Notification Sink"),
		WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL))
	{
		return FALSE;
	}
	m_pMainWnd = m_pServerRTU;
	m_pMainWnd->UpdateWindow();
	m_pServerRTU->Init();
	return TRUE;
}

int CRmsCCsThread::ExitInstance()
{
	if(m_pServerRTU){
		m_pServerRTU->DestroyWindow();
		delete m_pServerRTU;
		m_pServerRTU = NULL;
	}
	return CRmsCCsThreadB::ExitInstance();
}

BEGIN_MESSAGE_MAP(CRmsCCsThread, CRmsCCsThreadB)
END_MESSAGE_MAP()


int CRmsCCsThreadMgr::CreateRmsCCsThread(CWnd* pWnd, CRmsCCsThreadB* pThread)
{
	CRmsCCsThread  *pThread2 = new CRmsCCsThread(pWnd);
	if(0 == CRmsCCsThreadMgrB::CreateRmsCCsThread(pWnd, pThread2) ) {
		delete pThread2;
		return 0;
	}
	return 1;
}






