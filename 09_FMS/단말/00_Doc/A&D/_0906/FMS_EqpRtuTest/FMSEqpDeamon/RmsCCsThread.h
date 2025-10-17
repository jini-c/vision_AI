#if !defined(AFX_RMSCCSTHREAD_H__23064926_A2DA_4FAC_9230_1DD8A91E3196__INCLUDED_)
#define AFX_RMSCCSTHREAD_H__23064926_A2DA_4FAC_9230_1DD8A91E3196__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#include <afxtempl.h>
#include "ZhNet_Inc\RmsCCsThreadB.h"
#include "AsyncSckServerEQP.h"

class CRmsCCsThread : public CRmsCCsThreadB 
{
	DECLARE_DYNCREATE(CRmsCCsThread)
	friend class CRmsCCsThreadMgrB;

public:
	CRmsCCsThread();
	CRmsCCsThread(CWnd* pWnd);           
	virtual ~CRmsCCsThread();
	virtual CAsyncSckServerEQP*	GetPtrCAsyncSckServerEQP();
protected:
	void Init(); 
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	DECLARE_MESSAGE_MAP()
public:
	virtual CWnd* GetCcsWnd() {return (CWnd *)m_pServerRTU; }
private:
	CAsyncSckServerEQP	*m_pServerRTU;
	CWnd				*m_pCMainFrame;
};

//////////////////////////////////////////////////////////////////////////
//
class CRmsCCsThreadMgr : public CRmsCCsThreadMgrB
{
	friend class CRmsCCsThread;
public:
	CRmsCCsThreadMgr() { };
	virtual ~CRmsCCsThreadMgr() { };

	inline CAsyncSckServerEQP* GetCCsMgr() {
		m_cs.Lock();
		CRmsCCsThread* pThd = NULL;
		if(0 < m_threadList.GetCount() )
			pThd = (CRmsCCsThread *)m_threadList.GetHead();
		m_cs.Unlock();
		
		if(pThd) {
			return pThd->GetPtrCAsyncSckServerEQP();
		}
		return NULL;
	}
	virtual int CreateRmsCCsThread(CWnd* pWnd, CRmsCCsThreadB* pThread);
};

#endif 
