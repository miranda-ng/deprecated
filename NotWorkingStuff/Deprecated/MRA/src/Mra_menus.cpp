#include "stdafx.h"
#include "proto.h"

INT_PTR CMraProto::MraGotoInbox(WPARAM, LPARAM)
{
	MraMPopSessionQueueAddUrl(hMPopSessionQueue, MRA_WIN_INBOX_URL);
	return 0;
}

INT_PTR CMraProto::MraShowInboxStatus(WPARAM, LPARAM)
{
	MraUpdateEmailStatus("", "", true);
	return 0;
}

INT_PTR CMraProto::MraEditProfile(WPARAM, LPARAM)
{
	MraMPopSessionQueueAddUrl(hMPopSessionQueue, MRA_EDIT_PROFILE_URL);
	return 0;
}

INT_PTR CMraProto::MraWebSearch(WPARAM, LPARAM)
{
	Utils_OpenUrl(MRA_SEARCH_URL);
	return 0;
}

INT_PTR CMraProto::MraUpdateAllUsersInfo(WPARAM, LPARAM)
{
	if (!m_bLoggedIn)
		return 0;

	if (MessageBox(nullptr, TranslateT("Are you sure?"), TranslateT(MRA_UPD_ALL_USERS_INFO_STR), MB_YESNO | MB_ICONQUESTION) == IDYES) {
		for (auto &hContact : AccContacts()) {
			CMStringA szEmail;
			if (mraGetStringA(hContact, "e-mail", szEmail))
				MraWPRequestByEMail(hContact, ACKTYPE_GETINFO, szEmail);
		}
	}
	return 0;
}

INT_PTR CMraProto::MraCheckUpdatesUsersAvt(WPARAM, LPARAM)
{
	if (MessageBox(nullptr, TranslateT("Are you sure?"), TranslateT(MRA_CHK_USERS_AVATARS_STR), MB_YESNO | MB_ICONQUESTION) == IDYES) {
		for (auto &hContact : AccContacts()) {
			CMStringA szEmail;
			if (mraGetStringA(hContact, "e-mail", szEmail))
			if (!IsEMailChatAgent(szEmail))
				MraAvatarsQueueGetAvatarSimple(hAvatarsQueueHandle, 0, hContact);
		}
	}
	return 0;
}

INT_PTR CMraProto::MraRequestAuthForAll(WPARAM, LPARAM)
{
	if (!m_bLoggedIn)
		return 0;

	if (MessageBox(nullptr, TranslateT("Are you sure?"), TranslateT(MRA_REQ_AUTH_FOR_ALL_STR), MB_YESNO | MB_ICONQUESTION) == IDYES) {
		for (auto &hContact : AccContacts()) {
			DWORD dwContactSeverFlags;
			if (GetContactBasicInfoW(hContact, nullptr, nullptr, nullptr, &dwContactSeverFlags, nullptr, nullptr, nullptr, nullptr) == NO_ERROR)
			if (dwContactSeverFlags & CONTACT_INTFLAG_NOT_AUTHORIZED && dwContactSeverFlags != -1)
				MraRequestAuthorization(hContact, 0);
		}
	}
	return 0;
}

INT_PTR CMraProto::MraRequestAuthorization(WPARAM hContact, LPARAM)
{
	if (!hContact || !m_bLoggedIn)
		return 0;

	CMStringW wszAuthMessage;
	if (!mraGetStringW(NULL, "AuthMessage", wszAuthMessage))
		wszAuthMessage = TranslateW(MRA_DEFAULT_AUTH_MESSAGE);

	if (wszAuthMessage.IsEmpty())
		return 1;

	CMStringA szEmail;
	if (mraGetStringA(hContact, "e-mail", szEmail)) {
		BOOL bSlowSend = getByte("SlowSend", MRA_DEFAULT_SLOW_SEND);
		int iRet = MraMessage(bSlowSend, hContact, ACKTYPE_AUTHREQ, MESSAGE_FLAG_AUTHORIZE, szEmail, wszAuthMessage, nullptr, 0);
		if (bSlowSend == FALSE)
			ProtoBroadcastAck(hContact, ACKTYPE_AUTHREQ, ACKRESULT_SUCCESS, (HANDLE)iRet, 0);

		return 0;
	}
	return 1;
}

INT_PTR CMraProto::MraGrantAuthorization(WPARAM wParam, LPARAM)
{
	if (!m_bLoggedIn || !wParam)
		return 0;

	// send without reason, do we need any ?
	CMStringA szEmail;
	if (mraGetStringA(wParam, "e-mail", szEmail))
		MraAuthorize(szEmail);

	return 0;
}

INT_PTR CMraProto::MraSendEmail(WPARAM wParam, LPARAM)
{
	DWORD dwContactEMailCount = GetContactEMailCount(wParam, FALSE);
	if (dwContactEMailCount) {
		if (dwContactEMailCount == 1) {
			CMStringA szEmail;
			if (GetContactFirstEMail(wParam, FALSE, szEmail)) {
				szEmail.MakeLower();
				MraMPopSessionQueueAddUrl(hMPopSessionQueue, "https://e.mail.ru/cgi-bin/sentmsg?To=" + szEmail);
			}
		}
		else MraSelectEMailDlgShow(wParam, MRA_SELECT_EMAIL_TYPE_SEND_POSTCARD);
	}
	return 0;
}

INT_PTR CMraProto::MraSendPostcard(WPARAM wParam, LPARAM)
{
	DWORD dwContactEMailCount = GetContactEMailCount(wParam, FALSE);
	if (dwContactEMailCount) {
		if (dwContactEMailCount == 1) {
			CMStringA szUrl, szEmail;
			if (GetContactFirstEMail(wParam, FALSE, szEmail)) {
				szEmail.MakeLower();
				szUrl.Format("http://cards.mail.ru/event.html?rcptname=%S&rcptemail=%s", _T2A(Clist_GetContactDisplayName(wParam)), szEmail.c_str());
				MraMPopSessionQueueAddUrl(hMPopSessionQueue, szUrl);
			}
		}
		else MraSelectEMailDlgShow(wParam, MRA_SELECT_EMAIL_TYPE_SEND_POSTCARD);
	}
	return 0;
}

INT_PTR CMraProto::MraViewAlbum(WPARAM wParam, LPARAM)
{
	DWORD dwContactEMailMRCount = GetContactEMailCount(wParam, TRUE);
	if (dwContactEMailMRCount) {
		if (dwContactEMailMRCount == 1) {
			CMStringA szEmail;
			if (GetContactFirstEMail(wParam, TRUE, szEmail))
				MraMPopSessionQueueAddUrlAndEMail(hMPopSessionQueue, MRA_FOTO_URL, szEmail);
		}
		else MraSelectEMailDlgShow(wParam, MRA_SELECT_EMAIL_TYPE_VIEW_ALBUM);
	}
	return 0;
}

INT_PTR CMraProto::MraReplyBlogStatus(WPARAM wParam, LPARAM)
{
	if (!m_bLoggedIn)
		return 0;

	CMStringW blogStatusMsg;
	mraGetStringW(wParam, DBSETTING_BLOGSTATUS, blogStatusMsg);
	if (!blogStatusMsg.IsEmpty() || wParam == 0)
		MraSendReplyBlogStatus(wParam);

	return 0;
}

INT_PTR CMraProto::MraViewVideo(WPARAM wParam, LPARAM)
{
	DWORD dwContactEMailMRCount = GetContactEMailCount(wParam, TRUE);
	if (dwContactEMailMRCount) {
		if (dwContactEMailMRCount == 1) {
			CMStringA szEmail;
			if (GetContactFirstEMail(wParam, TRUE, szEmail))
				MraMPopSessionQueueAddUrlAndEMail(hMPopSessionQueue, MRA_VIDEO_URL, szEmail);
		}
		else MraSelectEMailDlgShow(wParam, MRA_SELECT_EMAIL_TYPE_VIEW_VIDEO);
	}
	return 0;
}

INT_PTR CMraProto::MraAnswers(WPARAM wParam, LPARAM)
{
	DWORD dwContactEMailMRCount = GetContactEMailCount(wParam, TRUE);
	if (dwContactEMailMRCount) {
		if (dwContactEMailMRCount == 1) {
			CMStringA szEmail;
			if (GetContactFirstEMail(wParam, TRUE, szEmail))
				MraMPopSessionQueueAddUrlAndEMail(hMPopSessionQueue, MRA_ANSWERS_URL, szEmail);
		}
		else MraSelectEMailDlgShow(wParam, MRA_SELECT_EMAIL_TYPE_ANSWERS);
	}
	return 0;
}

INT_PTR CMraProto::MraWorld(WPARAM wParam, LPARAM)
{
	DWORD dwContactEMailMRCount = GetContactEMailCount(wParam, TRUE);
	if (dwContactEMailMRCount) {
		if (dwContactEMailMRCount == 1) {
			CMStringA szEmail;
			if (GetContactFirstEMail(wParam, TRUE, szEmail))
				MraMPopSessionQueueAddUrlAndEMail(hMPopSessionQueue, MRA_WORLD_URL, szEmail);
		}
		else MraSelectEMailDlgShow(wParam, MRA_SELECT_EMAIL_TYPE_WORLD);
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

int CMraProto::MraRebuildContactMenu(WPARAM hContact, LPARAM)
{
	bool bIsContactMRA, bHasEMail, bHasEMailMR, bChatAgent;
	DWORD dwContactSeverFlags = 0;
	CMStringW blogStatusMsgSize;

	// proto own contact
	bIsContactMRA = IsContactMra(hContact);
	if (bIsContactMRA) {
		bHasEMail = true;
		bHasEMailMR = true;
		bChatAgent = IsContactChatAgent(hContact);
		GetContactBasicInfoW(hContact, nullptr, nullptr, nullptr, &dwContactSeverFlags, nullptr, nullptr, nullptr, nullptr);
		mraGetStringW(hContact, DBSETTING_BLOGSTATUS, blogStatusMsgSize);
	}
	// non proto contact
	else {
		bHasEMail = false;
		bHasEMailMR = false;
		bChatAgent = false;
		if (!getByte(NULL, "HideMenuItemsForNonMRAContacts", MRA_DEFAULT_HIDE_MENU_ITEMS_FOR_NON_MRA))
		if (!IsContactMraProto(hContact))// избегаем добавления менюшек в контакты других копий MRA
		if (GetContactEMailCount(hContact, FALSE)) {
			bHasEMail = true;
			if (GetContactEMailCount(hContact, TRUE)) bHasEMailMR = true;
		}
	}
	// menu root;
	Menu_ShowItem(hContactMenuRoot, bHasEMail);

	//"Request authorization"
	Menu_ShowItem(hContactMenuItems[0], (m_bLoggedIn && bIsContactMRA));// && (dwContactSeverFlags&CONTACT_INTFLAG_NOT_AUTHORIZED)

	//"Grant authorization"
	Menu_ShowItem(hContactMenuItems[1], (m_bLoggedIn && bIsContactMRA && !bChatAgent));

	//"&Send E-Mail"
	Menu_ShowItem(hContactMenuItems[2], (bHasEMail && !bChatAgent));

	//"&Send postcard"
	Menu_ShowItem(hContactMenuItems[3], (bHasEMail && !bChatAgent));

	//"&View Album"
	Menu_ShowItem(hContactMenuItems[4], (bHasEMailMR && !bChatAgent));

	//"Reply Blog Status"
	Menu_ShowItem(hContactMenuItems[5], (m_bLoggedIn && blogStatusMsgSize.GetLength() && !bChatAgent));

	//"View Video"
	Menu_ShowItem(hContactMenuItems[6], (bHasEMailMR && !bChatAgent));

	//"Answers"
	Menu_ShowItem(hContactMenuItems[7], (bHasEMailMR && !bChatAgent));

	//"World"
	Menu_ShowItem(hContactMenuItems[8], (bHasEMailMR && !bChatAgent));

	//"Send &Nudge"
	Menu_ShowItem(hContactMenuItems[9], (!m_heNudgeReceived) ? (m_bLoggedIn && bIsContactMRA) : 0);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

int CMraProto::MraRebuildStatusMenu(WPARAM, LPARAM)
{
	CHAR szServiceFunction[MAX_PATH], szValueName[MAX_PATH];

	HGENMENU hRoot;
	{
		wchar_t szItem[MAX_PATH + 64];
		mir_snwprintf(szItem, L"%s Custom Status", m_tszUserName);

		CMenuItem mi(&g_plugin);
		mi.root = Menu_GetProtocolMenu(m_szModuleName);
		mi.name.w = szItem;
		mi.position = 10001;
		hRoot = Menu_AddStatusMenuItem(&mi, m_szModuleName);
	}

	CMenuItem mi(&g_plugin);
	mi.position = 2000060000;
	mi.root = hRoot;
	mi.flags = CMIF_UNICODE;
	mi.pszService = szServiceFunction;

	CMStringW szStatusTitle;

	DWORD dwCount = MRA_XSTATUS_OFF_CLI_COUNT;
	if (getByte(NULL, "xStatusShowAll", MRA_DEFAULT_SHOW_ALL_XSTATUSES))
		dwCount = MRA_XSTATUS_COUNT;
	for (DWORD i = 0; i < dwCount; i ++) {
		mir_snprintf(szServiceFunction, "/menuXStatus%ld", i);
		mi.position ++;
		if (i) {
			mir_snprintf(szValueName, "XStatus%ldName", i);
			if (mraGetStringW(NULL, szValueName, szStatusTitle))
				mi.name.w = (wchar_t*)szStatusTitle.c_str();
			else
				mi.name.w = (wchar_t*)lpcszXStatusNameDef[i];

			mi.hIcolibItem = hXStatusAdvancedStatusIcons[i];
		}
		else {
			mi.name.w = (wchar_t*)lpcszXStatusNameDef[i];
			mi.hIcolibItem = nullptr;
		}
		hXStatusMenuItems[i] = Menu_AddStatusMenuItem(&mi, m_szModuleName);
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

HGENMENU CMraProto::CListCreateMenu(LONG lPosition, LONG lPopupPosition, BOOL bIsMain, const IconItem *pgdiItems, size_t dwCount, HGENMENU *hResult)
{
	if (!pgdiItems || !dwCount || !hResult)
		return nullptr;

	char szServiceFunction[MAX_PATH];

	CMenuItem mi(&g_plugin);

	HGENMENU hRootMenu, (__stdcall *fnAddFunc)(TMO_MenuItem*, const char*);
	if (bIsMain) {
		fnAddFunc = Menu_AddProtoMenuItem;

		hRootMenu = Menu_GetProtocolRoot(this);
		if (hRootMenu == nullptr) {
			mi.name.w = m_tszUserName;
			mi.flags = CMIF_UNICODE | CMIF_KEEPUNTRANSLATED;
			mi.hIcolibItem = g_hMainIcon;
			hRootMenu = Menu_AddProtoMenuItem(&mi);
		}

		mi.position = 20003;
		mi.root = hRootMenu;
	}
	else {
		fnAddFunc = Menu_AddContactMenuItem;
		mi.position = lPosition;
	}

	SET_UID(mi, 0x83C8B6A7, 0xEC0D, 0x41D6, 0x8A, 0x0E, 0xAC, 0x90, 0x8C, 0xEE, 0xAF, 0xFE);
	mi.flags = 0;
	mi.name.a = LPGEN("Services...");
	mi.hIcolibItem = g_hMainIcon;
	hRootMenu = fnAddFunc(&mi, m_szModuleName);
	UNSET_UID(mi);

	mi.flags = CMIF_SYSTEM;
	mi.root = hRootMenu;
	mi.pszService = szServiceFunction;

	for (size_t i = 0; i < dwCount; i++) {
		mi.pszService = pgdiItems[i].szName;
		mi.position = int(lPosition + i);
		mi.hIcolibItem = pgdiItems[i].hIcolib;
		mi.name.a = pgdiItems[i].szDescr;
		hResult[i] = fnAddFunc(&mi, m_szModuleName);
		Menu_ConfigureItem(hResult[i], MCI_OPT_EXECPARAM, lPopupPosition);
	}

	return hRootMenu;
}

void CMraProto::InitMenus()
{
	/* Main menu and contacts services. */
	CreateProtoService(MRA_GOTO_INBOX, &CMraProto::MraGotoInbox);
	CreateProtoService(MRA_SHOW_INBOX_STATUS, &CMraProto::MraShowInboxStatus);
	CreateProtoService(MRA_EDIT_PROFILE, &CMraProto::MraEditProfile);
	CreateProtoService(MRA_VIEW_ALBUM, &CMraProto::MraViewAlbum);
	CreateProtoService(MRA_REPLY_BLOG_STATUS, &CMraProto::MraReplyBlogStatus);
	CreateProtoService(MRA_VIEW_VIDEO, &CMraProto::MraViewVideo);
	CreateProtoService(MRA_ANSWERS, &CMraProto::MraAnswers);
	CreateProtoService(MRA_WORLD, &CMraProto::MraWorld);
	CreateProtoService(MRA_WEB_SEARCH, &CMraProto::MraWebSearch);
	CreateProtoService(MRA_UPD_ALL_USERS_INFO, &CMraProto::MraUpdateAllUsersInfo);
	CreateProtoService(MRA_CHK_USERS_AVATARS, &CMraProto::MraCheckUpdatesUsersAvt);
	CreateProtoService(MRA_REQ_AUTH_FOR_ALL, &CMraProto::MraRequestAuthForAll);
	/* Contacts only services. */
	CreateProtoService(MRA_REQ_AUTH, &CMraProto::MraRequestAuthorization);
	CreateProtoService(MRA_GRANT_AUTH, &CMraProto::MraGrantAuthorization);
	CreateProtoService(MRA_SEND_EMAIL, &CMraProto::MraSendEmail);
	CreateProtoService(MRA_SEND_POSTCARD, &CMraProto::MraSendPostcard);

	hContactMenuRoot = CListCreateMenu(-2000001001, -500050000, FALSE, gdiContactMenuItems, CONTACT_MENU_ITEMS_COUNT, hContactMenuItems);

	// xstatus menu
	for (DWORD i = 0; i < MRA_XSTATUS_COUNT; i++) {
		char szServiceName[100];
		mir_snprintf(szServiceName, "/menuXStatus%d", i);
		CreateProtoServiceParam(szServiceName, &CMraProto::MraXStatusMenu, i);
	}
}
