
#include "stdafx.h"
#include "DalsuRichEdit.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

_AFX_RICHEDITEX_STATE::_AFX_RICHEDITEX_STATE()
{
	m_hInstRichEdit20 = NULL ;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

_AFX_RICHEDITEX_STATE::~_AFX_RICHEDITEX_STATE()
{
	if( m_hInstRichEdit20 != NULL )
	{
		::FreeLibrary( m_hInstRichEdit20 ) ;
	}
}



_AFX_RICHEDITEX_STATE _afxRichEditStateEx ;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

BOOL PASCAL AfxInitRichEditEx()
{
	if( ! ::AfxInitRichEdit() )
	{
		return FALSE ;
	}
	
	_AFX_RICHEDITEX_STATE* l_pState = &_afxRichEditStateEx ;
	
	if( l_pState->m_hInstRichEdit20 == NULL )
	{
		l_pState->m_hInstRichEdit20 = LoadLibraryA( "RICHED20.DLL" ) ;
	}
	
	return l_pState->m_hInstRichEdit20 != NULL ;
}


/////////////////////////////////////////////////////////////////////////////

CDalsuRichEdit::CDalsuRichEdit()
{
	LINEMAX_LOG = 1024; 
}

CDalsuRichEdit::~CDalsuRichEdit()
{
}


BEGIN_MESSAGE_MAP(CDalsuRichEdit, CRichEditCtrl)
	ON_WM_CREATE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
int CDalsuRichEdit::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CRichEditCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;
	
#if 0
	CHARFORMAT cf;

	cf.cbSize = sizeof (CHARFORMAT);  
	cf.dwMask = CFM_FACE | CFM_SIZE; 
	cf.yHeight = 180; 
	sprintf(cf.szFaceName, "MS Sans Serif"); 
 
	SetDefaultCharFormat(cf); 
#else
	CHARFORMAT	cf;
	GetSelectionCharFormat (cf);
	cf.yHeight = (LONG)(cf.yHeight / 1.5);
	strcpy (cf.szFaceName, "±¼¸²");
	SetDefaultCharFormat(cf); 
	SetSelectionCharFormat(cf);
#endif

	SetOptions(ECOOP_OR, ECO_AUTOWORDSELECTION);

 	return 0;
}


void CDalsuRichEdit::AddText(LPCTSTR szTextIn, COLORREF &crNewColor)
{
	int iTotalTextLength = GetWindowTextLength();
	SetSel(iTotalTextLength, iTotalTextLength);
	ReplaceSel(szTextIn);
	int iStartPos = iTotalTextLength;

	CHARFORMAT cf;
	cf.cbSize		= sizeof(CHARFORMAT);
	cf.dwMask		= CFM_COLOR | CFM_UNDERLINE | CFM_BOLD;
	cf.dwEffects	=(unsigned long) ~( CFE_AUTOCOLOR | CFE_UNDERLINE | CFE_BOLD);
	cf.crTextColor	= crNewColor;

	int iEndPos = GetWindowTextLength();
	SetSel(iStartPos, iEndPos);
	SetSelectionCharFormat(cf);
	HideSelection(TRUE, FALSE);	

	LineScroll(1);
}

void CDalsuRichEdit::AddText(CString &strTextIn, COLORREF &crNewColor)
{
	int iTotalTextLength = GetWindowTextLength();
	SetSel(iTotalTextLength, iTotalTextLength);
	ReplaceSel((LPCTSTR)strTextIn);
	int iStartPos = iTotalTextLength;

	CHARFORMAT cf;
	cf.cbSize		= sizeof(CHARFORMAT);
	cf.dwMask		= CFM_COLOR | CFM_UNDERLINE | CFM_BOLD;
	cf.dwEffects	= (unsigned long)~( CFE_AUTOCOLOR | CFE_UNDERLINE | CFE_BOLD);
	cf.crTextColor	= crNewColor;

	int iEndPos = GetWindowTextLength();
	SetSel(iStartPos, iEndPos);
	SetSelectionCharFormat(cf);
	HideSelection(TRUE, FALSE);

	LineScroll(1);
}

void CDalsuRichEdit::AddName(CString &strName, COLORREF &crNewColor)
{
	int iTotalTextLength = GetWindowTextLength();
	SetSel(iTotalTextLength, iTotalTextLength);
	ReplaceSel((LPCTSTR)strName);
	int iStartPos = iTotalTextLength;

	CHARFORMAT cf;
	cf.cbSize		= sizeof (CHARFORMAT);  
	cf.dwMask		= CFM_COLOR | CFM_UNDERLINE | CFM_BOLD;
	cf.dwEffects	= (unsigned long)~(CFE_AUTOCOLOR | CFE_UNDERLINE | CFE_BOLD);
	cf.crTextColor	= crNewColor;
 
	int iEndPos = GetWindowTextLength();
	SetSel(iStartPos, iEndPos);
 	SetSelectionCharFormat(cf); 
	HideSelection(TRUE, FALSE);
}

void CDalsuRichEdit::AddMsg(CString &strMsg, COLORREF &crNewColor, 
							BOOL bUnderLine, 
							BOOL bBold)
{
	if(!::IsWindow(GetSafeHwnd()))
		return;
	
	if(GetLineCount() > LINEMAX_LOG){
		DeleteExtraLines(strMsg);
	}

	
	int iTotalLength = GetWindowTextLength();
	SetSel(iTotalLength, iTotalLength);
	ReplaceSel((LPCTSTR)strMsg);
	int iStartPos = iTotalLength;
	int iEndPos = GetWindowTextLength();

	CHARFORMAT cf;

	cf.cbSize = sizeof(CHARFORMAT);

	cf.dwMask = CFM_COLOR;

	cf.dwEffects = (unsigned long)~(CFE_UNDERLINE | CFE_BOLD | CFE_AUTOCOLOR);
	cf.crTextColor = crNewColor;
	cf.dwEffects |= bUnderLine ? CFE_UNDERLINE : cf.dwEffects ;
	cf.dwEffects |= bBold ? CFE_BOLD : cf.dwEffects;

	SetSel(iStartPos, iEndPos);
	SetSelectionCharFormat(cf);
	HideSelection(TRUE, FALSE);

	LineScroll(1);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

BOOL CDalsuRichEdit::Create(DWORD		in_dwStyle, 
							 const RECT& in_rcRect, 
							 CWnd*		in_pParentWnd, 
							 UINT		in_nID )
{
	if( ! ::AfxInitRichEditEx() )
	{
		return FALSE ;
	}
	CWnd* l_pWnd = this;
	return l_pWnd->Create( _T( "RichEdit20A" ), NULL, in_dwStyle, in_rcRect, in_pParentWnd, in_nID ) ;
}

void CDalsuRichEdit::AddLogLine(CString sLine)
{
	if(!::IsWindow(GetSafeHwnd()))	return;
	
	if(GetLineCount() > LINEMAX_LOG){
		DeleteExtraLines(sLine);
	}
    SetSel(-1, -1);				        
    ReplaceSel(sLine);					
	SendMessage(EM_SCROLLCARET);	        
}

void CDalsuRichEdit::DeleteExtraLines(LPCTSTR lpszStr, bool bClear)
{
	if (LINEMAX_LOG == 0) return;
	
	int nNewLine = 0;
	if (lpszStr != NULL)
	{
		LPCTSTR p = lpszStr;
		while (*p && *p == '\n') nNewLine++;
	}
	int nLineCount = GetLineCount() + nNewLine;
	if (nLineCount > LINEMAX_LOG 
		|| bClear)
	{
		SetRedraw(FALSE);
		int nLastChar = LineIndex(nLineCount - LINEMAX_LOG);
		SetSel(0, nLastChar);
		Clear();
		SetWindowText(_T(""));
		SetRedraw(TRUE);
		Invalidate();
	}
}