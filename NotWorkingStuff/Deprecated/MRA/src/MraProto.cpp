#include "stdafx.h"

static int MraExtraIconsApplyAll(WPARAM, LPARAM)
{
	for (auto &it : CMPlugin::g_arInstances)
		it->MraExtraIconsApply(0, 0);
	return 0;
}

CMraProto::CMraProto(const char* _module, const wchar_t* _displayName) :
	PROTO<CMraProto>(_module, _displayName),
	m_bLoggedIn(false),
	m_groups(5, NumericKeySortT)
{
	MraSendQueueInitialize(0, &hSendQueueHandle);
	MraFilesQueueInitialize(0, &hFilesQueueHandle);
	MraMPopSessionQueueInitialize(&hMPopSessionQueue);//getByte("AutoAuthOnWebServices", MRA_DEFAULT_AUTO_AUTH_ON_WEB_SVCS)
	MraAvatarsQueueInitialize(&hAvatarsQueueHandle);

	CreateProtoService(PS_SETCUSTOMSTATUSEX, &CMraProto::MraSetXStatusEx);
	CreateProtoService(PS_GETCUSTOMSTATUSEX, &CMraProto::MraGetXStatusEx);
	CreateProtoService(PS_GETCUSTOMSTATUSICON, &CMraProto::MraGetXStatusIcon);

	CreateProtoService(PS_SET_LISTENINGTO, &CMraProto::MraSetListeningTo);

	CreateProtoService(PS_CREATEACCMGRUI, &CMraProto::MraCreateAccMgrUI);
	CreateProtoService(PS_GETAVATARCAPS, &CMraProto::MraGetAvatarCaps);
	CreateProtoService(PS_GETAVATARINFO, &CMraProto::MraGetAvatarInfo);
	CreateProtoService(PS_GETMYAVATAR, &CMraProto::MraGetMyAvatar);

	CreateProtoService(MS_ICQ_SENDSMS, &CMraProto::MraSendSMS);
	CreateProtoService(PS_SEND_NUDGE, &CMraProto::MraSendNudge);
	CreateProtoService(PS_GETUNREADEMAILCOUNT, &CMraProto::GetUnreadEmailCount);

	HookProtoEvent(ME_OPT_INITIALISE, &CMraProto::OnOptionsInit);
	HookProtoEvent(ME_DB_CONTACT_DELETED, &CMraProto::MraContactDeleted);
	HookProtoEvent(ME_DB_CONTACT_SETTINGCHANGED, &CMraProto::MraDbSettingChanged);

	m_heNudgeReceived = CreateProtoEvent(PE_NUDGE);

	wchar_t name[MAX_PATH];
	mir_snwprintf(name, TranslateT("%s connection"), m_tszUserName);

	NETLIBUSER nlu = {};
	nlu.flags = NUF_INCOMING | NUF_OUTGOING | NUF_HTTPCONNS | NUF_UNICODE;
	nlu.szSettingsModule = m_szModuleName;
	nlu.szDescriptiveName.w = name;
	m_hNetlibUser = Netlib_RegisterUser(&nlu);

	InitMenus();

	mir_snprintf(szNewMailSound, "%s_new_email", m_szModuleName);
	g_plugin.addSound(szNewMailSound, m_tszUserName, MRA_SOUND_NEW_EMAIL);

	HookProtoEvent(ME_CLIST_PREBUILDSTATUSMENU, &CMraProto::MraRebuildStatusMenu);

	hExtraXstatusIcon = ExtraIcon_RegisterIcolib("MRAXstatus", LPGEN("Mail.ru xStatus"), "mra_xstatus25");
	hExtraInfo = ExtraIcon_RegisterIcolib("MRAStatus", LPGEN("Mail.ru extra info"), MRA_XSTATUS_UNKNOWN_STR);

	m_bHideXStatusUI = false;
	m_iXStatus = getByte(DBSETTING_XSTATUSID, MRA_MIR_XSTATUS_NONE);
	if (!IsXStatusValid(m_iXStatus))
		m_iXStatus = MRA_MIR_XSTATUS_NONE;
}

CMraProto::~CMraProto()
{
	Netlib_CloseHandle(m_hNetlibUser);

	DestroyHookableEvent(m_heNudgeReceived);

	MraAvatarsQueueDestroy(hAvatarsQueueHandle);
	MraMPopSessionQueueDestroy(hMPopSessionQueue);
	MraFilesQueueDestroy(hFilesQueueHandle);
	MraSendQueueDestroy(hSendQueueHandle);
}

INT_PTR CMraProto::MraCreateAccMgrUI(WPARAM, LPARAM lParam)
{
	return (INT_PTR)CreateDialogParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_MRAACCOUNT),
		(HWND)lParam, DlgProcAccount, LPARAM(this));
}

void CMraProto::OnModulesLoaded()
{
	HookProtoEvent(ME_CLIST_EXTRA_IMAGE_APPLY, &CMraProto::MraExtraIconsApply);
	HookProtoEvent(ME_CLIST_PREBUILDCONTACTMENU, &CMraProto::MraRebuildContactMenu);
	HookProtoEvent(ME_WAT_NEWSTATUS, &CMraProto::MraMusicChanged);
	HookProtoEvent(ME_CLIST_GROUPCHANGE, &CMraProto::OnGroupChanged);

	// всех в offline // тк unsaved values сохран¤ютс¤ их нужно инициализировать
	for (auto &hContact : AccContacts())
		SetContactBasicInfoW(hContact, SCBIFSI_LOCK_CHANGES_EVENTS, (SCBIF_ID | SCBIF_GROUP_ID | SCBIF_SERVER_FLAG | SCBIF_STATUS), -1, -1, 0, 0, ID_STATUS_OFFLINE, nullptr, nullptr, nullptr);

	// unsaved values
	db_set_resident(m_szModuleName, "LogonTS");
	db_set_resident(m_szModuleName, "ContactID");
	db_set_resident(m_szModuleName, "GroupID");
	db_set_resident(m_szModuleName, "ContactFlags");
	db_set_resident(m_szModuleName, "ContactServerFlags");
	db_set_resident(m_szModuleName, "HooksLocked");
	db_set_resident(m_szModuleName, DBSETTING_CAPABILITIES);
	db_set_resident(m_szModuleName, DBSETTING_XSTATUSNAME);
	db_set_resident(m_szModuleName, DBSETTING_XSTATUSMSG);
	db_set_resident(m_szModuleName, DBSETTING_BLOGSTATUSTIME);
	db_set_resident(m_szModuleName, DBSETTING_BLOGSTATUSID);
	db_set_resident(m_szModuleName, DBSETTING_BLOGSTATUS);
	db_set_resident(m_szModuleName, DBSETTING_BLOGSTATUSMUSIC);

	// destroy all chat sessions
	bChatExists = MraChatRegister();
}

void CMraProto::OnShutdown()
{
	m_bShutdown = true;
	SetStatus(ID_STATUS_OFFLINE);

	if (hAvatarsQueueHandle != nullptr)
		MraAvatarsQueueSuspend(hAvatarsQueueHandle);
}

/////////////////////////////////////////////////////////////////////////////////////////

MCONTACT CMraProto::AddToListByEmail(LPCTSTR plpsEMail, LPCTSTR plpsNick, LPCTSTR plpsFirstName, LPCTSTR plpsLastName, DWORD dwFlags)
{
	if (!plpsEMail)
		return NULL;

	BOOL bAdded;
	MCONTACT hContact = MraHContactFromEmail(plpsEMail, TRUE, TRUE, &bAdded);
	if (hContact == NULL)
		return NULL;

	if (plpsNick)
		mraSetStringW(hContact, "Nick", plpsNick);
	if (plpsFirstName)
		mraSetStringW(hContact, "FirstName", plpsFirstName);
	if (plpsLastName)
		mraSetStringW(hContact, "LastName", plpsLastName);

	if (dwFlags & PALF_TEMPORARY)
		db_set_b(hContact, "CList", "Hidden", 1);
	else
		db_unset(hContact, "CList", "NotOnList");

	if (bAdded)
		MraUpdateContactInfo(hContact);
	return hContact;
}

MCONTACT CMraProto::AddToList(int flags, PROTOSEARCHRESULT *psr)
{
	if (psr->cbSize != sizeof(PROTOSEARCHRESULT))
		return 0;

	return AddToListByEmail(psr->email.w, psr->nick.w, psr->firstName.w, psr->lastName.w, flags);
}

MCONTACT CMraProto::AddToListByEvent(int, int, MEVENT hDbEvent)
{
	DBEVENTINFO dbei = {};
	if ((dbei.cbBlob = db_event_getBlobSize(hDbEvent)) != -1) {
		dbei.pBlob = (PBYTE)alloca(dbei.cbBlob);
		if (db_event_get(hDbEvent, &dbei) == 0 &&
			 !mir_strcmp(dbei.szModule, m_szModuleName) &&
			 (dbei.eventType == EVENTTYPE_AUTHREQUEST || dbei.eventType == EVENTTYPE_CONTACTS))
		{
			DB_AUTH_BLOB blob(dbei.pBlob);
			return AddToListByEmail(dbei.getString(blob.get_email()), dbei.getString(blob.get_nick()), dbei.getString(blob.get_firstName()), dbei.getString(blob.get_lastName()), 0);
		}
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

int CMraProto::Authorize(MEVENT hDBEvent)
{
	if (!m_bLoggedIn)	return 1;

	DBEVENTINFO dbei = {};
	if ((dbei.cbBlob = db_event_getBlobSize(hDBEvent)) == -1)
		return 1;

	dbei.pBlob = (PBYTE)alloca(dbei.cbBlob);
	if (db_event_get(hDBEvent, &dbei))             return 1;
	if (dbei.eventType != EVENTTYPE_AUTHREQUEST)   return 1;
	if (mir_strcmp(dbei.szModule, m_szModuleName)) return 1;

	DB_AUTH_BLOB blob(dbei.pBlob);
	MraAuthorize(blob.get_email());
	return 0;
}

int CMraProto::AuthDeny(MEVENT hDBEvent, const wchar_t* szReason)
{
	if (!m_bLoggedIn) return 1;

	DBEVENTINFO dbei = {};
	if ((dbei.cbBlob = db_event_getBlobSize(hDBEvent)) == -1)
		return 1;

	dbei.pBlob = (PBYTE)alloca(dbei.cbBlob);
	if (db_event_get(hDBEvent, &dbei))             return 1;
	if (dbei.eventType != EVENTTYPE_AUTHREQUEST)   return 1;
	if (mir_strcmp(dbei.szModule, m_szModuleName)) return 1;

	DB_AUTH_BLOB blob(dbei.pBlob);
	MraMessage(FALSE, NULL, 0, 0, blob.get_email(), szReason, nullptr, 0);
	return 0;
}

int CMraProto::AuthRecv(MCONTACT, PROTORECVEVENT* pre)
{
	return Proto_AuthRecv(m_szModuleName, pre);
}

/////////////////////////////////////////////////////////////////////////////////////////

HANDLE CMraProto::FileAllow(MCONTACT, HANDLE hTransfer, const wchar_t *szPath)
{
	if (szPath != nullptr)
		if (MraFilesQueueAccept(hFilesQueueHandle, (DWORD_PTR)hTransfer, szPath, mir_wstrlen(szPath)) == NO_ERROR)
			return hTransfer; // Success

	return nullptr;
}

int CMraProto::FileCancel(MCONTACT hContact, HANDLE hTransfer)
{
	if (hContact && hTransfer) {
		MraFilesQueueCancel(hFilesQueueHandle, (DWORD_PTR)hTransfer, TRUE);
		return 0; // Success
	}

	return 1;
}

int CMraProto::FileDeny(MCONTACT hContact, HANDLE hTransfer, const wchar_t*)
{
	return FileCancel(hContact, hTransfer);
}

/////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CMraProto::GetCaps(int type, MCONTACT)
{
	switch (type) {
	case PFLAGNUM_1:
		return PF1_IM | PF1_FILE | PF1_MODEMSG | PF1_SERVERCLIST | PF1_AUTHREQ | PF1_ADDED | PF1_VISLIST | PF1_INVISLIST |
			PF1_INDIVSTATUS | PF1_PEER2PEER | PF1_CHAT | PF1_BASICSEARCH | PF1_EXTSEARCH | PF1_CANRENAMEFILE | PF1_FILERESUME |
			PF1_ADDSEARCHRES | PF1_CONTACT | PF1_SEARCHBYEMAIL | PF1_USERIDISEMAIL | PF1_SEARCHBYNAME | PF1_EXTSEARCHUI;

	case PFLAGNUM_2:
		return PF2_ONLINE | PF2_INVISIBLE | PF2_SHORTAWAY | PF2_HEAVYDND | PF2_FREECHAT | PF2_ONTHEPHONE;

	case PFLAGNUM_3:
		return PF2_ONLINE | PF2_INVISIBLE | PF2_SHORTAWAY | PF2_HEAVYDND | PF2_FREECHAT | PF2_ONTHEPHONE;

	case PFLAGNUM_4:
		return PF4_FORCEAUTH | PF4_FORCEADDED | PF4_SUPPORTTYPING | PF4_AVATARS;

	case PFLAGNUM_5:
		return PF2_ONTHEPHONE;

	case PFLAG_UNIQUEIDTEXT:
		return (INT_PTR)Translate("E-mail address");

	case PFLAG_MAXCONTACTSPERPACKET:
		return MRA_MAXCONTACTSPERPACKET;

	case PFLAG_MAXLENOFMESSAGE:
		return MRA_MAXLENOFMESSAGE;

	default:
		return 0;
	}
}

int CMraProto::GetInfo(MCONTACT hContact, int)
{
	return MraUpdateContactInfo(hContact) != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

HANDLE CMraProto::SearchBasic(const wchar_t *id)
{
	return SearchByEmail(id);
}

HANDLE CMraProto::SearchByEmail(const wchar_t *email)
{
	if (m_bLoggedIn && email) {
		CMStringA szEmail(email);
		return MraWPRequestByEMail(NULL, ACKTYPE_SEARCH, szEmail);
	}

	return nullptr;
}

HANDLE CMraProto::SearchByName(const wchar_t *pszNick, const wchar_t *pszFirstName, const wchar_t *pszLastName)
{
	if (m_bLoggedIn && (*pszNick || *pszFirstName || *pszLastName)) {
		DWORD dwRequestFlags = 0;
		if (*pszNick)      SetBit(dwRequestFlags, MRIM_CS_WP_REQUEST_PARAM_NICKNAME);
		if (*pszFirstName) SetBit(dwRequestFlags, MRIM_CS_WP_REQUEST_PARAM_FIRSTNAME);
		if (*pszLastName)  SetBit(dwRequestFlags, MRIM_CS_WP_REQUEST_PARAM_LASTNAME);
		return MraWPRequestW(NULL, ACKTYPE_SEARCH, dwRequestFlags, "", "", pszNick, pszFirstName, pszLastName, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}

	return nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////

int CMraProto::RecvContacts(MCONTACT hContact, PROTORECVEVENT* pre)
{
	DBEVENTINFO dbei = {};
	dbei.szModule = m_szModuleName;
	dbei.timestamp = pre->timestamp;
	dbei.flags = (pre->flags & PREF_CREATEREAD) ? DBEF_READ : 0;
	dbei.eventType = EVENTTYPE_CONTACTS;
	dbei.cbBlob = pre->lParam;
	dbei.pBlob = (PBYTE)pre->szMessage;
	db_event_add(hContact, &dbei);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

int CMraProto::SendContacts(MCONTACT hContact, int, int nContacts, MCONTACT *hContactsList)
{
	INT_PTR iRet = 0;

	if (m_bLoggedIn && hContact) {
		BOOL bSlowSend;
		CMStringW wszData, wszEmail;
		CMStringA szEmail;
		if (mraGetStringA(hContact, "e-mail", szEmail)) {
			for (int i = 0; i < nContacts; i++) {
				if (IsContactMra(hContactsList[i]))
					if (mraGetStringW(hContactsList[i], "e-mail", wszEmail))
						wszData += wszEmail + ';' + Clist_GetContactDisplayName(hContactsList[i]) + ';';
			}

			bSlowSend = getByte("SlowSend", MRA_DEFAULT_SLOW_SEND);
			iRet = MraMessage(bSlowSend, hContact, ACKTYPE_CONTACTS, MESSAGE_FLAG_CONTACT, szEmail, wszData, nullptr, 0);
			if (bSlowSend == FALSE)
				ProtoBroadcastAck(hContact, ACKTYPE_CONTACTS, ACKRESULT_SUCCESS, (HANDLE)iRet, 0);
		}
	}
	else ProtoBroadcastAck(hContact, ACKTYPE_CONTACTS, ACKRESULT_FAILED, nullptr, (LPARAM)"You cannot send when you are offline.");

	return iRet;
}

HANDLE CMraProto::SendFile(MCONTACT hContact, const wchar_t*, wchar_t **ppszFiles)
{
	if (!m_bLoggedIn || !hContact || !ppszFiles)
		return nullptr;

	size_t dwFilesCount;
	for (dwFilesCount = 0; ppszFiles[dwFilesCount]; dwFilesCount++);

	DWORD iRet = 0;
	MraFilesQueueAddSend(hFilesQueueHandle, 0, hContact, ppszFiles, dwFilesCount, &iRet);
	return (HANDLE)iRet;
}

int CMraProto::SendMsg(MCONTACT hContact, int, const char *lpszMessage)
{
	if (!m_bLoggedIn) {
		ProtoBroadcastAck(hContact, ACKTYPE_MESSAGE, ACKRESULT_FAILED, nullptr, (LPARAM)TranslateT("You cannot send when you are offline."));
		return 0;
	}

	DWORD dwFlags = 0;
	CMStringW wszMessage(ptrW(mir_utf8decodeW(lpszMessage)));
	if (wszMessage.IsEmpty()) {
		ProtoBroadcastAck(hContact, ACKTYPE_MESSAGE, ACKRESULT_FAILED, nullptr, (LPARAM)TranslateT("Can't allocate buffer for convert to Unicode."));
		return 0;
	}

	CMStringA szEmail;
	if (!mraGetStringA(hContact, "e-mail", szEmail))
		return 0;
		
	BOOL bSlowSend = getByte("SlowSend", MRA_DEFAULT_SLOW_SEND);
	if (getByte("RTFSendEnable", MRA_DEFAULT_RTF_SEND_ENABLE) && (MraContactCapabilitiesGet(hContact) & FEATURE_FLAG_RTF_MESSAGE))
		dwFlags |= MESSAGE_FLAG_RTF;

	int iRet = MraMessage(bSlowSend, hContact, ACKTYPE_MESSAGE, dwFlags, szEmail, wszMessage, NULL, 0);
	if (bSlowSend == FALSE)
		ProtoBroadcastAckAsync(hContact, ACKTYPE_MESSAGE, ACKRESULT_SUCCESS, (HANDLE)iRet, 0);
	return iRet;
}

/////////////////////////////////////////////////////////////////////////////////////////

int CMraProto::SetApparentMode(MCONTACT hContact, int mode)
{
	if (!m_bLoggedIn || !hContact)
		return 1;

	// Only 3 modes are supported
	if (hContact && (mode == 0 || mode == ID_STATUS_ONLINE || mode == ID_STATUS_OFFLINE)) {
		int dwOldMode = (int)getDword(hContact, "ApparentMode", 0);

		// Dont send redundant updates
		if (mode != dwOldMode) {
			DWORD dwContactFlag = 0;

			switch (mode) {
			case ID_STATUS_OFFLINE:
				dwContactFlag |= CONTACT_FLAG_INVISIBLE;
				break;
			case ID_STATUS_ONLINE:
				dwContactFlag |= CONTACT_FLAG_VISIBLE;
				break;
			}

			if (MraModifyContact(hContact, nullptr, &dwContactFlag)) {
				SetContactBasicInfoW(hContact, 0, SCBIF_FLAG, 0, 0, dwContactFlag, 0, 0, nullptr, nullptr, nullptr);
				return 0; // Success
			}
		}
	}

	return 1;
}

int CMraProto::SetStatus(int iNewStatus)
{
	// remap global statuses to local supported
	switch (iNewStatus) {
	case ID_STATUS_OCCUPIED:
		iNewStatus = ID_STATUS_DND;
		break;
	case ID_STATUS_NA:
	case ID_STATUS_ONTHEPHONE:
	case ID_STATUS_OUTTOLUNCH:
		iNewStatus = ID_STATUS_AWAY;
		break;
	}

	// nothing to change
	if (m_iStatus == iNewStatus)
		return 0;

	DWORD dwOldStatusMode;

	//set all contacts to offline
	if ((m_iDesiredStatus = iNewStatus) == ID_STATUS_OFFLINE) {
		m_bLoggedIn = FALSE;
		dwOldStatusMode = InterlockedExchange((volatile LONG*)&m_iStatus, m_iDesiredStatus);

		// всех в offline, только если мы бывали подключены
		if (dwOldStatusMode > ID_STATUS_OFFLINE)
			for (auto &hContact : AccContacts())
				SetContactBasicInfoW(hContact, SCBIFSI_LOCK_CHANGES_EVENTS, (SCBIF_ID | SCBIF_GROUP_ID | SCBIF_SERVER_FLAG | SCBIF_STATUS), -1, -1, 0, 0, ID_STATUS_OFFLINE, nullptr, nullptr, nullptr);

		if (m_hConnection != nullptr)
			Netlib_Shutdown(m_hConnection);
	}
	else {
		// если offline то сразу ставим connecting, но обработка как offline
		dwOldStatusMode = InterlockedCompareExchange((volatile LONG*)&m_iStatus, ID_STATUS_CONNECTING, ID_STATUS_OFFLINE);

		switch (dwOldStatusMode) {
		case ID_STATUS_OFFLINE: // offline, connecting
			if (StartConnect() != NO_ERROR) {
				m_bLoggedIn = FALSE;
				m_iDesiredStatus = ID_STATUS_OFFLINE;
				dwOldStatusMode = InterlockedExchange((volatile LONG*)&m_iStatus, m_iDesiredStatus);
			}
			break;
		case ID_STATUS_ONLINE:// connected, change status
		case ID_STATUS_AWAY:
		case ID_STATUS_DND:
		case ID_STATUS_FREECHAT:
		case ID_STATUS_INVISIBLE:
			MraSendNewStatus(m_iDesiredStatus, m_iXStatus, L"", L"");
		case ID_STATUS_CONNECTING:
			// предотвращаем переход в любой статус (кроме offline) из статуса connecting, если он не вызван самим плагином
			if (dwOldStatusMode == ID_STATUS_CONNECTING && iNewStatus != m_iDesiredStatus)
				break;

		default:
			dwOldStatusMode = InterlockedExchange((volatile LONG*)&m_iStatus, m_iDesiredStatus);
			break;
		}
	}
	MraSetContactStatus(NULL, m_iStatus);
	ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)dwOldStatusMode, m_iStatus);
	return 0;
}

HANDLE CMraProto::GetAwayMsg(MCONTACT hContact)
{
	if (!m_bLoggedIn || !hContact)
		return nullptr;

	wchar_t szStatusDesc[MICBLOG_STATUS_MAX + MICBLOG_STATUS_MAX + MAX_PATH], szTime[64];
	DWORD dwTime;
	int iRet = 0;

	CMStringW szBlogStatus;
	if (mraGetStringW(hContact, DBSETTING_BLOGSTATUS, szBlogStatus)) {
		SYSTEMTIME tt = { 0 };
		dwTime = getDword(hContact, DBSETTING_BLOGSTATUSTIME, 0);
		if (dwTime && MakeLocalSystemTimeFromTime32(dwTime, &tt))
			mir_snwprintf(szTime, L"%04ld.%02ld.%02ld %02ld:%02ld: ", tt.wYear, tt.wMonth, tt.wDay, tt.wHour, tt.wMinute);
		else
			szTime[0] = 0;

		mir_snwprintf(szStatusDesc, L"%s%s", szTime, szBlogStatus.c_str());
		iRet = GetTickCount();
		ProtoBroadcastAck(hContact, ACKTYPE_AWAYMSG, ACKRESULT_SUCCESS, (HANDLE)iRet, (LPARAM)szStatusDesc);
	}
	return (HANDLE)iRet;
}

int CMraProto::SetAwayMsg(int iStatus, const wchar_t *msg)
{
	if (!m_bLoggedIn)
		return 1;

	size_t dwStatusDescSize = mir_wstrlen(msg);
	DWORD dwStatus = iStatus;
	DWORD dwXStatus = m_iXStatus;

	// не отправл¤ем новый статусный текст дл¤ хстатусов, дл¤ хстатусов только эвей сообщени¤
	if (dwStatus != ID_STATUS_ONLINE || IsXStatusValid(dwXStatus) == FALSE) {
		dwStatusDescSize = min(dwStatusDescSize, STATUS_DESC_MAX);
		MraSendNewStatus(dwStatus, dwXStatus, L"", msg);
	}
	return 0;
}

int CMraProto::UserIsTyping(MCONTACT hContact, int type)
{
	if (!m_bLoggedIn || m_iStatus == ID_STATUS_INVISIBLE || !hContact || type == PROTOTYPE_SELFTYPING_OFF)
		return 1;

	CMStringA szEmail;
	if (MraGetContactStatus(hContact) != ID_STATUS_OFFLINE)
		if (mraGetStringA(hContact, "e-mail", szEmail))
			if (MraMessage(FALSE, hContact, 0, MESSAGE_FLAG_NOTIFY, szEmail, L" ", nullptr, 0))
				return 0;

	return 1;
}
