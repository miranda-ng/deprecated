#include "stdafx.h"

CEmLanProto::CEmLanProto(const char* protoName, const TCHAR* userName) :
	PROTO<CEmLanProto>(protoName, userName),
	hMessageProcess(1)
{
	CreateProtoService(PS_CREATEACCMGRUI, &CEmLanProto::OnAccountManagerInit);
}

CEmLanProto::~CEmLanProto()
{
}

DWORD_PTR CEmLanProto::GetCaps(int type, MCONTACT)
{
	switch (type)
	{
	case PFLAGNUM_1:
		return PF1_IM | PF1_BASICSEARCH | PF1_ADDSEARCHRES | PF1_PEER2PEER | PF1_INDIVSTATUS |
			PF1_URLRECV | PF1_MODEMSG | PF1_FILE | PF1_CANRENAMEFILE | PF1_FILERESUME;
	case PFLAGNUM_2:
		return PF2_ONLINE | PF2_SHORTAWAY | PF2_LONGAWAY | PF2_LIGHTDND | PF2_HEAVYDND | PF2_FREECHAT;
	case PFLAGNUM_3:
		return PF2_SHORTAWAY | PF2_LONGAWAY | PF2_LIGHTDND | PF2_HEAVYDND | PF2_FREECHAT;
	case PFLAG_UNIQUEIDTEXT:
		return (INT_PTR)Translate("User name or '*'");
	case PFLAG_UNIQUEIDSETTING:
		return (INT_PTR)"Nick";
	case PFLAG_MAXLENOFMESSAGE: //FIXME
	default:
		return 0;
	}
}

MCONTACT CEmLanProto::AddToList(int flags, PROTOSEARCHRESULT *psr)
{
	return AddToContactList((u_int)flags, (EMPSEARCHRESULT*)psr);
}

HANDLE CEmLanProto::FileAllow(MCONTACT hContact, HANDLE hTransfer, const PROTOCHAR* tszPath) { return 0; }
int CEmLanProto::FileCancel(MCONTACT hContact, HANDLE hTransfer) { return 0; }
int CEmLanProto::FileDeny(MCONTACT hContact, HANDLE hTransfer, const PROTOCHAR* tszReason) { return 0; }
int CEmLanProto::FileResume(HANDLE hTransfer, int* action, const PROTOCHAR** tszFilename) { return 0; }

int CEmLanProto::RecvMsg(MCONTACT hContact, PROTORECVEVENT *pre) { return 0; }

int CEmLanProto::SendMsg(MCONTACT hContact, int flags, const char *msg)
{
	return OnSendMessage(hContact, flags, msg);
}

int CEmLanProto::SetStatus(int iNewStatus)
{
	if (iNewStatus == m_iDesiredStatus)
		return 0;

	int old_status = m_iStatus;
	m_iDesiredStatus = iNewStatus;

	if (iNewStatus == ID_STATUS_OFFLINE)
	{
		Stop();
		m_iStatus = m_iDesiredStatus = ID_STATUS_OFFLINE;
	}
	else
	{
		if (old_status == ID_STATUS_CONNECTING)
			return 0;

		if (old_status == ID_STATUS_OFFLINE)
		{
			m_iStatus = m_iDesiredStatus = ID_STATUS_ONLINE;
			Start();
		}
		/*else if (m_iStatus == ID_STATUS_INVISIBLE)
		{
		ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)m_mirStatus, m_mirStatus);
		return;
		}*/
		else
		{
			RequestStatus(false);
		}
	}

	ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)old_status, m_iStatus);
	return 0;
}

int CEmLanProto::OnEvent(PROTOEVENTTYPE iEventType, WPARAM wParam, LPARAM lParam)
{
	return 0;
}