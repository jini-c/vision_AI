// FMSEqpDeamon.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "FMSEqpDeamon.h"
#include "FMSEqpDeamonDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
//{{AFX_MSG_MAP(CAboutDlg)
// No message handlers
//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CFMSEqpDeamonApp

BEGIN_MESSAGE_MAP(CFMSEqpDeamonApp, CWinApp)
	//{{AFX_MSG_MAP(CFMSEqpDeamonApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFMSEqpDeamonApp construction

CFMSEqpDeamonApp::CFMSEqpDeamonApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CFMSEqpDeamonApp object

CFMSEqpDeamonApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CFMSEqpDeamonApp initialization

BOOL CFMSEqpDeamonApp::InitInstance()
{
	TCHAR   szDllPath [ _MAX_PATH ] = { 0 };
	TCHAR	szLogPath [ _MAX_PATH ] = { 0 };
	::GetModuleFileName(NULL, szDllPath, _MAX_PATH);
	TCHAR	*pp	= ::_tcsrchr(szDllPath, '\\');
	//	*(++pp)	= 0;
	*(pp)	= 0;
	CString str;
	////////////////////////////////////////////////////////////////
	//[9/5/2007 ykhong] -- 로그파일초기화!
	str.Format("%s\\%s", szDllPath, "FMSEqpDeamon.log");
	g_strLogFileName = str;
	SckTrace("*************** [CFMSEqpDeamonApp] start ******************");

	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	CFMSEqpDeamonDlg dlg;
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}
