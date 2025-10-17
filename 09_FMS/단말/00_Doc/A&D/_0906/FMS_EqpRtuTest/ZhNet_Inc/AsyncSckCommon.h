// AsyncSckCommon.h: interface for the CAsyncSckCommon class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ASYNCSCKCOMMON_H__7462E4DB_7B9B_4609_82F4_71AE249EA738__INCLUDED_)
#define AFX_ASYNCSCKCOMMON_H__7462E4DB_7B9B_4609_82F4_71AE249EA738__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#define		MAX_ASYNCSCK_BUFFER			4096

// 4052 <== 4 byte 이상 단위를 고려하지 않음!
#define		SIZE_OF_CONTENTS	(MAX_ASYNCSCK_BUFFER - sizeof(HEADER) - 32) // 4052 byte!

#define OP_FMS_PROT				0xE2D8

//////////////////////////////////////////////////////////////////////////
#pragma pack(1) // start pack --------------------s
//////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////
// packet
// =======================================================================
typedef struct _HEADER
{
	unsigned short shProtocol;		// (2 byte) OP_FMS_PROT
	short          shOPCode;		// (2 byte) eCMD_RMS_EQPTATUS_REQ,, 
    unsigned int   dwContentsSize;  // (4 byte) 패킷헤더를 뺀 나머지 BODY 크기
	short		   shDirection;		// (2 byte) enumFMS_SYSDIR
} HEADER, *LPHEADER;

#define		HEADER_SIZE			sizeof(HEADER)

//////////////////////////////////////////////////////////////////////////
// PACKET
// =======================================================================
typedef struct _PACKET
{
	HEADER header;
	BYTE btContents[SIZE_OF_CONTENTS];
	
	//////////////////////////////////
	_PACKET()
	{
		memset(this, 0, sizeof(_PACKET));
	}
	
	// nType 는 Command 와 같다.
	void FillPacket(int nCmd, short shDirection, LPBYTE lpContent, int nSize)
	{
		memset(this, 0, sizeof(PACKET));
		if(1 > nSize || SIZE_OF_CONTENTS < nSize || !lpContent)
			return;
		header.shProtocol = (short)OP_FMS_PROT;
		header.shOPCode = nCmd;
		header.dwContentsSize = nSize;
		header.shDirection = shDirection;
		if(lpContent)
			CopyMemory(btContents, lpContent, nSize);
	}
	int GetRealSize() {
		ASSERT(0 < header.dwContentsSize && SIZE_OF_CONTENTS >= header.dwContentsSize);
		return (HEADER_SIZE + header.dwContentsSize);
	}
	
} PACKET, *LPPACKET;

//////////////////////////////////////////////////////////////////////////
#pragma pack() // end pack --------------------e
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//
class CAsyncSckCommon  
{
public:
	CAsyncSckCommon();
	virtual ~CAsyncSckCommon();

};

typedef int (*FN_CALLBACK_AsyncLOG)(const LPCTSTR format);


extern inline void SckTrace(char* fmt, ...);
extern void gfnGetIPAddressFromName(char* sHostName, char* sIPAddrss);
extern void SetFunc_AsyncLOGData(FN_CALLBACK_AsyncLOG pfn);

extern CString g_strLogFileName; // "d:\\CAsyncSckClient.txt"

extern CString gfnSocketError(int nErrorCode);



#endif // !defined(AFX_ASYNCSCKCOMMON_H__7462E4DB_7B9B_4609_82F4_71AE249EA738__INCLUDED_)
