#include "stdafx.h"

void CEmLanProto::Start()
{
	if (checkContactsThread)
		return;

	TContact *contact = m_pRootContact;
	while (contact)
	{
		contact->m_time = MLAN_CHECK + MLAN_TIMEOUT;
		contact = contact->m_prev;
	}

	isCheckingStarted = true;
	checkContactsThread = ForkThreadEx(&CEmLanProto::CheckContactsThread, NULL, 0);
	StartListen();
	RequestStatus(true);

	WORD oldStatus = m_iStatus;
	m_iStatus = m_iDesiredStatus;
	ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)oldStatus, m_iStatus);
}

void CEmLanProto::Stop()
{
	{
		mir_cslock lock(checkContactsLock);
		isCheckingStarted = true;
		checkContactsThread = false;
	}

	{
		mir_cslock lock(receiveThreadLock);
		//m_mirStatus = ID_STATUS_OFFLINE;
		RequestStatus(false);
		StopListen();
	}

	TFileConnection *fc = m_pFileConnectionList;
	while (fc)
	{
		fc->Terminate();
		fc = fc->m_pNext;
	}
	while (m_pFileConnectionList)
		Sleep(10);

	SetAllContactsStatus(ID_STATUS_OFFLINE);
}
