#if !defined(AFX_RmsCCsThreadB_H__23064926_A2DA_4FAC_9230_1DD8A91E3196__INCLUDED_)
#define AFX_RmsCCsThreadB_H__23064926_A2DA_4FAC_9230_1DD8A91E3196__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#include <afxmt.h>
#include <afxtempl.h>		

class CRmsCCsThreadMgrB;
class CRmsCCsWnd;
class CRmsCCsThreadB : public CWinThread
{
	DECLARE_DYNCREATE(CRmsCCsThreadB)
	friend class CRmsCCsThreadMgrB;

public:
	CRmsCCsThreadB();           
	CRmsCCsThreadB(CWnd* pWnd);           
	virtual ~CRmsCCsThreadB();
	virtual void Delete();
	virtual CRmsCCsWnd*	GetPtrCRmsCCsWnd() { return NULL; } 
	virtual CWnd* GetCcsWnd() {return NULL; } 
protected:
	void Init(CWnd* pWnd); 
public:
	static CCriticalSection  m_cs;
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	DECLARE_MESSAGE_MAP()
private:
	static CRmsCCsThreadMgrB  *m_pCRmsCCsThreadMgrB;
};

////////////////////////////////////////////////////////////////
class CRmsCCsThreadMgrB : public CObject
{
	friend class CRmsCCsThreadB;
public:
	CRmsCCsThreadMgrB();
	virtual ~CRmsCCsThreadMgrB();
	virtual int CreateRmsCCsThread(CWnd* pWnd, CRmsCCsThreadB* pThread); 
	void StopThread();
protected:
	void DeleteList(CRmsCCsThreadB  *pCRmsCCsThreadB);
	CRmsCCsThreadB* GetCCsThreadObj(CRmsCCsThreadB  *pCRmsCCsThreadB, POSITION &pos);

	CTypedPtrList<CObList, CRmsCCsThreadB*>   m_threadList;
	CCriticalSection  m_cs;
};
/////////////////////////////////////////////////////////////////////////////

#endif 
