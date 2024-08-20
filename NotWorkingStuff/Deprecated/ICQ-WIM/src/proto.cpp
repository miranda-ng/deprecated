// ---------------------------------------------------------------------------80
//                ICQ plugin for Miranda Instant Messenger
//                ________________________________________
//
// Copyright © 2000-2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
// Copyright © 2001-2002 Jon Keating, Richard Hughes
// Copyright © 2002-2004 Martin Öberg, Sam Kothari, Robert Rainwater
// Copyright © 2004-2010 Joe Kucera, George Hazan
// Copyright © 2012-2024 Miranda NG team
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
//  DESCRIPTION:
//
//  Protocol Interface Implementation
// -----------------------------------------------------------------------------

#include "stdafx.h"

#include "m_icolib.h"

#pragma warning(disable:4355)

static int CompareCache(const IcqUser *p1, const IcqUser *p2)
{
	return mir_wstrcmp(p1->m_aimid, p2->m_aimid);
}

CIcqProto::CIcqProto(const char *aProtoName, const wchar_t *aUserName) :
	PROTO<CIcqProto>(aProtoName, aUserName),
	m_impl(*this),
	m_arHttpQueue(10),
	m_arOwnIds(1, PtrKeySortT),
	m_arCache(20, &CompareCache),
	m_arGroups(10, NumericKeySortT),
	m_arDeleteQueue(10),
	m_arMarkReadQueue(10, NumericKeySortT),
	m_evRequestsQueue(CreateEvent(nullptr, FALSE, FALSE, nullptr)),
	m_szOwnId(this, DB_KEY_ID),
	m_iStatus1(this, "Status1", ID_STATUS_AWAY),
	m_iStatus2(this, "Status2", ID_STATUS_NA),
	m_iTimeDiff1(this, "TimeDiff1", 0),
	m_iTimeDiff2(this, "TimeDiff2", 0),
	m_bHideGroupchats(this, "HideChats", true),
	m_bUseTrayIcon(this, "UseTrayIcon", false),
	m_bErrorPopups(this, "ShowErrorPopups", true),
	m_bLaunchMailbox(this, "LaunchMailbox", true)
{
	db_set_resident(m_szModuleName, DB_KEY_ONLINETS);

	g_plugin.addPopupOption(CMStringW(FORMAT, TranslateT("%s error notifications"), m_tszUserName), m_bErrorPopups);

	// services
	CreateProtoService(PS_GETAVATARCAPS, &CIcqProto::GetAvatarCaps);
	CreateProtoService(PS_GETAVATARINFO, &CIcqProto::GetAvatarInfo);
	CreateProtoService(PS_GETMYAVATAR, &CIcqProto::GetAvatar);
	CreateProtoService(PS_SETMYAVATAR, &CIcqProto::SetAvatar);

	CreateProtoService(PS_MENU_LOADHISTORY, &CIcqProto::SvcLoadHistory);
	CreateProtoService(PS_EMPTY_SRV_HISTORY, &CIcqProto::SvcEmptyHistory);

	CreateProtoService(PS_GETUNREADEMAILCOUNT, &CIcqProto::SvcGetEmailCount);
	CreateProtoService(PS_GOTO_INBOX, &CIcqProto::SvcGotoInbox);

	// cloud file transfer
	CreateProtoService(PS_OFFLINEFILE, &CIcqProto::SvcOfflineFile);

	// events
	HookProtoEvent(ME_CLIST_GROUPCHANGE, &CIcqProto::OnGroupChange);
	HookProtoEvent(ME_GC_EVENT, &CIcqProto::GcEventHook);
	HookProtoEvent(ME_GC_BUILDMENU, &CIcqProto::GcMenuHook);
	HookProtoEvent(ME_OPT_INITIALISE, &CIcqProto::OnOptionsInit);

	// group chats
	GCREGISTER gcr = {};
	gcr.dwFlags = GC_TYPNOTIF | GC_CHANMGR | GC_DATABASE | GC_PERSISTENT;
	gcr.ptszDispName = m_tszUserName;
	gcr.pszModule = m_szModuleName;
	Chat_Register(&gcr);

	CreateProtoService(PS_LEAVECHAT, &CIcqProto::SvcLeaveChat);

	// avatars
	CreateDirectoryTreeW(GetAvatarPath());

	// netlib handle
	NETLIBUSER nlu = {};
	nlu.szSettingsModule = m_szModuleName;
	nlu.flags = NUF_OUTGOING | NUF_HTTPCONNS | NUF_UNICODE;
	nlu.szDescriptiveName.w = m_tszUserName;
	m_hNetlibUser = Netlib_RegisterUser(&nlu);

	// this was previously an old ICQ account
	ptrW wszUin(GetUIN(0));
	if (wszUin != nullptr) {
		delSetting("UIN");

		m_szOwnId = wszUin;

		for (auto &it : AccContacts())
			delSetting(it, "e-mail");
	}
	// this was previously an old MRA account
	else {
		CMStringW wszEmail(getMStringW("e-mail"));
		if (!wszEmail.IsEmpty()) {
			m_szOwnId = wszEmail;
			delSetting("e-mail");
		}
	}

	m_hWorkerThread = ForkThreadEx(&CIcqProto::ServerThread, nullptr, nullptr);
}

CIcqProto::~CIcqProto()
{
	::CloseHandle(m_evRequestsQueue);
}

////////////////////////////////////////////////////////////////////////////////////////
// OnModulesLoadedEx - performs hook registration

void CIcqProto::OnModulesLoaded()
{
	InitMenus();

	HookProtoEvent(ME_USERINFO_INITIALISE, &CIcqProto::OnUserInfoInit);

	// load custom smilies
	SmileyAdd_LoadContactSmileys(SMADD_FOLDER, m_szModuleName, GetAvatarPath() + L"\\Stickers\\*.png");
}

void CIcqProto::OnShutdown()
{
	m_bTerminated = true;
}

void CIcqProto::OnCacheInit()
{
	mir_cslock l(m_csCache);
	for (auto &it : AccContacts()) {
		m_bCacheInited = true;

		if (isChatRoom(it))
			continue;

		// that was previously an ICQ contact
		ptrW wszUin(GetUIN(it));
		if (wszUin != nullptr) {
			delSetting(it, "UIN");
			setWString(it, DB_KEY_ID, wszUin);
		}
		// that was previously a MRA contact
		else {
			CMStringW wszEmail(getMStringW(it, "e-mail"));
			if (!wszEmail.IsEmpty()) {
				delSetting(it, "e-mail");
				setWString(it, DB_KEY_ID, wszEmail);
			}
		}

		CMStringW wszId = GetUserId(it);
		auto *pUser = FindUser(wszId);
		if (pUser == nullptr) {
			pUser = new IcqUser(wszId, it);

			mir_cslock lck(m_csCache);
			m_arCache.insert(pUser);
		}
		pUser->m_iProcessedMsgId = getId(it, DB_KEY_LASTMSGID);
	}
}

void CIcqProto::OnContactAdded(MCONTACT hContact)
{
	CMStringW wszId(getMStringW(hContact, DB_KEY_ID));
	if (!wszId.IsEmpty() && !FindUser(wszId)) {
		mir_cslock l(m_csCache);
		m_arCache.insert(new IcqUser(wszId, hContact));
	}
}

bool CIcqProto::OnContactDeleted(MCONTACT hContact, uint32_t flags)
{
	if (flags & CDF_DEL_CONTACT) {
		CMStringW szId(GetUserId(hContact));
		if (!isChatRoom(hContact)) {
			mir_cslock lck(m_csCache);
			m_arCache.remove(FindUser(szId));
		}

		Push(new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "/buddylist/removeBuddy")
			<< AIMSID(this) << WCHAR_PARAM("buddy", szId) << INT_PARAM("allGroups", 1));
	}
	return true;
}

void CIcqProto::OnReceiveOfflineFile(DB::FILE_BLOB &blob)
{
	if (auto *pFileInfo = (IcqFileInfo *)blob.getUserInfo()) {
		blob.setUrl(pFileInfo->szOrigUrl);
		blob.setSize(pFileInfo->dwFileSize);
		delete pFileInfo;
	}
}

void CIcqProto::OnSendOfflineFile(DB::EventInfo &dbei, DB::FILE_BLOB &blob, void *hTransfer)
{	
	auto *ft = (IcqFileTransfer *)hTransfer;

	if (!ft->m_szMsgId.IsEmpty())
		dbei.szId = ft->m_szMsgId;

	auto *p = wcsrchr(ft->m_wszFileName, '\\');
	if (p == nullptr)
		p = ft->m_wszFileName;
	else
		p++;
	blob.setName(p);

	blob.setUrl(ft->m_szHost);
	blob.complete(ft->pfts.currentFileSize);
	blob.setLocalName(ft->m_wszFileName);
}

void CIcqProto::OnEventEdited(MCONTACT, MEVENT, const DBEVENTINFO &)
{

}

/////////////////////////////////////////////////////////////////////////////////////////
// batch events deletion from the server

void CIcqProto::OnBatchDeleteMsg(MHttpResponse *pReply, AsyncHttpRequest *pReq)
{
	RobustReply root(pReply);
	if (root.error() != 20000)
		return;

	if (auto *pUser = FindUser(GetUserId(pReq->hContact)))
		pUser->m_bSkipPatch = true;
	RetrievePatches(pReq->hContact);
}

void CIcqProto::BatchDeleteMsg()
{
	JSONNode ids(JSON_ARRAY); ids.set_name("msgIds");

	mir_cslock lck(m_csDeleteQueue);
	for (auto &it : m_arDeleteQueue) {
		ids.push_back(JSONNode("", _atoi64(it)));
		mir_free(it);
	}

	auto *pReq = new AsyncRapiRequest(this, "delMsgBatch", &CIcqProto::OnBatchDeleteMsg);
	pReq->hContact = m_hDeleteContact;
	pReq->params << WCHAR_PARAM("sn", GetUserId(m_hDeleteContact)) << BOOL_PARAM("silent", true) << BOOL_PARAM("shared", m_bRemoveForAll) << ids;
	Push(pReq);

	m_arDeleteQueue.destroy();
	m_hDeleteContact = INVALID_CONTACT_ID;
}

void CIcqProto::OnEventDeleted(MCONTACT hContact, MEVENT hEvent, int flags)
{
	// the command arrived from the server, don't send it back then
	if (!(flags & CDF_DEL_HISTORY))
		return;

	if (m_hDeleteContact != INVALID_CONTACT_ID)
		if (m_hDeleteContact != hContact)
			BatchDeleteMsg();

	DB::EventInfo dbei(hEvent, false);
	if (!dbei || !dbei.szId)
		return;

	mir_cslock lck(m_csDeleteQueue);
	m_hDeleteContact = hContact;
	m_bRemoveForAll = (flags & CDF_FOR_EVERYONE) != 0;
	m_arDeleteQueue.insert(mir_strdup(dbei.szId));
	m_impl.m_delete.Start(100);
}

/////////////////////////////////////////////////////////////////////////////////////////

static void __cdecl DownloadCallack(size_t iProgress, void *pParam)
{
	auto *ofd = (OFDTHREAD *)pParam;

	DBVARIANT dbv = { DBVT_DWORD };
	dbv.dVal = unsigned(iProgress);
	db_event_setJson(ofd->hDbEvent, "ft", &dbv);
}

void __cdecl CIcqProto::OfflineFileThread(void *pParam)
{
	auto *ofd = (OFDTHREAD *)pParam;

	DB::EventInfo dbei(ofd->hDbEvent);
	if (m_bOnline && dbei && !strcmp(dbei.szModule, m_szModuleName) && dbei.eventType == EVENTTYPE_FILE) {
		DB::FILE_BLOB blob(dbei);

		CMStringW wszUrl;
		if (fileText2url(blob.getUrl(), &wszUrl)) {
			if (auto *pFileInfo = RetrieveFileInfo(dbei.hContact, wszUrl)) {
				if (!ofd->bCopy) {
					MHttpRequest nlhr(REQUEST_GET);
					nlhr.m_szUrl = pFileInfo->szUrl;
					nlhr.AddHeader("Sec-Fetch-User", "?1");
					nlhr.AddHeader("Sec-Fetch-Site", "cross-site");
					nlhr.AddHeader("Sec-Fetch-Mode", "navigate");
					nlhr.AddHeader("Accept-Encoding", "gzip");

					debugLogW(L"Saving to [%s]", ofd->wszPath.c_str());
					NLHR_PTR reply(Netlib_DownloadFile(m_hNetlibUser, &nlhr, ofd->wszPath, DownloadCallack, ofd));
					if (reply && reply->resultCode == 200) {
						struct _stat st;
						_wstat(ofd->wszPath, &st);

						DBVARIANT dbv = { DBVT_DWORD };
						dbv.dVal = st.st_size;
						db_event_setJson(ofd->hDbEvent, "ft", &dbv);

						ofd->Finish();
					}				
				}
				else {
					ofd->wszPath.Empty();
					ofd->wszPath.Append(_A2T(pFileInfo->szUrl));
					ofd->pCallback->Invoke(*ofd);
				}
			}
		}
	}

	delete ofd;
}

INT_PTR __cdecl CIcqProto::SvcOfflineFile(WPARAM param, LPARAM)
{
	ForkThread((MyThreadFunc)&CIcqProto::OfflineFileThread, (void *)param);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CIcqProto::SvcLoadHistory(WPARAM hContact, LPARAM)
{
	delSetting(hContact, DB_KEY_LASTMSGID);

	RetrieveUserHistory(hContact, 1, true);
	return 0;
}

INT_PTR CIcqProto::SvcEmptyHistory(WPARAM hContact, LPARAM)
{
	auto *pReq = new AsyncRapiRequest(this, "delHistory");
	#ifndef _DEBUG
	pReq->flags |= NLHRF_NODUMPSEND;
	#endif
	pReq->hContact = hContact;
	pReq->params << WCHAR_PARAM("sn", GetUserId(hContact)) << INT64_PARAM("uptoMsgId", getId(hContact, DB_KEY_LASTMSGID));
	Push(pReq);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

void CIcqProto::OnBuildProtoMenu()
{
	CMenuItem mi(&g_plugin);
	mi.root = Menu_GetProtocolRoot(this);
	mi.flags = CMIF_UNMOVABLE;

	// Groups uploader
	mi.pszService = "/UploadGroups";
	CreateProtoService(mi.pszService, &CIcqProto::UploadGroups);
	mi.name.a = LPGEN("Synchronize server groups");
	mi.position = 200001;
	mi.hIcolibItem = Skin_GetIconHandle(SKINICON_OTHER_GROUP);
	m_hUploadGroups = Menu_AddProtoMenuItem(&mi, m_szModuleName);

	Menu_ShowItem(m_hUploadGroups, false);

	// Groups editor
	mi.pszService = "/EditGroups";
	CreateProtoService(mi.pszService, &CIcqProto::EditGroups);
	mi.name.a = LPGEN("Edit server groups");
	mi.position = 200002;
	mi.hIcolibItem = Skin_GetIconHandle(SKINICON_OTHER_GROUP);
	Menu_AddProtoMenuItem(&mi, m_szModuleName);
}

/////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CIcqProto::SvcGetEmailCount(WPARAM, LPARAM)
{
	if (!m_bOnline)
		return 0;
	return m_unreadEmails;
}

INT_PTR CIcqProto::SvcGotoInbox(WPARAM, LPARAM)
{
	Utils_OpenUrl("https://e.mail.ru/messages/inbox");
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// mark events read at the server

void CIcqProto::SendMarkRead()
{
	mir_cslock lck(m_csMarkReadQueue);
	while (m_arMarkReadQueue.getCount()) {
		auto *pUser = m_arMarkReadQueue[0];

		auto *pReq = new AsyncRapiRequest(this, "setDlgStateWim");
		pReq->params << WCHAR_PARAM("sn", GetUserId(pUser->m_hContact)) << INT64_PARAM("lastRead", getId(pUser->m_hContact, DB_KEY_LASTMSGID));
		Push(pReq);

		m_arMarkReadQueue.remove(0);
	}
}

void CIcqProto::OnMarkRead(MCONTACT hContact, MEVENT)
{
	if (!m_bOnline)
		return;

	m_impl.m_markRead.Start(200);

	auto *pUser = FindUser(GetUserId(hContact));
	if (pUser) {
		mir_cslock lck(m_csMarkReadQueue);
		if (m_arMarkReadQueue.indexOf(pUser) == -1)
			m_arMarkReadQueue.insert(pUser);
	}
}

int CIcqProto::OnGroupChange(WPARAM hContact, LPARAM lParam)
{
	if (!m_bOnline)
		return 0;

	CLISTGROUPCHANGE *pParam = (CLISTGROUPCHANGE*)lParam;
	if (hContact == 0) { // whole group is changed
		if (pParam->pszOldName == nullptr) {
			for (auto &it : m_arGroups)
				if (it->wszName == pParam->pszNewName)
					return 0;

			Push(new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "/buddylist/addGroup")
				<< AIMSID(this) << GROUP_PARAM("group", pParam->pszNewName));
		}
		else if (pParam->pszNewName == nullptr) {
			Push(new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "/buddylist/removeGroup") 
				<< AIMSID(this) << GROUP_PARAM("group", pParam->pszOldName));
		}
		else {
			Push(new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "/buddylist/renameGroup") 
				<< AIMSID(this) << GROUP_PARAM("oldGroup", pParam->pszOldName) << GROUP_PARAM("newGroup", pParam->pszNewName));
		}
	}
	else MoveContactToGroup(hContact, ptrW(getWStringA(hContact, "IcqGroup")), pParam->pszNewName);
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// PS_AddToList - adds a contact to the contact list

MCONTACT CIcqProto::AddToList(int, PROTOSEARCHRESULT *psr)
{
	if (mir_wstrlen(psr->id.w) == 0)
		return 0;

	MCONTACT hContact = CreateContact(psr->id.w, true);
	if (psr->nick.w)
		setWString(hContact, "Nick", psr->nick.w);
	if (psr->firstName.w)
		setWString(hContact, "FirstName", psr->firstName.w);
	if (psr->lastName.w)
		setWString(hContact, "LastName", psr->lastName.w);

	return hContact;
}

////////////////////////////////////////////////////////////////////////////////////////
// PSR_AUTH

int CIcqProto::AuthRecv(MCONTACT, DB::EventInfo &dbei)
{
	return Proto_AuthRecv(m_szModuleName, dbei);
}

////////////////////////////////////////////////////////////////////////////////////////
// PSS_AUTHREQUEST

int CIcqProto::AuthRequest(MCONTACT hContact, const wchar_t* szMessage)
{
	ptrW wszGroup(Clist_GetGroup(hContact));
	if (!wszGroup)
		wszGroup = mir_wstrdup(L"General");

	auto *pReq = new AsyncHttpRequest(CONN_MAIN, REQUEST_POST, "/buddylist/addBuddy", &CIcqProto::OnAddBuddy);
	pReq << AIMSID(this) << WCHAR_PARAM("authorizationMsg", szMessage) << WCHAR_PARAM("buddy", GetUserId(hContact)) << WCHAR_PARAM("group", wszGroup) << INT_PARAM("preAuthorized", 1);
	pReq->hContact = hContact;
	Push(pReq);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// GetCaps - return protocol capabilities bits

INT_PTR CIcqProto::GetCaps(int type, MCONTACT)
{
	INT_PTR nReturn = 0;

	switch (type) {
	case PFLAGNUM_1:
		nReturn = PF1_IM | PF1_AUTHREQ | PF1_BASICSEARCH | PF1_ADDSEARCHRES | /*PF1_SEARCHBYNAME | TODO */
			PF1_FILE | PF1_CONTACT | PF1_SERVERCLIST;
		break;

	case PFLAGNUM_2:
	case PFLAGNUM_3:
		return PF2_ONLINE | PF2_SHORTAWAY | PF2_LONGAWAY | PF2_LIGHTDND | PF2_HEAVYDND | PF2_INVISIBLE;
	
	case PFLAGNUM_5:
		return PF2_SHORTAWAY | PF2_LONGAWAY | PF2_LIGHTDND | PF2_HEAVYDND;

	case PFLAGNUM_4:
		nReturn = PF4_FORCEAUTH | PF4_SUPPORTIDLE | PF4_OFFLINEFILES | PF4_IMSENDOFFLINE | PF4_SUPPORTTYPING |
			PF4_AVATARS | PF4_SERVERMSGID | PF4_READNOTIFY | PF4_REPLY;
		break;

	case PFLAG_UNIQUEIDTEXT:
		return (INT_PTR)TranslateT("E-mail");
	}

	return nReturn;
}

////////////////////////////////////////////////////////////////////////////////////////
// GetInfo - retrieves a contact info

int CIcqProto::GetInfo(MCONTACT hContact, int)
{
	RetrieveUserInfo(hContact);
	RetrievePresence(hContact);

	if (Contact::IsGroupChat(hContact))
		RetrieveChatInfo(hContact);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// SearchBasic - searches the contact by UID

HANDLE CIcqProto::SearchBasic(const wchar_t *pszSearch)
{
	if (!m_bOnline)
		return nullptr;

	auto *pReq = new AsyncRapiRequest(this, "search", &CIcqProto::OnSearchResults);
	pReq->params << WCHAR_PARAM(*pszSearch == '+' ? "phonenum" : "keyword", pszSearch);
	Push(pReq);

	return pReq;
}

////////////////////////////////////////////////////////////////////////////////////////
// SendFile - sends a file

HANDLE CIcqProto::SendFile(MCONTACT hContact, const wchar_t *szDescription, wchar_t **ppszFiles)
{
	IcqFileTransfer *pTransfer = nullptr;

	for (int i = 0; ppszFiles[i] != 0; i++) {
		auto *pwszFileName = ppszFiles[i];
		struct _stat statbuf;
		if (_wstat(pwszFileName, &statbuf)) {
			debugLogW(L"'%s' is an invalid filename", pwszFileName);
			continue;
		}

		int iFileId = _wopen(pwszFileName, _O_RDONLY | _O_BINARY, _S_IREAD);
		if (iFileId < 0)
			continue;

		pTransfer = new IcqFileTransfer(hContact, pwszFileName);
		pTransfer->pfts.totalFiles = 1;
		pTransfer->pfts.currentFileSize = pTransfer->pfts.totalBytes = statbuf.st_size;
		pTransfer->m_fileId = iFileId;
		if (mir_wstrlen(szDescription))
			pTransfer->m_wszDescr = szDescription;

		auto *pReq = new AsyncHttpRequest(CONN_NONE, REQUEST_GET, "https://files.icq.com/files/init", &CIcqProto::OnFileInit);
		pReq << CHAR_PARAM("a", m_szAToken) << CHAR_PARAM("client", "icq") << CHAR_PARAM("f", "json") << WCHAR_PARAM("filename", pTransfer->m_wszShortName)
			<< CHAR_PARAM("k", APP_ID) << INT_PARAM("size", statbuf.st_size) << INT_PARAM("ts", TS());
		CalcHash(pReq);
		pReq->pUserInfo = pTransfer;
		Push(pReq);
	}

	return pTransfer; // Success, if at least one file was sent
}

////////////////////////////////////////////////////////////////////////////////////////
// PS_SendMessage - sends a message

int CIcqProto::SendMsg(MCONTACT hContact, MEVENT hReplyEvent, const char *pszSrc)
{
	JSONNode parts(JSON_ARRAY);
	if (hReplyEvent) {
		DB::EventInfo dbei(hReplyEvent);
		if (dbei) {
			JSONNode replyTo;
			CMStringA replyId(GetUserId(dbei.hContact));
			replyTo << CHAR_PARAM("mediaType", "quote") << CHAR_PARAM("sn", replyId) << INT_PARAM("time", dbei.timestamp)
				<< CHAR_PARAM("msgId", dbei.szId) << WCHAR_PARAM("friendly", Clist_GetContactDisplayName(dbei.hContact, 0))
				<< WCHAR_PARAM("text", ptrW(dbei.getText()));
			parts.push_back(replyTo);
		}
	}

	JSONNode msgText; msgText << CHAR_PARAM("mediaType", "text") << CHAR_PARAM("text", pszSrc);
	parts.push_back(msgText);

	int id = InterlockedIncrement(&m_msgId);
	auto *pOwn = new IcqOwnMessage(hContact, id, pszSrc);
	{
		mir_cslock lck(m_csOwnIds);
		m_arOwnIds.insert(pOwn);
	}

	SendMessageParts(hContact, parts, pOwn);
	return id;
}

////////////////////////////////////////////////////////////////////////////////////////
// PS_SetStatus - sets the protocol status

int CIcqProto::SetStatus(int iNewStatus)
{
	debugLogA("CIcqProto::SetStatus iNewStatus = %d, m_iStatus = %d, m_iDesiredStatus = %d m_hWorkerThread = %p", iNewStatus, m_iStatus, m_iDesiredStatus, m_hWorkerThread);

	switch (iNewStatus) {
	case ID_STATUS_OFFLINE:
	case ID_STATUS_ONLINE:
	case ID_STATUS_INVISIBLE:
		break;

	default:
		iNewStatus = ID_STATUS_ONLINE;
	}

	if (iNewStatus == m_iStatus)
		return 0;

	int iOldStatus = m_iStatus;

	// go offline
	if (iNewStatus == ID_STATUS_OFFLINE) {
		if (m_bOnline)
			SetServerStatus(ID_STATUS_OFFLINE);

		m_iStatus = m_iDesiredStatus = ID_STATUS_OFFLINE;
		setAllContactStatuses(ID_STATUS_OFFLINE, false);

		ProtoBroadcastAck(0, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)iOldStatus, m_iStatus);
		return 0;
	}

	m_iDesiredStatus = iNewStatus;

	// not logged in? come on
	if (!m_bOnline && !IsStatusConnecting(m_iStatus)) {
		m_iStatus = ID_STATUS_CONNECTING;
		ProtoBroadcastAck(0, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)iOldStatus, m_iStatus);

		if (mir_wstrlen(m_szOwnId) == 0) {
			debugLogA("Thread ended, UIN/password are not configured");
			ConnectionFailed(LOGINERR_BADUSERID);
			return 0;
		}

		if (!RetrievePassword()) {
			debugLogA("Thread ended, password is not configured");
			ConnectionFailed(LOGINERR_BADUSERID);
			return 0;
		}

		CheckPassword();
	}
	else if (m_bOnline) {
		debugLogA("setting server online status to %d", iNewStatus);
		SetServerStatus(iNewStatus);
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// PS_UserIsTyping - sends a UTN notification

int CIcqProto::UserIsTyping(MCONTACT hContact, int type)
{
	Push(new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "/im/setTyping")
		<< AIMSID(this) << WCHAR_PARAM("t", GetUserId(hContact)) << CHAR_PARAM("typingStatus", (type == PROTOTYPE_SELFTYPING_ON) ? "typing" : "typed"));
	return 0;
}
