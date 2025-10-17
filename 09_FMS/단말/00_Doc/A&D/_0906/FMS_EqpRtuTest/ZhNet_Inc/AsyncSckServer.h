#if !defined(AFX_AsyncSckServer_H__C70B3E60_D2AA_452F_BD52_F7E67582D0B1__INCLUDED_)
#define AFX_AsyncSckServer_H__C70B3E60_D2AA_452F_BD52_F7E67582D0B1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


#include <afxtempl.h>
#include "ZhNet_Inc\AsyncSckPublic.h"
#include "ZhNet_Inc\AsyncListenSckSvr.h"


typedef CList<CConntSckSvr*, CConntSckSvr*> ConnectEntryList;


/////////////////////////////////////////////////////////////////////////////

class CAsyncSckServer 
{
public:

	friend class CListenS;
	friend class CConntSckSvr;
public:

public:
	CAsyncSckServer();
	virtual ~CAsyncSckServer();

	
	virtual void FireOnError(LPCTSTR lpErrorDescription);
	virtual void FireOnClose(long lChildId);
	virtual void FireOnReceive(long lChildId, LPCTSTR lpData, long nDataLen);
	virtual void FireOnAccept(long lChildId);


	void Init_CAsyncSckServer(void *pMgrObj);
	
public:
	BSTR GetLocalAddress();
	void SetLocalAddress(LPCTSTR lpszNewValue);
	long GetLocalPort();
	void SetLocalPort(long nNewValue);
	
	long GetReceiveBufferCount();
	void SetReceiveBufferCount(long nNewValue);
	BOOL Listen();
	BOOL Send(long lChildId, LPCTSTR lpData, long nDataLen);
private:
	BOOL DoSend(CConntSckSvr* pConntSckSvr, BYTE* lpData, long nDataLen);
public:
	CConntSckSvr* GetConnectSObject(long lChildId, POSITION &pos);
	void DeleteList_ConnectEntryList();
	void DeleteList_ConnectEntryList(long lChildId);
	
	void SocketError(int nErrorCode);
	

	void GetRemoteIPPort(LONG lClientID, 
								   CString &strRemoteIP, UINT &nRemotePort); 
private:
	CListenSckSvr m_CListenSckSvr;

	BOOL m_bListen;
	CString m_LocalAddress;

	long m_LocalPort;

	void Close();
	BOOL SocketError();
public:
	ConnectEntryList m_list;
protected:
	char* m_lpBuff;
	long m_nBuffLen;
	long m_iReadedByte;
};


/////////////////////////////////////////////////////////////////////////////


#endif 
