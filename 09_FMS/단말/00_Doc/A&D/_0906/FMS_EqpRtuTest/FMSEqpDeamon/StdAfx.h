// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__EC362B24_7F50_41DA_A154_00DC3D18B003__INCLUDED_)
#define AFX_STDAFX_H__EC362B24_7F50_41DA_A154_00DC3D18B003__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC Automation classes
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxsock.h>		// MFC socket extensions
#include "ZhNet_Inc\ZhNet_cb.h"

#include "_XCommonInc\_Protocol_RTU.h"
#include "_XCommonInc\_Protocol_EQP.h"
#include "ZhNet_Inc\AsyncSckCommon.h"

extern CString g_strLocalIP;
extern int g_nTcpPortEQP;

extern int gfnLogView(const LPCTSTR format);

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__EC362B24_7F50_41DA_A154_00DC3D18B003__INCLUDED_)
