#if !defined(AFX_AsyncSckServerEQP_H__D8F3C42C_6DA5_44D2_AAE1_8F5F6E9F279C__INCLUDED_)
#define AFX_AsyncSckServerEQP_H__D8F3C42C_6DA5_44D2_AAE1_8F5F6E9F279C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#include "ZhNet_Inc\AsyncSckServer.h"


#define IDT_TEST_EQPDAT_REQ		1000
#define IDT_TEST_EQPCTL_POWER	1001



class CRmsCCsThread;
class CAsyncSckServerEQP :	public CWnd, 
							public CAsyncSckServer
{
public:
	CAsyncSckServerEQP();
	CAsyncSckServerEQP(CRmsCCsThread *pCRmsCCsThread, CWnd* pWnd);
	virtual ~CAsyncSckServerEQP();
	
protected:
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg LRESULT OnRemoveSocket(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
	
private:
	int				m_nConnectedCount; // 1:1 용
	CRmsCCsThread   *m_pCRmsCCsThread;
	
	long			m_lCliSckID; // 주의 : 1:1 관계만 허용

	CWnd			*m_pImplDlg;

	int SockInit();
	BOOL CreateCCsSocket();
public:
	int SendSck(LPCTSTR lpData, long nDataLen);
	int Init();
	virtual void FireOnError(LPCTSTR lpErrorDescription);
	virtual void FireOnClose(long lChildId);
	virtual void FireOnReceive(long lChildId, LPCTSTR lpData, long nDataLen);
	virtual void FireOnAccept(long lChildId);

	void AnalysysRecvData(LPCTSTR lpData, long nDataLen);

	FCLT_CTRL_EQPOnOff	 m_eqpCtl;
};

extern UINT UM_CCSTASK;

#endif 
