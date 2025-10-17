// FMSEqpDeamonDlg.cpp : implementation file
//

#include "stdafx.h"
#include "FMSEqpDeamon.h"
#include "FMSEqpDeamonDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CFMSEqpDeamonDlg*  g_pCFMSEqpDeamonDlg = NULL;
BOOL g_bAppEndFlag = FALSE;

LPSTR IPCtrlToSTR(CIPAddressCtrl* ctrl)
{
	//Converts the control address to textual address
	//Convert bytes to string
	BYTE bOctet1;
	BYTE bOctet2;
	BYTE bOctet3;
	BYTE bOctet4;
	
	//Get the value and blank values
	int iBlank;
	iBlank = ctrl->GetAddress(bOctet1,bOctet2,bOctet3,bOctet4);
	
	if (iBlank!=4)
		//Not filled
		return NULL;
	else
	{
		in_addr iAddr;
		iAddr.S_un.S_un_b.s_b1=bOctet1;
		iAddr.S_un.S_un_b.s_b2=bOctet2;
		iAddr.S_un.S_un_b.s_b3=bOctet3;
		iAddr.S_un.S_un_b.s_b4=bOctet4;
		
		return inet_ntoa(iAddr);
	}
}


// SetFunc_AsyncLOGData
// static 
int gfnLogView(const LPCTSTR format)
{
	CString strMsg;
	strMsg.Format("%s\n", format);
	
	if(g_bAppEndFlag)
		return 0;
	
	COLORREF crColor = RGB(0, 0, 0);
	if(g_pCFMSEqpDeamonDlg) {
		if(g_pCFMSEqpDeamonDlg->m_pDalRichEdit)
			g_pCFMSEqpDeamonDlg->m_pDalRichEdit->AddMsg(strMsg, crColor, TRUE, TRUE);
	}
	
	return 1;
}

/////////////////////////////////////////////////////////////////////////////
// CFMSEqpDeamonDlg dialog

CFMSEqpDeamonDlg::CFMSEqpDeamonDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CFMSEqpDeamonDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CFMSEqpDeamonDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pDalRichEdit = NULL;
	g_pCFMSEqpDeamonDlg = this;

	g_nTcpPortEQP = 57002;

	m_strConnectIP_SBC = "";
	m_nConntPort_SBC = 5150;

	// �α� ���� ����
	SetFunc_AsyncLOGData(gfnLogView);

	m_nSiteID = 4002;
	m_nFcltID = atoi("0065");

	m_pSvrRTU = NULL;
	m_iWaitCountEqpCtrl = -1;
	m_bEQPCTL_POWER_Timer = FALSE;
}

void CFMSEqpDeamonDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFMSEqpDeamonDlg)
	DDX_Control(pDX, IDC_IPADDRESS_SBC, m_ctlConnectIP);
	DDX_Control(pDX, IDC_IPADDRESS_EQP, m_ctlServerIP);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CFMSEqpDeamonDlg, CDialog)
	//{{AFX_MSG_MAP(CFMSEqpDeamonDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_START, OnBtnStart)
	ON_BN_CLICKED(IDC_BTN_STOP, OnBtnStop)
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_BTN_CONN_SBC, OnBtnConn_SBC)
	ON_BN_CLICKED(IDC_BTN_DISCONN_SBC, OnBtnDisconn_SBC)
	ON_BN_CLICKED(IDC_BTN_EQPDAT_RES, OnBtnEqpDatRes)
	ON_BN_CLICKED(IDC_BTN_EQPDAT_ACK, OnBtnEqpDatAck)
	ON_BN_CLICKED(IDC_BTN_EQPCTL_RES_OFF, OnBtnEqpCtlResOff)
	ON_BN_CLICKED(IDC_BTN_EQPCTL_RES_ON, OnBtnEqpCtlResOn)
	ON_BN_CLICKED(IDC_BTN_EQPCTL_POWER_AUTO, OnBtnEqpCtlPowerAuto)
	ON_WM_TIMER()
	//}}AFX_MSG_MAP
	ON_MESSAGE(CCSMSG_AsyncSckServerEQP, OnAsyncSckServerEQP)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFMSEqpDeamonDlg message handlers

BOOL CFMSEqpDeamonDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	SckTrace("*************** [CFMSEqpDeamonDlg::OnInitDialog] ******************");

	GetDlgItem(IDC_BTN_START)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_STOP)->EnableWindow(FALSE);

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	char wkLocalName[64];
	char cLocalIPAddress[64];

	memset(wkLocalName,		'\0', sizeof(wkLocalName));
	memset(cLocalIPAddress,	'\0', sizeof(cLocalIPAddress));
	gfnGetIPAddressFromName((char*)wkLocalName, (char*)cLocalIPAddress);

	g_strLocalIP = cLocalIPAddress;

	SetDlgItemText(IDC_IPADDRESS_EQP, g_strLocalIP);
	// ���� port ��ȣ
	SetDlgItemInt(IDC_EDT_SERVERPORT, g_nTcpPortEQP);

	// client !!!! ----------------------------SBC �� ����
	m_strConnectIP_SBC = g_strLocalIP;
	SetDlgItemText(IDC_IPADDRESS_SBC, m_strConnectIP_SBC);
	// ������ port ��ȣ
	SetDlgItemInt(IDC_EDT_CONNTPORT, m_nConntPort_SBC);


	// -----------------------S
	CRect rc, rcClient;
	CWnd* pWnd = GetDlgItem(IDC_STATIC_RICHEDIT);
	pWnd->GetWindowRect(&rc);
	pWnd->DestroyWindow();
	ScreenToClient(&rc);
	
	m_pDalRichEdit = new CDalsuRichEdit;
	m_pDalRichEdit->Create(WS_VISIBLE | WS_CHILD |WS_BORDER | WS_HSCROLL | 
		WS_CLIPCHILDREN | WS_VSCROLL | ES_MULTILINE | ES_AUTOHSCROLL | ES_READONLY |
		ES_AUTOVSCROLL |ES_LEFT | ES_WANTRETURN, rc, this, IDC_STATIC_RICHEDIT);
	// -----------------------E

	SckTrace("*************** [CFMSEqpDeamonDlg::OnInitDialog] ");
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CFMSEqpDeamonDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CFMSEqpDeamonDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CFMSEqpDeamonDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CFMSEqpDeamonDlg::OnBtnStart() 
{
	GetDlgItem(IDC_BTN_START)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_STOP)->EnableWindow(FALSE);

	g_strLocalIP = IPCtrlToSTR(&m_ctlServerIP);
	g_nTcpPortEQP = (int)GetDlgItemInt(IDC_EDT_SERVERPORT);

	// TODO: Add your control notification handler code here
	if(0 != m_CRmsCCsThreadMgr.CreateRmsCCsThread(this, NULL) ) {
		GetDlgItem(IDC_BTN_START)->EnableWindow(FALSE);
		GetDlgItem(IDC_BTN_STOP)->EnableWindow(TRUE);
		SckTrace("���� ���� �Ϸ�!");
		m_pSvrRTU = m_CRmsCCsThreadMgr.GetCCsMgr();
		
	} else {
		GetDlgItem(IDC_BTN_START)->EnableWindow(TRUE);
		GetDlgItem(IDC_BTN_STOP)->EnableWindow(FALSE);
		SckTrace("���� ���� ����!");
	}
}

void CFMSEqpDeamonDlg::OnBtnStop() 
{
	GetDlgItem(IDC_BTN_START)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_STOP)->EnableWindow(FALSE);

	// TODO: Add your control notification handler code here
	CAsyncSckServerEQP*  pSvrRTU = m_CRmsCCsThreadMgr.GetCCsMgr();
	if(pSvrRTU)
		pSvrRTU->SendMessage(CCSMSG_RemoveSocket, 11, NULL); 
	m_CRmsCCsThreadMgr.StopThread();
	m_pSvrRTU = NULL;

	GetDlgItem(IDC_BTN_START)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_STOP)->EnableWindow(FALSE);

	SckTrace("���� ����!");
}

void CFMSEqpDeamonDlg::OnClose() 
{
	// TODO: Add your message handler code here and/or call default
	g_bAppEndFlag = TRUE;

	m_ClientSBC.Close();

	OnBtnStop();

	if(m_pDalRichEdit) {
		m_pDalRichEdit->DestroyWindow();
		delete m_pDalRichEdit;
		m_pDalRichEdit = NULL;
	}
	
	CDialog::OnClose();
}

void CFMSEqpDeamonDlg::OnBtnConn_SBC() 
{
	GetDlgItem(IDC_BTN_CONN_SBC)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_DISCONN_SBC)->EnableWindow(FALSE);

	// TODO: Add your control notification handler code here
	m_ClientSBC.Init_AsyncSckClient(this, CallbackClientSckEvent_SBC);

	// client !!!!
	m_strConnectIP_SBC = IPCtrlToSTR(&m_ctlConnectIP);
	m_nConntPort_SBC = (int)GetDlgItemInt(IDC_EDT_CONNTPORT);

	if(!m_ClientSBC.Connect(g_strLocalIP, m_strConnectIP_SBC, m_nConntPort_SBC) ) {
		GetDlgItem(IDC_BTN_CONN_SBC)->EnableWindow(TRUE);
		GetDlgItem(IDC_BTN_DISCONN_SBC)->EnableWindow(FALSE);
	}
}

void CFMSEqpDeamonDlg::OnBtnDisconn_SBC() 
{
	// TODO: Add your control notification handler code here
	m_ClientSBC.Close();
	GetDlgItem(IDC_BTN_CONN_SBC)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_DISCONN_SBC)->EnableWindow(FALSE);
}

void CFMSEqpDeamonDlg::CallbackClientSckEvent_SBC(int event, StCallBackSckData  *pData)
{
	CFMSEqpDeamonDlg* pDlg = (CFMSEqpDeamonDlg*)pData->pMgrObj;
	switch((ASYNCSCK_CALLBACK_EVENT)event)
	{
	case ASYNCSCK_CONNECT:
		pDlg->OnClient_Connect();
		break;
	case ASYNCSCK_CLOSE:
		pDlg->OnClient_Close();
		break;
	case ASYNCSCK_WRITE:
		pDlg->OnClient_Send();
		break;
	case ASYNCSCK_ERROR:
		pDlg->OnClient_Error(pData->lpErrorDescription);
		break;
	case ASYNCSCK_READ:
		pDlg->OnClient_Receive(pData->lpData, pData->iByteLen);
		break;
	}
}

void CFMSEqpDeamonDlg::OnClient_Connect() 
{
	
}

void CFMSEqpDeamonDlg::OnClient_Close() 
{
	
}

void CFMSEqpDeamonDlg::OnClient_Send() 
{
	
}

void CFMSEqpDeamonDlg::OnClient_Error(LPCTSTR lpErrorDescription) 
{
	TRACE("SBC Connect --- socket error : %s \n", lpErrorDescription);

	GetDlgItem(IDC_BTN_CONN_SBC)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_DISCONN_SBC)->EnableWindow(FALSE);
}

void CFMSEqpDeamonDlg::OnClient_Receive(LPCTSTR Data, long nLen) 
{

}

// 4002	��������	��⵵ ������ ������ ���ﵿ	40	2101	1	2016-08-16	DEV	2016-08-16	DEV	Y

// FCLT_ID, SITE_ID,FCLT_NM,		FCLT_CD,	IP,				PORT,SEND_CYCLE,UTCK_CYCLE,NOTICE,SRL_NO,VER,GATEWAY,SUBNETMASK,HIGH_FCLT_ID,IMG_ID,POINT_ID,CREATE_DATE,CREATE_USR,UPDATE_DATE,UPDATE_USR,SYN_CD
// 0063		4002	�����ͼ�����ġ	001			158.181.17.201	COM1	30	30	30	1	20160829			0		1	2016-09-05 ���� 9:57:05	DEV	2016-09-05 ���� 9:57:05	DEV	Y
// 0064		4002	����			002			158.181.17.201	COM1	30	30	30	1	20160829			0	10	1	2016-09-05 ���� 9:57:05	DEV	2016-09-05 ���� 9:57:05	DEV	Y
// 0065		4002	��������SBC		011			158.181.17.201	COM1	30	30	30	1	20160829			0	02	1	2016-09-05 ���� 9:57:06	DEV	2016-09-05 ���� 9:57:06	DEV	Y
// 0066		4002	�������ÿ��SW	012			158.181.17.201	COM1	30	30	30	1	20160829			0	02	1	2016-09-05 ���� 9:57:06	DEV	2016-09-05 ���� 9:57:06	DEV	Y

// EQP -> RTU
// ����Ǿ� ������ �뺸(Alive Check)
void CFMSEqpDeamonDlg::OnBtnEqpDatAck() 
{
	// TODO: Add your control notification handler code here
	eFT_EQPDAT_ACK;

	FCLT_AliveCheckAck  aliveChkAck;

	// (1) : FMS_ElementFclt  tFcltElem;
	aliveChkAck.tFcltElem.nSiteID = m_nSiteID;		// ���ݱ� ID
	aliveChkAck.tFcltElem.nFcltID = m_nFcltID;		// FCLT_ID
	aliveChkAck.tFcltElem.eFclt = eFCLT_EQP01;		// ���񱸺�(��з� �Ǵ� �ý��� ����)

	// ------------------
	// (2) : FMS_ElementFclt     tFcltElem;
	aliveChkAck.timeT = (long)CTime::GetCurrentTime().GetTime();		// time_t  now; // "%Y%m%d%H%M%S"
	
	PACKET  pkt;
	pkt.FillPacket(eFT_EQPDAT_ACK, (short)eSYSD_EQPRTU, (LPBYTE)&aliveChkAck, sizeof(aliveChkAck) );

	if(m_pSvrRTU )
		m_pSvrRTU->SendSck((LPCTSTR)&pkt, pkt.GetRealSize() );
}

// EQP -> RTU
// ���İ������ ����͸� ������ ����
void CFMSEqpDeamonDlg::OnBtnEqpDatRes() 
{
	// TODO: Add your control notification handler code here
	eFT_EQPDAT_RES;

	// ������� ���� ����͸� ����
	
	FCLT_CollectRslt_DIDO  snsrDI;
	
	// (1)
	// FMS_ElementFclt  tFcltElem;
	snsrDI.tFcltElem.nSiteID = m_nSiteID;		// ���ݱ� ID
	snsrDI.tFcltElem.nFcltID = m_nFcltID;		// FCLT_ID
	snsrDI.tFcltElem.eFclt = eFCLT_EQP01;		// ���񱸺�(��з� �Ǵ� �ý��� ����)
	// ------------------
	// (2)
	// FMS_TransDBData  tDbTans;
	snsrDI.tDbTans.timeT = (long)CTime::GetCurrentTime().GetTime();		// time_t  now; // "%Y%m%d%H%M%S"
	
	// ---------------------------------------s
	// ������ ���� �� _ACK ���� �� ���ڵ� ID
	// SNSR_TYPE	00	AI	Y
	// SNSR_TYPE	01	DI	Y
	// SNSR_TYPE	02	DO	Y
	// SNSR_TYPE	03	�������	Y
	int nSnsrType = 3; // 
//	CString str = GetDBFclt_TID("TFMS_FDEHIST_DTL", m_nSiteID, nSnsrType);
	CString str = "";
	wsprintf(snsrDI.tDbTans.sDBFCLT_TID, str);
	// ---------------------------------------e
	
	
	FMS_CollectRsltDIDO rsltTmp[20];
	// -------------------------
	// (3)
	snsrDI.nFcltSubCount = 1;	// [2016/8/10 ykhong] : �׷����� ������ ����͸� ��� ����! (eFcltSub count)
	
	// ------------------
	// FMS_CollectRsltDIDO	rslt[1];
	rsltTmp[0].eSnsrType = eSNSRTYP_EQP;		// ���� Ÿ��(AI, DI, DO, EQP)
	rsltTmp[0].eSnsrSub = eSNSRSUB_EQP_RX;		// ���� ���� ����(��, ����, ����, ��, ���Թ� ��)
	rsltTmp[0].idxSub = 0;						// ���� ������ 3���� ��ġ�Ǿ� �ִٸ� index�� ����(0,1,,)
	
	rsltTmp[0].eCnnStatus = eSnsrCnnST_Normal;	// bConnected, RTU�� ����Ǿ� �ִ��� ��/��
	rsltTmp[0].bRslt = 0;						// ����͸� ���
	
	// (4)
	memcpy(&snsrDI.rslt[0], &rsltTmp[0], sizeof(FMS_CollectRsltDIDO) );
	
	int i = 0;
#if 0 // �Ѱ� ������ ���
	PACKET  pkt;
	pkt.FillPacket(eFT_EQPDAT_RES, (LPBYTE)&snsrDI, sizeof(snsrDI) );
	if(m_pSvrRTU )
		m_pSvrRTU->SendSck((LPCTSTR)&pkt, pkt.GetRealSize() );
#else
	// ------------------
	// FMS_CollectRsltDIDO	rslt[1];
	rsltTmp[1].eSnsrType = eSNSRTYP_EQP;			// ���� Ÿ��(AI, DI, DO, EQP)
	
	// ���� ���� ���� -- ���-���ű�#1
	rsltTmp[1].eSnsrSub = (enumSNSR_SUB)(eSNSRSUB_EQP_RX + 1);	
	
	rsltTmp[1].idxSub = 0;						// ���� ������ 3���� ��ġ�Ǿ� �ִٸ� index�� ����(0,1,,)
	
	rsltTmp[1].eCnnStatus = eSnsrCnnST_Normal;		// bConnected, RTU�� ����Ǿ� �ִ��� ��/��
	rsltTmp[1].bRslt = 1;						// ����͸� ���
	snsrDI.nFcltSubCount = 2;
	
	// ==================================================
	char szBuffer[MAX_ASYNCSCK_BUFFER];
	int off = 0;
	
	memcpy(&snsrDI.rslt[0], &rsltTmp[0], sizeof(FMS_CollectRsltDIDO) );
	
	memcpy(szBuffer + 0, &snsrDI, sizeof(FCLT_CollectRslt_DIDO) );
	off += sizeof(FCLT_CollectRslt_DIDO);
	
	for(i = 1; i < snsrDI.nFcltSubCount; i++) {
		memcpy(szBuffer + off, &rsltTmp[i], sizeof(FMS_CollectRsltDIDO) );
		off += sizeof(FMS_CollectRsltDIDO);
	}
	
	// -----------------------------
	PACKET pkt;
	pkt.FillPacket(eFT_EQPDAT_RES, (short)eSYSD_EQPRTU, (LPBYTE)szBuffer, sizeof(char) + off);
	if(m_pSvrRTU )
		m_pSvrRTU->SendSck((LPCTSTR)&pkt, pkt.GetRealSize() );
#endif
}

// EQP -> RTU
// ������� OFF �ܰ躰 ������� ����
void CFMSEqpDeamonDlg::OnBtnEqpCtlResOff() 
{
	// ������� ���� ����μ� �߰� �������� ��� ����
	eFT_EQPCTL_RES;
	
	FCLT_CTRL_EQPOnOffRslt  &eqpCtlStatus = m_EqpCtlRsltStatus;
	
	// (1)
	// FMS_ElementFclt  tFcltElem;
	eqpCtlStatus.tFcltElem.nSiteID = m_nSiteID;		// ���ݱ� ID
	eqpCtlStatus.tFcltElem.nFcltID = m_nFcltID;			// FCLT_ID
	eqpCtlStatus.tFcltElem.eFclt = eFCLT_EQP01;			// ���񱸺�(��з� �Ǵ� �ý��� ����)
	
	// (2)
	eqpCtlStatus.eSnsrType = eSNSRTYP_EQP;				// ���� Ÿ��(AI, DI, DO, EQP)
	eqpCtlStatus.eSnsrSub = eSNSRSUB_EQP_ALL;			// ���� ���� ����(��, ����, ����, ��, ���Թ� ��)
	eqpCtlStatus.idxSub = 0;					// �µ������� 3���� ��ġ�Ǿ� �ִٸ� index�� ����(0,1,,)
	
	// ------------------
	// (3) 
	// FMS_TransDBData  tDbTans;
	eqpCtlStatus.tDbTans.timeT = (long)CTime::GetCurrentTime().GetTime();		// time_t  now; // "%Y%m%d%H%M%S"
	
	// ---------------------------------------s
	// ������ ���� �� _ACK ���� �� ���ڵ� ID
	// SNSR_TYPE	00	AI	Y
	// SNSR_TYPE	01	DI	Y
	// SNSR_TYPE	02	DO	Y
	// SNSR_TYPE	03	�������	Y
	int nSnsrType = 1; // 
//	CString str = GetDBFclt_TID("TFMS_CTLHIST_DTL", m_nSiteID, nSnsrType);
	CString str = "";
	
	// wsprintf(snsrCtl.tDbTans.sDBFCLT_TID, "000001");
	wsprintf(eqpCtlStatus.tDbTans.sDBFCLT_TID, str);
	// ---------------------------------------e
	
	// (4)
	wsprintf(eqpCtlStatus.sUserID, "%s", "");
	
	// ---------------------
	// (5)
	eqpCtlStatus.eCtlType = eFCLTCtlTYP_DoCtrl;		// ���� Ÿ��
	if(!m_bEQPCTL_POWER_Timer)
		eqpCtlStatus.eCtlCmd = eFCLTCtlCMD_Off;		// ���� ���� ��� : ���� Off �Ǵ� On ���μ��� ��� ����
	else
		eqpCtlStatus.eCtlCmd = m_EqpCtlST.eCtlCmd;
	
	// (6)
	if(!m_bEQPCTL_POWER_Timer) {
		eqpCtlStatus.eMaxStep = (enumFCLTOnOffStep)(eEQPCTL_STEP1 + 4);  // �������� ���μ����� ��ġ�� MAX �ܰ�
		eqpCtlStatus.eCurStep = (enumFCLTOnOffStep)(eEQPCTL_STEP1 + 0);	 // ����ܰ�: 1) �������SBC ���ֵ��� OFF, 2) ���׳�����PC OS ����, , 
		eqpCtlStatus.eCurStatus = eFCLTCtlSt_End;		// �ܰ躰 ó�� ���� : start, ing, end �߿� �Ѱ��� ����
	} else {
		eqpCtlStatus.eMaxStep = m_EqpCtlST.eMaxStep;
		eqpCtlStatus.eCurStep = m_EqpCtlST.eCurStep; // **
		eqpCtlStatus.eCurStatus = m_EqpCtlST.eCurStatus;// **
	}
	
	PACKET  pkt;
	pkt.FillPacket(eFT_EQPCTL_RES, (short)eSYSD_EQPRTU, (LPBYTE)&eqpCtlStatus, sizeof(eqpCtlStatus) );
	if(m_pSvrRTU )
		m_pSvrRTU->SendSck((LPCTSTR)&pkt, pkt.GetRealSize() );
}

// EQP -> RTU
// ������� ON �ܰ躰 ������� ����
void CFMSEqpDeamonDlg::OnBtnEqpCtlResOn() 
{
	// ������� ���� ����μ� �߰� �������� ��� ����
	eFT_EQPCTL_RES;
	
	FCLT_CTRL_EQPOnOffRslt  eqpCtlStatus;
	
	// (1)
	// FMS_ElementFclt  tFcltElem;
	eqpCtlStatus.tFcltElem.nSiteID = m_nSiteID;		// ���ݱ� ID
	eqpCtlStatus.tFcltElem.nFcltID = m_nFcltID;		// FCLT_ID
	eqpCtlStatus.tFcltElem.eFclt = eFCLT_EQP01;		// ���񱸺�(��з� �Ǵ� �ý��� ����)
	
	// (2)
	eqpCtlStatus.eSnsrType = eSNSRTYP_EQP;			// ���� Ÿ��(AI, DI, DO, EQP)
	eqpCtlStatus.eSnsrSub = eSNSRSUB_EQP_ALL;		// ���� ���� ����(��, ����, ����, ��, ���Թ� ��)
	eqpCtlStatus.idxSub = 0;						// �µ������� 3���� ��ġ�Ǿ� �ִٸ� index�� ����(0,1,,)
	
	// ------------------
	// (3) 
	// FMS_TransDBData  tDbTans;
	eqpCtlStatus.tDbTans.timeT = (long)CTime::GetCurrentTime().GetTime();		// time_t  now; // "%Y%m%d%H%M%S"
	
	// ---------------------------------------s
	// ������ ���� �� _ACK ���� �� ���ڵ� ID
	// SNSR_TYPE	00	AI	Y
	// SNSR_TYPE	01	DI	Y
	// SNSR_TYPE	02	DO	Y
	// SNSR_TYPE	03	�������	Y
	int nSnsrType = 1; // 
	//	CString str = GetDBFclt_TID("TFMS_CTLHIST_DTL", m_nSiteID, nSnsrType);
	CString str = "";
	
	// wsprintf(snsrCtl.tDbTans.sDBFCLT_TID, "000001");
	wsprintf(eqpCtlStatus.tDbTans.sDBFCLT_TID, str);
	// ---------------------------------------e
	
	// (4)
	wsprintf(eqpCtlStatus.sUserID, "%s", "");
	
	// ---------------------
	// (5)
	eqpCtlStatus.eCtlType = eFCLTCtlTYP_DoCtrl;		// ���� Ÿ��
	eqpCtlStatus.eCtlCmd = eFCLTCtlCMD_On;			// ���� ���� ��� : ���� Off �Ǵ� On ���μ��� ��� ����
	
	// (6)
	eqpCtlStatus.eMaxStep = (enumFCLTOnOffStep)(eEQPCTL_STEP1 + 4);  // �������� ���μ����� ��ġ�� MAX �ܰ�
	eqpCtlStatus.eCurStep = (enumFCLTOnOffStep)(eEQPCTL_STEP1 + 0);	 // ����ܰ�: 1) �������SBC ���ֵ��� OFF, 2) ���׳�����PC OS ����, , 
	eqpCtlStatus.eCurStatus = eFCLTCtlSt_End;		// �ܰ躰 ó�� ���� : start, ing, end �߿� �Ѱ��� ����
	
	PACKET  pkt;
	pkt.FillPacket(eFT_EQPCTL_RES, (short)eSYSD_EQPRTU, (LPBYTE)&eqpCtlStatus, sizeof(eqpCtlStatus) );
	if(m_pSvrRTU )
		m_pSvrRTU->SendSck((LPCTSTR)&pkt, pkt.GetRealSize() );
}

// EQP -> RTU
// �ڵ� ���� On/Off 
void CFMSEqpDeamonDlg::OnBtnEqpCtlPowerAuto() 
{
	// TODO: Add your control notification handler code here

	// ������� ���� ����
	eFT_EQPCTL_POWER;
	
	FCLT_CTRL_EQPOnOff		eqpCtl;
	
	// (1)
	// FMS_ElementFclt  tFcltElem;
	eqpCtl.tFcltElem.nSiteID = m_nSiteID;			// ���ݱ� ID
	eqpCtl.tFcltElem.nFcltID = m_nFcltID;			// FCLT_ID
	eqpCtl.tFcltElem.eFclt = eFCLT_EQP01;			// ���񱸺�(��з� �Ǵ� �ý��� ����)
	// -------------------------
	// (2)
	eqpCtl.eSnsrType = eSNSRTYP_EQP;				// ���� Ÿ��(AI, DI, DO, EQP)
	eqpCtl.eSnsrSub = eSNSRSUB_EQP_ALL;			// ���� ���� ����(��, ����, ����, ��, ���Թ� ��)
	// (3)
	eqpCtl.idxSub = 0;								// �µ������� 3���� ��ġ�Ǿ� �ִٸ� index�� ����(0,1,,)
	// ------------------
	// (4)
	// FMS_TransDBData  tDbTans;
	eqpCtl.tDbTans.timeT = (long)CTime::GetCurrentTime().GetTime();		// time_t  now; // "%Y%m%d%H%M%S"
	
	// ---------------------------------------s
	// ������ ���� �� _ACK ���� �� ���ڵ� ID
	// wsprintf(snsrCtl.tDbTans.sDBFCLT_TID, "000001");
	// SNSR_TYPE	00	AI	Y
	// SNSR_TYPE	01	DI	Y
	// SNSR_TYPE	02	DO	Y
	// SNSR_TYPE	03	�������	Y
	int nSnsrType = 1; // 
//	CString str = GetDBFclt_TID("TFMS_CTLHIST_DTL", m_nSiteID, nSnsrType);
	CString str = "";
	// wsprintf(snsrCtl.tDbTans.sDBFCLT_TID, "000001");
	wsprintf(eqpCtl.tDbTans.sDBFCLT_TID, str);
	// ---------------------------------------e
	
	// (5)
	wsprintf(eqpCtl.sUserID, "%s", "");
	
	// -------------------------
	// (6) (7)
	eqpCtl.eCtlType = eFCLTCtlTYP_DoCtrl; // CTL_TYPE_CD	16	DO����	Y
	eqpCtl.eCtlCmd =  eFCLTCtlCMD_Off;// CTL_CMD_CD	00	Off	Y
	
	eqpCtl.bAutoMode = TRUE;		  // �������� �SW ������ �ڵ����� OFF->ON �ϰ��� �ϴ� ��� �ڡڡ�
	
	PACKET  pkt;
	pkt.FillPacket(eFT_EQPCTL_POWER, (short)eSYSD_EQPRTU, (LPBYTE)&eqpCtl, sizeof(eqpCtl) );
	if(m_pSvrRTU )
		m_pSvrRTU->SendSck((LPCTSTR)&pkt, pkt.GetRealSize() );
}

void CFMSEqpDeamonDlg::OnTimer(UINT nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default
	switch(nIDEvent) {
	case IDT_TEST_EQPDAT_REQ:
		KillTimer(nIDEvent);
		// eFT_EQPDAT_REQ �� ���� ó�� �׽�Ʈ!
		OnBtnEqpDatRes();
		break;
	case IDT_TEST_EQPCTL_POWER:
		{
			if(!m_pSvrRTU)
				KillTimer(nIDEvent);

			switch(++m_iWaitCountEqpCtrl) 
			{
			case 0:
			default:
				{
					// m_EqpCtlST.bAutoMode;
					// eFCLTCtlCMD_Off == m_EqpCtlST.eCtlCmd
					
					// ���� ON/OFF
					m_EqpCtlST.eCurStep = (enumFCLTOnOffStep)(m_EqpCtlST.eCurStep + 1);
					m_EqpCtlST.eCurStatus = eFCLTCtlSt_End;
					OnBtnEqpCtlResOff();
					SckTrace("eFT_EQPCTL_POWER : ������� �������� Step -- max=%d, cur=%d", m_EqpCtlST.eMaxStep, m_EqpCtlST.eCurStep);

					if(m_EqpCtlST.eMaxStep == m_EqpCtlST.eCurStep) {
						SendMessage(CCSMSG_AsyncSckServerEQP, 22, 0);
					}
						

					
				} break;
			}

		} break;
	}
	
	CDialog::OnTimer(nIDEvent);
}

// CCSMSG_AsyncSckServerEQP
LRESULT CFMSEqpDeamonDlg::OnAsyncSckServerEQP(WPARAM wParam, LPARAM lParam)
{
	if(11 == wParam) { // start
		SckTrace("eFT_EQPCTL_POWER : ������� �������� ���μ��� ����, bAutoMode=%d, bOnOff=%d", m_pSvrRTU->m_eqpCtl.bAutoMode, m_pSvrRTU->m_eqpCtl.eCtlCmd);

		m_iWaitCountEqpCtrl = 0;
		ASSERT(eFCLTCtlTYP_DoCtrl == m_pSvrRTU->m_eqpCtl.eCtlType);

		m_EqpCtlST.eCtlCmd = m_pSvrRTU->m_eqpCtl.eCtlCmd;
		m_EqpCtlST.eMaxStep = (enumFCLTOnOffStep)(eEQPCTL_STEP1 + 2);
		m_EqpCtlST.eCurStep = eEQPCTL_STEP_None;
		m_EqpCtlST.eCurStatus = eFCLTCtlSt_Start;
		m_EqpCtlST.bAutoMode = m_pSvrRTU->m_eqpCtl.bAutoMode;


		if(FALSE == m_EqpCtlST.bAutoMode) {
			if(eFCLTCtlCMD_Off == m_EqpCtlST.eCtlCmd) {
				m_EqpCtlST.eMaxStep = (enumFCLTOnOffStep)(eEQPCTL_STEP1 + 2);
			} else if(eFCLTCtlCMD_On == m_EqpCtlST.eCtlCmd) {
				m_EqpCtlST.eMaxStep = (enumFCLTOnOffStep)(eEQPCTL_STEP1 + 2);
			}
		} else {
			m_EqpCtlST.eMaxStep = (enumFCLTOnOffStep)(eEQPCTL_STEP1 + 5);
		}

		m_bEQPCTL_POWER_Timer = TRUE;
		SetTimer(IDT_TEST_EQPCTL_POWER, 2000, NULL);
	} else if(22 == wParam) { // end
		KillTimer(IDT_TEST_EQPCTL_POWER);
		m_iWaitCountEqpCtrl = -1;
		m_bEQPCTL_POWER_Timer = FALSE;
		SckTrace("eFT_EQPCTL_POWER : ������� �������� ���μ��� ����");

	}

	return 0L;
}
