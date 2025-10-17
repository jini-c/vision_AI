#if !defined(AFX_LISTENS_H__FAAA2F41_D8FE_11D3_B4AE_00C04F2B300E__INCLUDED_)
#define AFX_LISTENS_H__FAAA2F41_D8FE_11D3_B4AE_00C04F2B300E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#include <afxmt.h>

/////////////////////////////////////////////////////////////////////////////

class CAsyncSckServer;
class CConntSckSvr : public CAsyncSocket
{
private:
	CCriticalSection	m_RecvLock;

public:
	CConntSckSvr();
	CConntSckSvr(CAsyncSckServer* pCAsyncSckServer);
	void Constructor();

	virtual ~CConntSckSvr();

public:
	void virtual OnClose(int nErrorCode);
	void virtual OnReceive(int nErrorCode);

	void GetRemoteIPPort(CString &strRemoteIP, UINT &nRemotePort);

	CString SetSocketError(int nErrorCode);
public:
	long		 m_lChildId;
	static long  m_lObjectCount;

	CAsyncSckServer* m_pCAsyncSckServer;

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
};


/////////////////////////////////////////////////////////////////////////////

class CListenSckSvr : public CAsyncSocket
{
public:

public:
	CListenSckSvr();
	virtual ~CListenSckSvr();

	void SetCAsyncSckServer(CAsyncSckServer	*pCAsyncSckServer);
public:
	public:
	virtual void OnAccept(int nErrorCode);


protected:
	CConntSckSvr		*m_pCConntSckSvr;
	CAsyncSckServer	*m_pCAsyncSckServer;
};

/////////////////////////////////////////////////////////////////////////////


#endif 
