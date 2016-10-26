#include "stdafx.h"

void CEmLanProto::SendMessageThread(void *arg)
{
	TDataParam *param = (TDataParam*)arg;
	CEmLanProto *proto = param->proto;

	if (proto->IsOnline())
	{
		TPacket pak;
		memset(&pak, 0, sizeof(pak));
		pak.strMessage = param->message;
		pak.idMessage = param->id;

		u_long addr = proto->getDword(param->hContact, "ipaddr", 0);
		
		proto->SendPacketExt(pak, addr);
	}
	else
		proto->ProtoBroadcastAck(param->hContact, ACKTYPE_MESSAGE, ACKRESULT_FAILED, (HANDLE)param->id, 0);

	delete param;
}

int CEmLanProto::OnSendMessage(MCONTACT hContact, int flags, const char *msg)
{
	ULONG hMessage = InterlockedIncrement(&hMessageProcess);

	ptrA message;
	if (flags & PREF_UNICODE)
		message = mir_utf8encodeW((wchar_t*)&msg[mir_strlen(msg) + 1]);
	else if (flags & PREF_UTF)
		message = mir_strdup(msg);
	else
		message = mir_utf8encode(msg);

	TDataParam *param = new TDataParam(hContact, message, hMessage, /*LEXT_SENDMESSAGE*/ 0, this);

	mir_forkthread(&CEmLanProto::SendMessageThread, param);
	
	return hMessage;
}