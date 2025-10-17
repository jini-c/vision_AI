// FMSEqpDeamonDlg.h : header file
//

#if !defined(AFX_FMSEqpDeamonDLG_H__41C84291_2429_4FDA_824E_B4382A914274__INCLUDED_)
#define AFX_FMSEqpDeamonDLG_H__41C84291_2429_4FDA_824E_B4382A914274__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "RmsCCsThread.h"
#include "DalsuRichEdit.h"
#include "AsyncSckClientTmpFMS.h"


typedef struct _APP_EQPCTL_ST
{
    enumFCLTCtl_CMD eCtlCmd;
    enumFCLTOnOffStep eMaxStep;
    enumFCLTOnOffStep eCurStep;
    enumFCLTCtlStatus eCurStatus;
	int bAutoMode;
} APP_EQPCTL_ST;

/////////////////////////////////////////////////////////////////////////////
// CFMSEqpDeamonDlg dialog

class CFMSEqpDeamonDlg : public CDialog
{
// Construction
public:
	CFMSEqpDeamonDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CFMSEqpDeamonDlg)
	enum { IDD = IDD_FMSEqpDeamon_DIALOG };
	CIPAddressCtrl	m_ctlConnectIP;
	CIPAddressCtrl	m_ctlServerIP;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFMSEqpDeamonDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CFMSEqpDeamonDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBtnStart();
	afx_msg void OnBtnStop();
	afx_msg void OnClose();
	afx_msg void OnBtnConn_SBC();
	afx_msg void OnBtnDisconn_SBC();
	afx_msg void OnBtnEqpDatRes();
	afx_msg void OnBtnEqpDatAck();
	afx_msg void OnBtnEqpCtlResOff();
	afx_msg void OnBtnEqpCtlResOn();
	afx_msg void OnBtnEqpCtlPowerAuto();
	afx_msg void OnTimer(UINT nIDEvent);
	//}}AFX_MSG
	afx_msg void OnClient_Connect();
	afx_msg void OnClient_Close();
	afx_msg void OnClient_Send();
	afx_msg void OnClient_Receive(LPCTSTR Data, long nLen);
	afx_msg void OnClient_Error(LPCTSTR lpErrorDescription);

	afx_msg LRESULT OnAsyncSckServerEQP(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
public:
	// -------------------------------------------------------
	// FMS-OP 대기 소켓 관리
	CRmsCCsThreadMgr	 m_CRmsCCsThreadMgr;
	CAsyncSckServerEQP*  m_pSvrRTU;

	BOOL m_bEQPCTL_POWER_Timer;
	int m_iWaitCountEqpCtrl;
	APP_EQPCTL_ST   m_EqpCtlST;
	FCLT_CTRL_EQPOnOffRslt  m_EqpCtlRsltStatus;


	CDalsuRichEdit		*m_pDalRichEdit;

	// -------------------------------------------------------
	CAsyncSckClientTmpFMS	m_ClientSBC;		// 서버에 연결하는 소켓
	CString					m_strConnectIP_SBC;
	int						m_nConntPort_SBC;

	static void CallbackClientSckEvent_SBC(int event, StCallBackSckData  *pData);

	int m_nSiteID;
	int m_nFcltID;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FMSEqpDeamonDLG_H__41C84291_2429_4FDA_824E_B4382A914274__INCLUDED_)
