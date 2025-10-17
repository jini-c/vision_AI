// _Protocol_Common.cpp: implementation of the C_Protocol_Common class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "_Protocol_Common.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////////////


CString gfn_GetDBDateTime(time_t tm_time)
{
	char sDateTime[30];
	struct tm* st_time = NULL;
	// 날짜 / 시간
	st_time = localtime(&tm_time);

	if(!st_time)
		return "";
	
	//	to_date('2007.07.16 23:59:59','YYYY.mm.dd HH24:MI:SS')
	strftime(sDateTime, sizeof(sDateTime), "%Y.%m.%d %H:%M:%S", st_time);
	
	return CString(sDateTime);
}


CString GetStr_FCLT_TYPE(enumFCLT_TYPE e)
{
	CString str;

	if(eFCLT_FcltAll == e) {
		str = "eFCLT_FcltAll";
	} else if(eFCLT_RTU == e) {
		str = "eFCLT_RTU";
	} else if(eFCLT_DV == e) {
		str = "eFCLT_DV";
	} else if(eFCLT_EQP01 <= e && 98 >= (int)e) {
		str.Format("eFCLT_EQP%02d", e);
	} else if(eFCLT_TIME == e) {
		str = "eFCLT_TIME";
	} else {
		ASSERT(0);
		str = "eFCLT_Unknown";
	}
	return str;
}

CString GetStr_MethodType(enumMethodType e)
{
	CString str;
	switch(e) {
	case eDBMethod_Rslt:		// 모니터링 서버 전송
		str = "eDBMethod_Rslt";		break;
		// -------------------------
	case eDBMethod_Event:	// 장애 이벤트 서버 전송
		str = "eDBMethod_Event";		break;
	case eDBMethod_Ctrl:		// 전원제어(ON/OFF) 이력
		str = "eDBMethod_Ctrl";		break;
	case eDBMethod_Thrld:	// 항목별 임계 설정 전송
		str = "eDBMethod_Thrld";		break;
		// -------------------------
	case eDBMethod_Time:		// 시간설정 
		str = "eDBMethod_Time";		break;
	default:
		str = "eFCLT_ Unknown";
	}
	return str;
}

CString GetStr_FMS_CMD(enumFMS_CMD e)
{
	CString str;
	switch(e) {
	case eFT_NETWORK_CONFIRM_REQ:
		str = "eFT_NETWORK_CONFIRM_REQ";		break;
	case eFT_NETWORK_CONFIRM_RES:
		str = "eFT_NETWORK_CONFIRM_RES";		break;

	case eFT_SNSRDAT_RES:
		str = "eFT_SNSRDAT_RES";		break;
	case eFT_SNSRDAT_RESACK:
		str = "eFT_SNSRDAT_RESACK";		break;
	case eFT_SNSRCUDAT_REQ:
		str = "eFT_SNSRCUDAT_REQ";		break;
	case eFT_SNSRCUDAT_RES:
		str = "eFT_SNSRCUDAT_RES";		break;

	case eFT_SNSREVT_FAULT:
		str = "eFT_SNSREVT_FAULT";		break;
	case eFT_SNSREVT_FAULTACK:
		str = "eFT_SNSREVT_FAULTACK";		break;

	case eFT_SNSRCTL_POWER:
		str = "eFT_SNSRCTL_POWER";		break;
	case eFT_SNSRCTL_POWERACK:
		str = "eFT_SNSRCTL_POWERACK";		break;
	case eFT_SNSRCTL_RES:
		str = "eFT_SNSRCTL_RES";		break;
	case eFT_SNSRCTL_RESACK:
		str = "eFT_SNSRCTL_RESACK";		break;

	case eFT_SNSRCTL_THRLD:
		str = "eFT_SNSRCTL_THRLD";		break;
	case eFT_SNSRCTL_THRLDACK:
		str = "eFT_SNSRCTL_THRLDACK";		break;
	case eFT_SNSRCtlCompleted_THRLD:
		str = "eFT_SNSRCtlCompleted_THRLD";		break;
	
	case eFT_EQPDAT_REQ:
		str = "eFT_EQPDAT_REQ";		break;
	case eFT_EQPDAT_RES:
		str = "eFT_EQPDAT_RES";		break;
	case eFT_EQPDAT_RESACK:
		str = "eFT_EQPDAT_RESACK";		break;

	case eFT_EQPDAT_ACK:
		str = "eFT_EQPDAT_ACK";		break;

	case eFT_EQPCUDAT_REQ:
		str = "eFT_EQPCUDAT_REQ";		break;
	case eFT_EQPCUDAT_RES:
		str = "eFT_EQPCUDAT_RES";		break;

	case eFT_EQPEVT_FAULT:
		str = "eFT_EQPEVT_FAULT";		break;
	case eFT_EQPEVT_FAULTACK:
		str = "eFT_EQPEVT_FAULTACK";		break;

	case eFT_EQPCTL_POWER:
		str = "eFT_EQPCTL_POWER";		break;
	case eFT_EQPCTL_POWERACK:
		str = "eFT_EQPCTL_POWERACK";		break;
	case eFT_EQPCTL_RES:
		str = "eFT_EQPCTL_RES";		break;
	case eFT_EQPCTL_RESACK:
		str = "eFT_EQPCTL_RESACK";		break;
	
	case eFT_EQPCTL_THRLD:
		str = "eFT_EQPCTL_THRLD";		break;
	case eFT_EQPCTL_THRLDACK:
		str = "eFT_EQPCTL_THRLDACK";		break;
	case eFT_EQPCtlCompleted_THRLD:
		str = "eFT_EQPCtlCompleted_THRLD";		break;

	case eFTSVR_EVT_BASEDATUPDATE_REQ:
		str = "eFTSVR_EVT_BASEDATUPDATE_REQ";		break;

	case eFT_TIMEDAT_REQ:
		str = "eFT_TIMEDAT_REQ";		break;
	case eFT_TIMEDAT_RES:
		str = "eFT_TIMEDAT_RES";		break;
	case eFT_EVT_BASEDATUPDATE_REQ:
		str = "eFT_EVT_BASEDATUPDATE_REQ";		break;
	case eFT_EVT_BASEDATUPDATE_RES:
		str = "eFT_EVT_BASEDATUPDATE_RES";		break;
	case eFT_EVT_BASEDATUPDATE_RESET:
		str = "eFT_EVT_BASEDATUPDATE_RESET";		break;

	case eFT_NETALIVECHK_REQ:
		str = "eFT_NETALIVECHK_REQ";		break;
	case eFT_NETALIVECHK_RES:
		str = "eFT_NETALIVECHK_RES";		break;

	case eFT_FCLTNETINFO_REQ:
		str = "eFT_FCLTNETINFO_REQ";		break;
	case eFT_FCLTNETINFO_RES:
		str = "eFT_FCLTNETINFO_RES";		break;

	case eFT_EVT_FIRMWAREUPDATE_REQ:
		str = "eFT_EVT_FIRMWAREUPDATE_REQ";		break;
	case eFT_EVT_FIRMWAREUPDATE_NOTI:
		str = "eFT_EVT_FIRMWAREUPDATE_NOTI";		break;

	case eFT_EVT_FCLTNetwork_ERR:
		str = "eFT_EVT_FCLTNetwork_ERR";		break;
	case eFT_EVT_FCLTNetwork_ERRACK:
		str = "eFT_EVT_FCLTNetwork_ERRACK";		break;
	// --------------------------------
	default:
		str.Format("enumFMS_CMD_Unknown, eTag=%d\n", e);
	}
	return str;
}


CString GetStr_FMS_SYSDIR(enumFMS_SYSDIR e)
{
	CString str;
	switch(e) {
	case eSYSD_None:
		str = "eSYSD_None";		break;
	case eSYSD_RTUEQP:       // FMS_RTU --> RMS_EQP(감시장비)
		str = "eSYSD_RTUEQP";		break;
	case eSYSD_RTUFOP:       // FMS_RTU --> RMS_FOP
		str = "eSYSD_RTUFOP";		break;
	case eSYSD_RTUSVR:       // FMS_RTU --> RMS_FOP
		str = "eSYSD_RTUSVR";		break;

	case eSYSD_EQPRTU:		// FMS_EQP --> RMS_RTU 
		str = "eSYSD_EQPRTU";		break;

	case eSYSD_FOPRTU:		// FMS_OP --> FMS_RTU
		str = "eSYSD_FOPRTU";		break;
	case eSYSD_FOPSVR:       // FMS_OP --> FMS_SVR
		str = "eSYSD_FOPSVR";		break;

	case eSYSD_SVRRTU:		// FMS_SVR --> FMS_RTU
		str = "eSYSD_SVRRTU";		break;
	case eSYSD_SVRFOP:		// FMS_SVR --> FMS_FOP
		str = "eSYSD_SVRFOP";		break;
	// -----------------------
	case eSYSD_UPDRTU:       // UPD_SVR --> FMS_RTU 
		str = "eSYSD_UPDRTU";		break;
	case eSYSD_RTUUPD:       // FMS_RTU --> UPD_SVR (업로드 서버)
		str = "eSYSD_RTUUPD";		break;
	case eSYSD_UPDSVR:       // UPD_SVR --> FMS_SVR
		str = "eSYSD_UPDSVR";		break;
	case eSYSD_SVRUPD:       // UPD_SVR --> UPD_SVR // [2016/10/12 ykhong] : 
		str = "eSYSD_SVRUPD";		break;
	// ---------------
	case eSYSD_SVR2RSTRDB:		// [2016/10/7 ykhong] : FMS_SVR -> FMS_RST(RestoreRTU-DB.exe)
		str = "eSYSD_SVR2RSTRDB";		break;
	case eSYSD_RSTRDB2SVR:
		str = "eSYSD_RSTRDB2SVR";		break;
	case eSYSD_SVR2DBRPLC:		// [2016/10/7 ykhong] : FMS_SVR -> FMS_DBR(FMSDBReplicationMgr.exe : RTU-DB동기화)
		str = "eSYSD_SVR2DBRPLC";		break;
	case eSYSD_DBRPLC2SVR:
		str = "eSYSD_DBRPLC2SVR";		break;
	case eSYSD_SVR2SVR:
		str = "eSYSD_SVR2SVR";		break;
		// ================================
	default:
		str = "eSYSD_Unknown";		break;
	}
	return str;
}

