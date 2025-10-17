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

	case eFT_TIMEDAT_REQ:
		str = "eFT_TIMEDAT_REQ";		break;
	case eFT_TIMEDAT_RES:
		str = "eFT_TIMEDAT_RES";		break;
	case eFT_EVT_BASEDATUPDATE_REQ:
		str = "eFT_EVT_BASEDATUPDATE_REQ";		break;
	case eFT_EVT_BASEDATUPDATE_RES:
		str = "eFT_EVT_BASEDATUPDATE_RES";		break;
	// --------------------------------
	default:
		str = "enumFMS_CMD_Unknown";
	}
	return str;
}