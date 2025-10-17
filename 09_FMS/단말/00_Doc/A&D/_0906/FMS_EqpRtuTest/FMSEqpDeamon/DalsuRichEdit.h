#if !defined(AFX_DALSURICHEDIT_H__6081508D_AA8B_11D2_9D7B_00207415044C__INCLUDED_)
#define AFX_DALSURICHEDIT_H__6081508D_AA8B_11D2_9D7B_00207415044C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

class _AFX_RICHEDITEX_STATE
{
public:
	_AFX_RICHEDITEX_STATE() ;
	 virtual ~_AFX_RICHEDITEX_STATE() ;
public:
	HINSTANCE m_hInstRichEdit20;
} ;


BOOL PASCAL AfxInitRichEditEx() ;

/////////////////////////////////////////////////////////////////////////////

class CDalsuRichEdit : public CRichEditCtrl
{
public:
	CDalsuRichEdit();
public:
	void AddText(LPCTSTR szTextIn, COLORREF &crNewColor);
	void AddText(CString &strTextIn, COLORREF &crNewColor);
	void AddName(CString &strName, COLORREF &crNewColor);
	void AddMsg(CString &strMSG, COLORREF &crNewColor, BOOL bUnderLine = FALSE, BOOL bBold = FALSE);
	BOOL Create(DWORD in_dwStyle, const RECT& in_rcRect, CWnd* in_pParentWnd, UINT in_nID);
	void DeleteExtraLines(LPCTSTR lpszStr, bool bClear=false);
	void AddLogLine(CString sLine);
public:
	virtual ~CDalsuRichEdit();

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

	DECLARE_MESSAGE_MAP()

private:
	int LINEMAX_LOG;
};

/////////////////////////////////////////////////////////////////////////////

#endif 
