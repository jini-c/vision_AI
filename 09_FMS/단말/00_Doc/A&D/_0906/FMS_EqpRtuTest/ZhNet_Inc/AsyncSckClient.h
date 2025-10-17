#if !defined(AFX_ASYNCSCKCLIENT_H__7F4AC2E0_247B_4045_9473_186522AF5834__INCLUDED_)
#define AFX_ASYNCSCKCLIENT_H__7F4AC2E0_247B_4045_9473_186522AF5834__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AsyncSckClient.h : header file
//

#include <afxmt.h>
#include "AsyncSckPublic.h"


/////////////////////////////////////////////////////////////////////////////
// CAsyncSckClient command target

class CAsyncSckClient : public CAsyncSocket
{
	DECLARE_DYNCREATE(CAsyncSckClient)
// Attributes
public:
	CCriticalSection	m_RecvLock;

// Operations
public:
	CAsyncSckClient();
	virtual ~CAsyncSckClient();

public:
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAsyncSckClient)
	virtual void OnClose(int nErrorCode);
	virtual void OnConnect(int nErrorCode);
	virtual void OnSend(int nErrorCode);
	virtual void OnReceive(int nErrorCode);
	//}}AFX_VIRTUAL

	// Generated message map functions
	//{{AFX_MSG(CAsyncSckClient)
	//}}AFX_MSG
	
	void Init_AsyncSckClient(void *pMgrObj, CALLBACK_SCKEVENT_CLI pCALLBACK_SCKEVENT_CLI=NULL);
	void SetCallBackFuncPtr(CALLBACK_SCKEVENT_CLI pCALLBACK_SCKEVENT_CLI);

	BSTR GetLocalAddress();
	void SetLocalAddress(LPCTSTR lpszNewValue) {
			m_LocalAddress = lpszNewValue; }

	long GetLocalPort() {
			return m_LocalPort; }
	void SetLocalPort(long nNewValue) {
			m_LocalPort = nNewValue; }

	

	BOOL Connect(LPCTSTR lpLocalAddress, LPCTSTR lpRemoteAddress, long nRemotePort);
	BOOL SendData(LPCTSTR lpData, long nDataLen);
	void DoClose();
	
	BOOL IsConnected();

// Implementation
protected:
	BOOL SocketError();
	void SetSocketError(int nErrorCode);

	// ----------------------------------------------------s
	long GetReceiveBufferCount() {
		return m_nBuffLen; }
	void SetReceiveBufferCount(long nNewValue);
	ULONG GetBufferSize(ULONG ulBufSize, ULONG uGob);
	int DoReceive(void* lpBuf, int nBufLen, int nFlags= 0);
	int OnReceivePck(int nErrorCode, DWORD off, ULONG ulTotalRecv);
	BOOL		m_bPending;
	BYTE*		m_lpBuff;
	ULONG		m_iReadedByte;

	ULONG		m_nBuffLen;
	// ----------------------------------------------------e

	CString		m_RemoteAddress;
	CString		m_LocalAddress;

	long		m_RemotePort;
	long		m_LocalPort;

	BOOL		m_bInitConnect;
	BOOL		m_bConnect; 

	StCallBackSckData		m_StCallBackData;
	CALLBACK_SCKEVENT_CLI	m_pCALLBACK_SCKEVENT_CLI;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ASYNCSCKCLIENT_H__7F4AC2E0_247B_4045_9473_186522AF5834__INCLUDED_)
