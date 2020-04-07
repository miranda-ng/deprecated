/*
Plugin for Miranda NG for communicating with users of the MSN Messenger protocol.

Copyright (C) 2012-20 Miranda NG team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"
#include "msn_proto.h"

void CMsnProto::AvatarQueue_Init()
{
	hevAvatarQueue = ::CreateSemaphore(nullptr, 0, 255, nullptr);

	ForkThread(&CMsnProto::MSN_AvatarsThread, nullptr);
}

void CMsnProto::AvatarQueue_Uninit()
{
	::CloseHandle(hevAvatarQueue);
}

void CMsnProto::pushAvatarRequest(MCONTACT hContact, LPCSTR pszUrl)
{
	ProtoBroadcastAck(hContact, ACKTYPE_AVATAR, ACKRESULT_STATUS, nullptr);

	if (pszUrl != nullptr && *pszUrl != 0) {
		mir_cslock lck(csAvatarQueue);

		for (auto &it : lsAvatarQueue)
			if (it->hContact == hContact)
				return;

		lsAvatarQueue.insert(new AvatarQueueEntry(hContact, pszUrl));
		ReleaseSemaphore(hevAvatarQueue, 1, nullptr);
	}
}

bool CMsnProto::loadHttpAvatar(AvatarQueueEntry *p)
{
	NETLIBHTTPHEADER nlbhHeaders[1];
	nlbhHeaders[0].szName = "User-Agent";
	nlbhHeaders[0].szValue = (char*)MSN_USER_AGENT;

	NETLIBHTTPREQUEST nlhr = { sizeof(nlhr) };
	nlhr.requestType = REQUEST_GET;
	nlhr.flags = NLHRF_HTTP11 | NLHRF_REDIRECT;
	nlhr.szUrl = p->pszUrl;
	nlhr.headers = (NETLIBHTTPHEADER*)&nlbhHeaders;
	nlhr.headersCount = 1;

	NLHR_PTR nlhrReply(Netlib_HttpTransaction(m_hNetlibUser, &nlhr));
	if (nlhrReply == nullptr)
		return false;

	if (nlhrReply->resultCode != 200 || nlhrReply->dataLength == 0)
		return false;

	const wchar_t *szExt;
	int fmt = ProtoGetBufferFormat(nlhrReply->pData, &szExt);
	if (fmt == PA_FORMAT_UNKNOWN)
		return false;

	PROTO_AVATAR_INFORMATION ai = { 0 };
	ai.format = fmt;
	ai.hContact = p->hContact;
	MSN_GetAvatarFileName(ai.hContact, ai.filename, _countof(ai.filename), szExt);
	_wremove(ai.filename);

	int fileId = _wopen(ai.filename, _O_CREAT | _O_TRUNC | _O_WRONLY | O_BINARY, _S_IREAD | _S_IWRITE);
	if (fileId == -1)
		return false;

	_write(fileId, nlhrReply->pData, (unsigned)nlhrReply->dataLength);
	_close(fileId);

	ProtoBroadcastAck(p->hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, &ai);
	return true;
}

void __cdecl CMsnProto::MSN_AvatarsThread(void*)
{
	while (true) {
		if (WaitForSingleObject(hevAvatarQueue, INFINITE) != WAIT_OBJECT_0)
			break;

		if (g_bTerminated)
			break;

		AvatarQueueEntry *p = nullptr;
		{
			mir_cslock lck(csAvatarQueue);
			if (lsAvatarQueue.getCount() > 0) {
				p = lsAvatarQueue[0];
				lsAvatarQueue.remove(0);
			}
		}

		if (p == nullptr)
			continue;

		if (!loadHttpAvatar(p))
			ProtoBroadcastAck(p->hContact, ACKTYPE_AVATAR, ACKRESULT_FAILED, nullptr);
		delete p;
	}

	mir_cslock lck(csAvatarQueue);
	for (auto &it : lsAvatarQueue)
		delete it;
	lsAvatarQueue.destroy();
}
