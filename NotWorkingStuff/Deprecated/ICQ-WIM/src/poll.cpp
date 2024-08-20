// -----------------------------------------------------------------------------
// ICQ plugin for Miranda NG
// -----------------------------------------------------------------------------
// Copyright © 2018-24 Miranda NG team
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
// -----------------------------------------------------------------------------
// Long-poll thread and its item handlers

#include "stdafx.h"

void CIcqProto::ProcessBuddyList(const JSONNode &ev)
{
	m_arGroups.destroy();

	LIST<IcqGroup> tmpGroups(10);
	bool bEnableMenu = false;

	for (auto &it : ev["groups"]) {
		auto *pGroup = new IcqGroup(it["id"].as_int(), it["name"].as_mstring());
		debugLogA("new group: id=%d, level=%d, name=%S", pGroup->id, pGroup->level, pGroup->wszName.c_str());
		if (pGroup->level != 0) {
			for (auto &p : tmpGroups.rev_iter()) {
				if (p->level == pGroup->level-1) {
					pGroup->wszName = p->wszName + L"\\" + pGroup->wszName;
					debugLogA("Group name fixed as %S", pGroup->wszName.c_str());
					break;
				}
			}
		}
		tmpGroups.insert(pGroup);
		m_arGroups.insert(pGroup);

		bool bCreated = false;

		for (auto &buddy : it["buddies"]) {
			MCONTACT hContact = ParseBuddyInfo(buddy);
			if (hContact == INVALID_CONTACT_ID)
				continue;

			ProcessOnline(buddy, hContact);
			setWString(hContact, "IcqGroup", pGroup->wszName);

			if (!bCreated) {
				Clist_GroupCreate(0, pGroup->wszName);
				bCreated = true;
			}

			ptrW mirGroup(Clist_GetGroup(hContact));
			if (mir_wstrcmp(mirGroup, pGroup->wszName))
				bEnableMenu = true;

			if (!mirGroup)
				Clist_SetGroup(hContact, pGroup->wszName);
		}
	}

	if (bEnableMenu)
		Menu_ShowItem(m_hUploadGroups, true);

	mir_cslock lck(m_csCache);
	for (auto &it : m_arCache)
		if (!it->m_bInList && !getBool(it->m_hContact, "IcqDeleted"))
			Contact::RemoveFromList(it->m_hContact);
}

void CIcqProto::ProcessDiff(const JSONNode &ev)
{
	for (auto &block : ev) {
		CMStringW szType = block["type"].as_mstring();
		if (szType != "updated" && szType != "created" && szType != "deleted")
			continue;

		for (auto &it : block["data"]) {
			int grpId = it["id"].as_int();
			CMStringW wszName = it["name"].as_mstring();

			auto *pGroup = m_arGroups.find((IcqGroup *)&grpId);
			if (pGroup == nullptr) {
				if (szType != "created") {
					debugLogA("Group %d isn't found", grpId);
					continue;
				}

				pGroup = new IcqGroup(grpId, wszName);
				m_arGroups.insert(pGroup);
			}
			else {
				pGroup->wszSrvName = wszName;
				pGroup->SetName(wszName);
			}

			bool bCreated = false, bDeleted = (szType == "deleted");

			int iProcessed = 0;
			for (auto &buddy : it["buddies"]) {
				MCONTACT hContact = ParseBuddyInfo(buddy, true);
				if (hContact == INVALID_CONTACT_ID)
					continue;

				iProcessed++;
				if (bDeleted) {
					db_delete_contact(hContact, CDF_FROM_SERVER);
					continue;
				}

				ProcessOnline(buddy, hContact);
				setWString(hContact, "IcqGroup", pGroup->wszName);

				if (!bCreated) {
					Clist_GroupCreate(0, pGroup->wszName);
					bCreated = true;
				}

				ptrW wszGroup(Clist_GetGroup(hContact));
				if (!wszGroup)
					Clist_SetGroup(hContact, pGroup->wszName);
			}

			if (bDeleted && !iProcessed)
				m_arGroups.remove(pGroup);
		}

		RefreshGroups();
	}
}

void CIcqProto::ProcessEvent(const JSONNode &ev)
{
	const JSONNode &pData = ev["eventData"];
	CMStringW szType = ev["type"].as_mstring();
	if (szType == L"buddylist")
		ProcessBuddyList(pData);
	else if (szType == L"diff")
		ProcessDiff(pData);
	else if (szType == L"histDlgState")
		ProcessHistData(pData);
	else if (szType == L"imState")
		ProcessImState(pData);
	else if (szType == L"mchat")
		ProcessGroupChat(pData);
	else if (szType == L"myInfo")
		ProcessMyInfo(pData);
	else if (szType == L"notification")
		ProcessNotification(pData);
	else if (szType == L"permitDeny")
		ProcessPermissions(pData);
	else if (szType == L"presence")
		ProcessPresence(pData);
	else if (szType == L"sessionEnded")
		ProcessSessionEnd(pData);
	else if (szType == L"typing")
		ProcessTyping(pData);
}

void CIcqProto::ProcessHistData(const JSONNode &ev)
{
	MCONTACT hContact;
	bool bVeryBeginning = m_bFirstBos;

	CMStringW wszId(ev["sn"].as_mstring());
	auto *pUser = FindUser(wszId); // might be NULL for groupchats

	if (IsChat(wszId)) {
		SESSION_INFO *si = Chat_Find(wszId, m_szModuleName);
		if (si == nullptr) {
			CMStringW wszNick;
			for (auto &it : ev["persons"])
				if (it["sn"].as_mstring() == wszId) {
					wszNick = it["friendly"].as_mstring();
					break;
				}

			if ((si = GcCreate(wszId, wszNick)) == nullptr)
				return;
		}

		hContact = si->hContact;

		__int64 srvInfoVer = _wtoi64(ev["mchatState"]["infoVersion"].as_mstring());
		__int64 srvMembersVer = _wtoi64(ev["mchatState"]["membersVersion"].as_mstring());
		if (srvInfoVer != getId(hContact, "InfoVersion") || srvMembersVer != getId(hContact, "MembersVersion") || si->arUsers.getCount() == 0)
			RetrieveChatInfo(hContact);
	}
	else {
		hContact = CreateContact(wszId, true);

		// for temporary contacts that just gonna be created
		if (pUser == nullptr) {
			bVeryBeginning = true;
			pUser = FindUser(wszId);
		}
	}

	// restore reading from the previous point, if we just installed Miranda
	__int64 lastMsgId = getId(hContact, DB_KEY_LASTMSGID);
	if (lastMsgId == 0) {
		lastMsgId = _wtoi64(ev["yours"]["lastRead"].as_mstring());
		setId(hContact, DB_KEY_LASTMSGID, lastMsgId);
	}

	if (pUser && pUser->m_bSkipPatch) {
		pUser->m_bSkipPatch = false;
		goto LBL_SkipPatch;
	}

	ProcessPatchVersion(hContact, _wtoi64(ev["patchVersion"].as_mstring()));
LBL_SkipPatch:

	__int64 srvLastId = _wtoi64(ev["lastMsgId"].as_mstring());

	// we load history in the very beginning or if the previous message 
	if (bVeryBeginning) {
		if (pUser) {
			debugLogA("Setting cache = %lld for %d", srvLastId, hContact);
			pUser->m_iProcessedMsgId = srvLastId;
		}

		if (srvLastId > lastMsgId) {
			debugLogA("We need to retrieve history for %S: %lld > %lld", wszId.c_str(), srvLastId, lastMsgId);
			RetrieveUserHistory(hContact, lastMsgId, false);
		}
	}
	else {
		if (!(pUser && pUser->m_iProcessedMsgId >= srvLastId)) {
			if (pUser)
				debugLogA("Proceeding with cache for %d: %lld < %lld", hContact, pUser->m_iProcessedMsgId, srvLastId);
			else
				debugLogA("Proceeding with empty cache for %d", hContact);

			for (auto &it : ev["intro"]["messages"])
				ParseMessage(hContact, lastMsgId, it, 0);

			for (auto &it : ev["tail"]["messages"])
				ParseMessage(hContact, lastMsgId, it, PM::LocalTime);

			setId(hContact, DB_KEY_LASTMSGID, lastMsgId);
			if (pUser) {
				pUser->m_iProcessedMsgId = lastMsgId;
				debugLogA("Setting second cache = %lld for %d", srvLastId, hContact);
			}
		}
	}

	// check remote read
	if (g_bMessageState) {
		__int64 srvRemoteRead = _wtoi64(ev["theirs"]["lastRead"].as_mstring());
		__int64 lastRemoteRead = getId(hContact, DB_KEY_REMOTEREAD);
		if (srvRemoteRead > lastRemoteRead) {
			setId(hContact, DB_KEY_REMOTEREAD, srvRemoteRead);

			if (g_bMessageState)
				CallService(MS_MESSAGESTATE_UPDATE, hContact, MRD_TYPE_READ);
		}
	}
}

void CIcqProto::ProcessImState(const JSONNode &ev)
{
	for (auto &it : ev["imStates"]) {
		if (it["state"].as_mstring() != L"delivered")
			continue;

		CMStringA reqId(it["sendReqId"].as_mstring());
		CMStringA msgId(it["histMsgId"].as_mstring());
		MCONTACT hContact = CheckOwnMessage(reqId, msgId, false);
		if (hContact)
			CheckLastId(hContact, ev);
	}
}

void CIcqProto::ProcessMyInfo(const JSONNode &ev)
{
	Json2string(0, ev, "friendly", "Nick", true);
	Json2string(0, ev, "attachedPhoneNumber", DB_KEY_PHONE, true);

	CheckAvatarChange(0, ev);
}

void CIcqProto::ProcessNotification(const JSONNode &ev)
{
	for (auto &fld : ev["fields"]) {
		const JSONNode &email = fld["mailbox.newMessage"];
		if (email) {
			JSONROOT root(email.as_string().c_str());
			CMStringW wszFrom((*root)["from"].as_mstring());
			CMStringW wszSubj((*root)["subject"].as_mstring());
			m_unreadEmails = (*root)["unreadCount"].as_int();
			debugLogW(L"You received e-mail (%d) from <%s>: <%s>", m_unreadEmails, wszFrom.c_str(), wszSubj.c_str());

			CMStringW wszMessage(FORMAT, TranslateT("You received e-mail from %s: %s"), wszFrom.c_str(), wszSubj.c_str());
			EmailNotification(wszMessage);
		}

		const JSONNode &status = fld["mailbox.status"];
		if (status) {
			int iOldCount = m_unreadEmails;

			JSONROOT root(status.as_string().c_str());
			m_szMailBox = (*root)["email"].as_mstring();			
			m_unreadEmails = (*root)["unreadCount"].as_int();
			
			// we've read/removed some messages from server
			if (iOldCount > m_unreadEmails) {
				Clist_RemoveEvent(0, ICQ_FAKE_EVENT_ID);
				return;
			}

			// we notify about initial mail count only during login
			if (m_bFirstBos && m_unreadEmails > 0) {
				CMStringW wszMessage(FORMAT, TranslateT("You have %d unread emails"), m_unreadEmails);
				EmailNotification(wszMessage);
			}
		}
	}
}

void CIcqProto::ProcessPresence(const JSONNode &ev)
{
	CMStringW aimId = ev["aimId"].as_mstring();

	auto *pUser = FindUser(aimId);
	if (pUser == nullptr)
		return;

	MCONTACT hContact = GetRealContact(pUser);
	int iNewStatus = StatusFromPresence(ev, hContact);
	if (iNewStatus != -1)
		ProcessStatus(pUser, iNewStatus);

	if (hContact != m_hFavContact)
		Json2string(hContact, ev, "friendly", "Nick", true);
	CheckAvatarChange(hContact, ev);
}

void CIcqProto::ProcessSessionEnd(const JSONNode &/*ev*/)
{
	m_szRToken.Empty();
	m_iRClientId = 0;
	delSetting(DB_KEY_RCLIENTID);

	ShutdownSession();
}

void CIcqProto::ProcessTyping(const JSONNode &ev)
{
	CMStringW aimId = ev["aimId"].as_mstring();
	CMStringW wszStatus = ev["typingStatus"].as_mstring();

	auto *pUser = FindUser(aimId);
	if (pUser) {
		MCONTACT hContact = GetRealContact(pUser);
		if (wszStatus == "typing")
			CallService(MS_PROTO_CONTACTISTYPING, hContact, 60);
		else
			CallService(MS_PROTO_CONTACTISTYPING, hContact, PROTOTYPE_CONTACTTYPING_OFF);
	}
}

void CIcqProto::OnFetchEvents(MHttpResponse *pReply, AsyncHttpRequest*)
{
	JsonReply root(pReply);
	if (root.error() != 200) {
		ShutdownSession();
		return;
	}

	JSONNode &data = root.data();
	m_fetchBaseURL = data["fetchBaseURL"].as_mstring();

	for (auto &it : data["events"])
		ProcessEvent(it);
}

void __cdecl CIcqProto::PollThread(void*)
{
	debugLogA("Polling thread started");
	m_bFirstBos = true;

	while (m_bOnline && !m_fetchBaseURL.IsEmpty()) {
		CMStringA szUrl = m_fetchBaseURL;
		if (m_bFirstBos)
			szUrl.Append("&first=1");
		else
			szUrl.Append("&timeout=25000");

		if (m_bInvisible)
			szUrl.Append("&bg=1&hidden=1");

		auto *pReq = new AsyncHttpRequest(CONN_FETCH, REQUEST_GET, szUrl, &CIcqProto::OnFetchEvents);
		if (!m_bFirstBos)
			pReq->timeout = 62000;

		if (!ExecuteRequest(pReq)) {
			ShutdownSession();
			break;
		}

		m_bFirstBos = false;
	}

	debugLogA("Polling thread ended");
}
