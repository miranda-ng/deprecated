#include "stdafx.h"
#include "proto.h"

LPCSTR lpcszStatusUri[] =
{
	"", // offline // "status_0",
	"STATUS_ONLINE", // "status_1",
	"STATUS_AWAY", 	// "status_2",
	"STATUS_INVISIBLE", // "status_3",
	"status_dnd",
	"status_chat",
	"status_mobile",
	"status_4", // first xstatus
	"status_5",
	"status_6",
	"status_7", // 10
	"status_8",
	"status_9",
	"status_10",
	"status_11",
	"status_12",
	"status_13",
	"status_14",
	"status_15",
	"status_16",
	"status_17", // 20
	"status_18",
	"status_19",
	"status_20",
	"status_21",
	"status_22",
	"status_23",
	"status_24",
	//"status_25", // chat/dnd
	"status_26",
	"status_27",
	"status_28", // 30
	"status_29",
	"status_30",
	//"status_31", // chat/dnd
	"status_32",
	"status_33",
	"status_34",
	"status_35",
	"status_36",
	"status_37",
	"status_38",
	"status_39", // 40
	"status_40",
	"status_41",
	"status_42",
	"status_43",
	"status_44",
	"status_45",
	"status_46",
	"status_47",
	"status_48",
	"status_49", // 50
	"status_50",
	"status_51",
	"status_52",
	"status_53",
	"status_dating",
	nullptr
};

LPCWSTR lpcszXStatusNameDef[] =
{
	LPGENW("None"),
	LPGENW("Sick"),
	LPGENW("Home"),
	LPGENW("Eating"),
	LPGENW("Compass"),
	LPGENW("On WC"),
	LPGENW("Cooking"),
	LPGENW("Walking"),
	LPGENW("Alien"),
	LPGENW("Shrimp"),
	LPGENW("Got lost"),
	LPGENW("Crazy"),
	LPGENW("Duck"),
	LPGENW("Playing"),
	LPGENW("Smoking"),
	LPGENW("Office"),
	LPGENW("Meeting"),
	LPGENW("Beer"),
	LPGENW("Coffee"),
	LPGENW("Working"),
	LPGENW("Relaxing"),
	LPGENW("On the phone"),
	LPGENW("In institute"),
	LPGENW("At school"),
	LPGENW("Wrong number"),
	LPGENW("Laughing"),
	LPGENW("Malicious"),
	LPGENW("Imp"),
	LPGENW("Blind"),
	LPGENW("Disappointed"),
	LPGENW("Almost crying"),
	LPGENW("Fearful"),
	LPGENW("Angry"),
	LPGENW("Vampire"),
	LPGENW("Ass"),
	LPGENW("Love"),
	LPGENW("Sleeping"),
	LPGENW("Cool!"),
	LPGENW("Peace!"),
	LPGENW("Cock a snook"),
	LPGENW("Get out"),
	LPGENW("Death"),
	LPGENW("Rocket"),
	LPGENW("Devil-fish"),
	LPGENW("Heavy metal"),
	LPGENW("Things look bad"),
	LPGENW("Squirrel"),
	LPGENW("Star"),
	LPGENW("Music"),
	LPGENW("Dating"),
	nullptr
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CMraProto::SetExtraIcons(MCONTACT hContact)
{
	DWORD dwID, dwGroupID, dwContactSeverFlags;
	if (GetContactBasicInfoW(hContact, &dwID, &dwGroupID, nullptr, &dwContactSeverFlags, nullptr, nullptr, nullptr, nullptr))
		return;

	DWORD dwIconID = -1;
	DWORD dwXStatus = MRA_MIR_XSTATUS_NONE;

	if (m_bLoggedIn) {
		dwXStatus = getByte(hContact, DBSETTING_XSTATUSID, MRA_MIR_XSTATUS_NONE);
		if (dwID == -1)
			dwIconID = (dwContactSeverFlags == -1) ? ADV_ICON_DELETED : ADV_ICON_NOT_ON_SERVER;
		else {
			if (dwGroupID == 103)
				dwIconID = ADV_ICON_PHONE;
			else {
				if (dwContactSeverFlags)
				if (dwContactSeverFlags == -1)
					dwIconID = ADV_ICON_DELETED;
				else
					dwIconID = ADV_ICON_NOT_AUTHORIZED;
			}
		}
	}

	if (dwIconID == -1) {
		CMStringW statusMsg;
		mraGetStringW(hContact, DBSETTING_BLOGSTATUS, statusMsg);
		if (statusMsg.GetLength())
			dwIconID = ADV_ICON_BLOGSTATUS;
	}

	ExtraIcon_SetIcon(hExtraXstatusIcon, hContact,
		(IsXStatusValid(dwXStatus) || dwXStatus == MRA_MIR_XSTATUS_UNKNOWN) ? hXStatusAdvancedStatusIcons[dwXStatus] : nullptr);
	ExtraIcon_SetIcon(hExtraInfo, hContact, (dwIconID != -1) ? gdiExtraStatusIconsItems[dwIconID].hIcolib : nullptr);
}

INT_PTR CMraProto::MraXStatusMenu(WPARAM, LPARAM, LPARAM param)
{
	if (MraRequestXStatusDetails(param) == FALSE)
		MraSetXStatusInternal(param);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

int CMraProto::MraContactDeleted(WPARAM hContact, LPARAM)
{
	if (!m_bLoggedIn || !hContact)
		return 0;

	if (IsContactMra(hContact)) {
		DWORD dwID, dwGroupID;
		GetContactBasicInfoW(hContact, &dwID, &dwGroupID, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

		MraSetContactStatus(hContact, ID_STATUS_OFFLINE);
		if (!db_get_b(hContact, "CList", "NotOnList", 0) || dwID != -1) {
			DWORD dwFlags = CONTACT_FLAG_REMOVED;
			MraModifyContact(hContact, &dwID, &dwFlags);
		}
		MraAvatarsDeleteContactAvatarFile(hAvatarsQueueHandle, hContact);
	}
	return 0;
}

int CMraProto::MraDbSettingChanged(WPARAM hContact, LPARAM lParam)
{
	if (!m_bLoggedIn || !lParam || !hContact)
		return 0;

	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING*)lParam;

	// это наш контакт, он не временный (есть в списке на сервере) и его обновление разрешено
	if (IsContactMra(hContact) && !db_get_b(hContact, "CList", "NotOnList", 0) && getDword(hContact, "HooksLocked", FALSE) == FALSE) {
		if (!strcmp(cws->szModule, "CList")) {
			// MyHandle setting
			if (!strcmp(cws->szSetting, "MyHandle")) {
				// always store custom nick
				CMStringW wszNick;
				if (cws->value.type == DBVT_DELETED) {
					wszNick = Clist_GetContactDisplayName(hContact);
					db_set_ws(hContact, "CList", "MyHandle", wszNick);
				}
				else if (cws->value.pszVal) {
					switch (cws->value.type) {
					case DBVT_WCHAR:
						wszNick = cws->value.pwszVal;
						break;
					case DBVT_UTF8:
						wszNick = ptrW(mir_utf8decodeW(cws->value.pszVal));
						break;
					case DBVT_ASCIIZ:
						wszNick = ptrW(mir_a2u_cp(cws->value.pszVal, MRA_CODE_PAGE));
						break;
					}
					if (wszNick.GetLength())
						MraModifyContact(hContact, nullptr, nullptr, nullptr, nullptr, &wszNick);
				}
			}
			// Group setting
			else if (!strcmp(cws->szSetting, "Group")) {
				CMStringW wszGroup;
				// manage group on server
				switch (cws->value.type) {
				case DBVT_WCHAR:
					wszGroup = cws->value.pwszVal;
					break;
				case DBVT_UTF8:
					wszGroup = ptrW(mir_utf8decodeW(cws->value.pszVal));
					break;
				case DBVT_ASCIIZ:
					wszGroup = ptrW(mir_a2u_cp(cws->value.pszVal, MRA_CODE_PAGE));
					break;
				}
				if (wszGroup.GetLength()) {
					DWORD dwGroupID = getDword(hContact, "GroupID", -1);
					if (dwGroupID != -1)
						MraMoveContactToGroup(hContact, dwGroupID, wszGroup);
				}
			}
			// NotOnList setting. Has a temporary contact just been added permanently?
			else if (!strcmp(cws->szSetting, "NotOnList")) {
				if (cws->value.type == DBVT_DELETED || (cws->value.type == DBVT_BYTE && cws->value.bVal == 0)) {
					CMStringW wszAuthMessage;
					if (!mraGetStringW(NULL, "AuthMessage", wszAuthMessage))
						wszAuthMessage = TranslateW(MRA_DEFAULT_AUTH_MESSAGE);

					db_unset(hContact, "CList", "Hidden");

					CMStringA szEmail, szPhones;
					CMStringW wszNick;
					DWORD dwGroupID, dwContactFlag;
					GetContactBasicInfoW(hContact, nullptr, &dwGroupID, &dwContactFlag, nullptr, nullptr, &szEmail, &wszNick, &szPhones);
					MraAddContact(hContact, dwContactFlag, dwGroupID, szEmail, wszNick, &szPhones, &wszAuthMessage);
				}
			}
			// Hidden setting
			else if (!strcmp(cws->szSetting, "Hidden")) {
				DWORD dwContactFlag = GetContactFlags(hContact);
				if (cws->value.type == DBVT_DELETED || (cws->value.type == DBVT_BYTE && cws->value.bVal == 0))
					dwContactFlag &= ~CONTACT_FLAG_SHADOW;
				else
					dwContactFlag |= CONTACT_FLAG_SHADOW;

				MraModifyContact(hContact, nullptr, &dwContactFlag);
			}
		}
		// Ignore section
		else if (!strcmp(cws->szModule, "Ignore")) {
			if (!strcmp(cws->szSetting, "Mask1")) {
				DWORD dwContactFlag = GetContactFlags(hContact);
				if (cws->value.type == DBVT_DELETED || (cws->value.type == DBVT_DWORD && cws->value.dVal&IGNOREEVENT_MESSAGE) == 0)
					dwContactFlag &= ~CONTACT_FLAG_IGNORE;
				else
					dwContactFlag |= CONTACT_FLAG_IGNORE;

				MraModifyContact(hContact, nullptr, &dwContactFlag);
			}
		}
		// User info section
		else if (!strcmp(cws->szModule, "UserInfo")) {
			if (!strcmp(cws->szSetting, "MyPhone0") || !strcmp(cws->szSetting, "MyPhone1") || !strcmp(cws->szSetting, "MyPhone2"))
				MraModifyContact(hContact);
		}
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Extra icons

int CMraProto::MraExtraIconsApply(WPARAM wParam, LPARAM)
{
	SetExtraIcons(wParam);
	return 0;
}

INT_PTR CMraProto::MraSetListeningTo(WPARAM, LPARAM lParam)
{
	LISTENINGTOINFO *pliInfo = (LISTENINGTOINFO*)lParam;

	if (pliInfo == nullptr || pliInfo->cbSize != sizeof(LISTENINGTOINFO)) {
		MraChangeUserBlogStatus(MRIM_BLOG_STATUS_MUSIC, L"", 0);
		delSetting(DBSETTING_BLOGSTATUSMUSIC);
	}
	else if (pliInfo->dwFlags & LTI_UNICODE) {
		CMStringW wszListeningTo;
		if (ServiceExists(MS_LISTENINGTO_GETPARSEDTEXT))
			wszListeningTo = ptrW((LPWSTR)CallService(MS_LISTENINGTO_GETPARSEDTEXT, (WPARAM)L"%track%. %title% - %artist% - %player%", (LPARAM)pliInfo));
		else
			wszListeningTo.Format(L"%s. %s - %s - %s", pliInfo->ptszTrack ? pliInfo->ptszTrack : L"", pliInfo->ptszTitle ? pliInfo->ptszTitle : L"", pliInfo->ptszArtist ? pliInfo->ptszArtist : L"", pliInfo->ptszPlayer ? pliInfo->ptszPlayer : L"");

		mraSetStringExW(NULL, DBSETTING_BLOGSTATUSMUSIC, wszListeningTo);
		MraChangeUserBlogStatus(MRIM_BLOG_STATUS_MUSIC, wszListeningTo, 0);
	}

	return 0;
}

int CMraProto::MraMusicChanged(WPARAM wParam, LPARAM lParam)
{
	switch (wParam) {
	case WAT_EVENT_PLAYERSTATUS:
		// stopped
		if (1 == lParam) {
			delSetting(DBSETTING_BLOGSTATUSMUSIC);
			MraChangeUserBlogStatus(MRIM_BLOG_STATUS_MUSIC, L"", 0);
		}
		break;

	case WAT_EVENT_NEWTRACK:
		SONGINFO *psiSongInfo;
		if (WAT_RES_OK == CallService(MS_WAT_GETMUSICINFO, WAT_INF_UNICODE, (LPARAM)&psiSongInfo)) {
			CMStringW wszMusic;
			wszMusic.Format(L"%ld. %s - %s - %s", psiSongInfo->track, psiSongInfo->artist, psiSongInfo->title, psiSongInfo->player);
			mraSetStringExW(NULL, DBSETTING_BLOGSTATUSMUSIC, wszMusic);
			MraChangeUserBlogStatus(MRIM_BLOG_STATUS_MUSIC, wszMusic, 0);
		}
		break;
	}

	return 0;
}

DWORD CMraProto::MraSetXStatusInternal(DWORD dwXStatus)
{
	if (IsXStatusValid(dwXStatus)) {
		CMStringW szBuff;

		// obsolete (TODO: remove in next version)
		char szValueName[MAX_PATH];
		mir_snprintf(szValueName, "XStatus%ldName", dwXStatus);
		if (!mraGetStringW(NULL, szValueName, szBuff))
			szBuff = lpcszXStatusNameDef[dwXStatus];
		mraSetStringExW(NULL, DBSETTING_XSTATUSNAME, szBuff);

		// obsolete (TODO: remove in next version)
		mir_snprintf(szValueName, "XStatus%ldMsg", dwXStatus);
		if (mraGetStringW(NULL, szValueName, szBuff))
			mraSetStringExW(NULL, DBSETTING_XSTATUSMSG, szBuff);
		else
			delSetting(DBSETTING_XSTATUSMSG);
	}
	else {
		delSetting(DBSETTING_XSTATUSNAME);
		delSetting(DBSETTING_XSTATUSMSG);
		dwXStatus = MRA_MIR_XSTATUS_NONE;
	}

	DWORD dwOldStatusMode = InterlockedExchange((volatile LONG*)&m_iXStatus, dwXStatus);
	setByte(DBSETTING_XSTATUSID, (BYTE)dwXStatus);

	MraSendNewStatus(m_iStatus, dwXStatus, L"", L"");

	return dwOldStatusMode;
}

INT_PTR CMraProto::MraSetXStatusEx(WPARAM, LPARAM lParam)
{
	INT_PTR iRet = 1;
	DWORD dwXStatus;
	CUSTOM_STATUS *pData = (CUSTOM_STATUS*)lParam;

	if (pData->cbSize >= sizeof(CUSTOM_STATUS)) {
		iRet = 0;

		if (pData->flags & CSSF_MASK_STATUS) {
			dwXStatus = *pData->status;
			if (IsXStatusValid(dwXStatus) == FALSE && dwXStatus != MRA_MIR_XSTATUS_NONE)
				iRet = 1;
		}
		else dwXStatus = m_iXStatus;

		if (pData->flags & (CSSF_MASK_NAME | CSSF_MASK_MESSAGE) && iRet == 0) {
			if (IsXStatusValid(dwXStatus) || dwXStatus == MRA_MIR_XSTATUS_NONE) {
				CHAR szValueName[MAX_PATH];

				// set custom status name
				if (pData->flags & CSSF_MASK_NAME) {
					mir_snprintf(szValueName, "XStatus%ldName", dwXStatus);
					if (pData->flags & CSSF_UNICODE) {
						mraSetStringExW(NULL, szValueName, pData->pwszName);
						mraSetStringExW(NULL, DBSETTING_XSTATUSNAME, pData->pwszName);
					}
					else {
						mraSetStringExA(NULL, szValueName, pData->pszName);
						mraSetStringExA(NULL, DBSETTING_XSTATUSNAME, pData->pszName);
					}
				}

				// set custom status message
				if (pData->flags & CSSF_MASK_MESSAGE) {
					mir_snprintf(szValueName, "XStatus%ldMsg", dwXStatus);
					if (pData->flags & CSSF_UNICODE) {
						mraSetStringExW(NULL, szValueName, pData->pwszMessage);
						mraSetStringExW(NULL, DBSETTING_XSTATUSMSG, pData->pwszMessage);
					}
					else {
						mraSetStringExA(NULL, szValueName, pData->pszMessage);
						mraSetStringExA(NULL, DBSETTING_XSTATUSMSG, pData->pszMessage);
					}
				}
			}
			// неудача только если мы не ставили Хстатус и попытались записать сообщения для "нет" статуса
			else if (!(pData->flags & CSSF_MASK_STATUS))
				iRet = 1;
		}

		// set/update xstatus code and/or message
		if (pData->flags & (CSSF_MASK_STATUS | CSSF_MASK_NAME | CSSF_MASK_MESSAGE) && iRet == 0)
			MraSetXStatusInternal(dwXStatus);

		// hide menu items
		if (pData->flags & CSSF_DISABLE_UI) {
			m_bHideXStatusUI = (*pData->wParam) ? false : true;
			DWORD dwCount = MRA_XSTATUS_OFF_CLI_COUNT;
			if (getByte(NULL, "xStatusShowAll", MRA_DEFAULT_SHOW_ALL_XSTATUSES))
				dwCount = MRA_XSTATUS_COUNT;
			for (DWORD i = 0; i < dwCount; i++)
				Menu_ShowItem(hXStatusMenuItems[i], !m_bHideXStatusUI);
		}
	}
	return iRet;
}

INT_PTR CMraProto::MraGetXStatusEx(WPARAM hContact, LPARAM lParam)
{
	CUSTOM_STATUS *pData = (CUSTOM_STATUS*)lParam;

	if (pData->cbSize < sizeof(CUSTOM_STATUS))
		return 1;

	// fill status member
	if (pData->flags & CSSF_MASK_STATUS)
		*pData->status = m_iXStatus;

	// fill status name member
	if (pData->flags & CSSF_MASK_NAME) {
		if (pData->flags & CSSF_DEFAULT_NAME) {
			DWORD dwXStatus = (pData->wParam == nullptr) ? m_iXStatus : *pData->wParam;
			if (!IsXStatusValid(dwXStatus))
				return 1;

			if (pData->flags & CSSF_UNICODE)
				mir_wstrncpy(pData->ptszName, lpcszXStatusNameDef[dwXStatus], (STATUS_TITLE_MAX + 1));
			else {
				size_t dwStatusTitleSize = mir_wstrlen(lpcszXStatusNameDef[dwXStatus]);
				if (dwStatusTitleSize > STATUS_TITLE_MAX)
					dwStatusTitleSize = STATUS_TITLE_MAX;

				WideCharToMultiByte(MRA_CODE_PAGE, 0, lpcszXStatusNameDef[dwXStatus], (DWORD)dwStatusTitleSize, pData->pszName, MAX_PATH, nullptr, nullptr);
				(*((CHAR*)(pData->pszName + dwStatusTitleSize))) = 0;
			}
		}
		else {
			if (pData->flags & CSSF_UNICODE)
				mraGetStaticStringW(hContact, DBSETTING_XSTATUSNAME, pData->pwszName, (STATUS_TITLE_MAX + 1), nullptr);
			else
				mraGetStaticStringA(hContact, DBSETTING_XSTATUSNAME, pData->pszName, (STATUS_TITLE_MAX + 1), nullptr);
		}
	}

	// fill status message member
	if (pData->flags & CSSF_MASK_MESSAGE) {
		char szSetting[100];
		mir_snprintf(szSetting, "XStatus%dMsg", m_iXStatus);
		if (pData->flags & CSSF_UNICODE)
			mraGetStaticStringW(hContact, szSetting, pData->pwszMessage, (STATUS_DESC_MAX + 1), nullptr);
		else
			mraGetStaticStringA(hContact, szSetting, pData->pszMessage, (STATUS_DESC_MAX + 1), nullptr);
	}

	if ((pData->flags & CSSF_DISABLE_UI) && pData->wParam)
		*pData->wParam = m_bHideXStatusUI;

	if ((pData->flags & CSSF_STATUSES_COUNT) && pData->wParam)
		*pData->wParam = MRA_XSTATUS_COUNT - 1;

	//**deb можно оптимизировать, данный параметр возможно уже был вычислен при получении самих текстов
	if (pData->flags & CSSF_STR_SIZES) {
		if (pData->wParam) mraGetStaticStringW(hContact, DBSETTING_XSTATUSNAME, nullptr, 0, (size_t*)pData->wParam);
		if (pData->lParam) mraGetStaticStringW(hContact, DBSETTING_XSTATUSMSG, nullptr, 0, (size_t*)pData->lParam);
	}
	return 0;
}

INT_PTR CMraProto::MraGetXStatusIcon(WPARAM wParam, LPARAM lParam)
{
	if (wParam == 0)
		wParam = m_iXStatus;
	if ( !IsXStatusValid(wParam))
		return 0;

	return (INT_PTR)IconLibGetIconEx(hXStatusAdvancedStatusIcons[wParam], lParam);
}

DWORD CMraProto::MraSendNewStatus(DWORD dwStatusMir, DWORD dwXStatusMir, const CMStringW &pwszStatusTitle, const CMStringW &pwszStatusDesc)
{
	if (!m_bLoggedIn)
		return 0;

	CMStringW wszStatusTitle, wszStatusDesc;
	DWORD dwXStatus, dwStatus = GetMraStatusFromMiradaStatus(dwStatusMir, dwXStatusMir, &dwXStatus);
	if (IsXStatusValid(dwXStatusMir)) {
		char szValueName[MAX_PATH];
		if (pwszStatusTitle.IsEmpty()) {
			mir_snprintf(szValueName, "XStatus%ldName", dwXStatusMir);
			// custom xstatus name
			if (!mraGetStringW(NULL, szValueName, wszStatusTitle))
				wszStatusTitle = TranslateW(lpcszXStatusNameDef[dwXStatusMir]);
		}
		else wszStatusTitle = pwszStatusTitle;

		if (pwszStatusDesc.IsEmpty()) {
			mir_snprintf(szValueName, "XStatus%ldMsg", dwXStatusMir);
			// custom xstatus description
			mraGetStringW(NULL, szValueName, wszStatusDesc);
		}
		else wszStatusDesc = pwszStatusDesc;
	}
	else if (pwszStatusTitle.IsEmpty())
		wszStatusTitle = Clist_GetStatusModeDescription(dwStatusMir, 0);

	MraChangeStatus(dwStatus, lpcszStatusUri[dwXStatus], wszStatusTitle, wszStatusDesc, ((getByte("RTFReceiveEnable", MRA_DEFAULT_RTF_RECEIVE_ENABLE) ? FEATURE_FLAG_RTF_MESSAGE : 0) | MRA_FEATURE_FLAGS));
	return 0;
}

INT_PTR CMraProto::MraSendSMS(WPARAM wParam, LPARAM lParam)
{
	if (!m_bLoggedIn || !wParam || !lParam)
		return 0;

	ptrW lpwszMessageXMLEncoded(mir_utf8decodeW((LPSTR)lParam));
	if (lpwszMessageXMLEncoded) {
		CMStringW decoded = DecodeXML(CMStringW(lpwszMessageXMLEncoded));
		if (decoded.GetLength())
			return (MraSMSW(NULL, CMStringA((LPSTR)wParam), decoded));
	}

	return 0;
}

INT_PTR CMraProto::MraSendNudge(WPARAM hContact, LPARAM)
{
	if (m_bLoggedIn && hContact) {
		LPWSTR lpwszAlarmMessage = TranslateW(MRA_ALARM_MESSAGE);

		CMStringA szEmail;
		if (mraGetStringA(hContact, "e-mail", szEmail))
			if (MraMessage(FALSE, hContact, 0, (MESSAGE_FLAG_RTF | MESSAGE_FLAG_ALARM), szEmail, lpwszAlarmMessage, nullptr, 0))
				return 0;
	}
	return 1;
}

INT_PTR CMraProto::GetUnreadEmailCount(WPARAM, LPARAM)
{
	if (!m_bLoggedIn)
		return 0;
	return m_dwEmailMessagesUnread;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Avatars

INT_PTR CMraProto::MraGetAvatarCaps(WPARAM wParam, LPARAM)
{

	switch (wParam) {
	case AF_MAXSIZE:
		return -1;
	case AF_PROPORTION:
		return PIP_NONE;
	case AF_FORMATSUPPORTED:
		return 0; // no formats to set
	case AF_ENABLED:
		return 1; // always on
	case AF_DONTNEEDDELAYS:
		return 0; // need delay
	case AF_MAXFILESIZE:
		return 0;
	case AF_DELAYAFTERFAIL:
		return 5000;
	}

	return 0;
}


INT_PTR CMraProto::MraGetAvatarInfo(WPARAM wParam, LPARAM lParam)
{
	if (lParam) {
		PROTO_AVATAR_INFORMATION *pai = (PROTO_AVATAR_INFORMATION*)lParam;
		return (INT_PTR)MraAvatarsQueueGetAvatar(hAvatarsQueueHandle, (DWORD)wParam, pai->hContact, nullptr, (DWORD*)&pai->format, pai->filename);
	}
	return GAIR_NOAVATAR;
}


INT_PTR CMraProto::MraGetMyAvatar(WPARAM wParam, LPARAM lParam)
{
	CMStringW wszFileName;
	if (MraAvatarsGetFileName(hAvatarsQueueHandle, NULL, GetContactAvatarFormat(NULL, PA_FORMAT_DEFAULT), wszFileName) == NO_ERROR) {
		mir_wstrncpy((LPTSTR)wParam, wszFileName, (size_t)lParam);
		return 0;
	}
	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// processes clist group removal

int CMraProto::OnGroupChanged(WPARAM hContact, LPARAM lParam)
{
	if (hContact == NULL) {
		CLISTGROUPCHANGE *cgc = (CLISTGROUPCHANGE*)lParam;
		if (cgc->pszOldName == nullptr)
			return 0;

		debugLogA("removing group %S", cgc->pszOldName);
		MraGroupItem *pGrp = nullptr;
		for (auto &it : m_groups) {
			if (!mir_wstrcmp(it->m_name, cgc->pszOldName)) {
				pGrp = it;
				debugLogA("found a group at the server, idx = %d", it->m_id);
				break;
			}
		}
		if (pGrp == nullptr) // no MRA contacts in it
			return 0;

		DWORD dwFlags = CONTACT_FLAG_GROUP;
		CMStringW wszGroup;
		if (cgc->pszNewName != nullptr) // renaming group
			wszGroup = cgc->pszNewName;
		else { // removing group
			dwFlags |= CONTACT_FLAG_REMOVED;
			wszGroup = cgc->pszOldName;
		}
		MraModifyContact(NULL, &pGrp->m_id, &dwFlags, nullptr, nullptr, &wszGroup);
	}
	return 0;
}
