/*
Plugin of Miranda IM for communicating with users of the MSN Messenger protocol.

Copyright (c) 2012-2020 Miranda NG team
Copyright (c) 2006-2012 Boris Krasnovskiy.
Copyright (c) 2003-2005 George Hazan.
Copyright (c) 2002-2003 Richard Hughes (original version).

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
#include "m_smileyadd.h"

const char sttVoidUid[] = "{00000000-0000-0000-0000-000000000000}";

void CMsnProto::Lists_Uninit(void)
{
	Lists_Wipe();
}

void CMsnProto::Lists_Wipe(void)
{
	mir_cslock lck(m_csLists);
	m_arContacts.destroy();
}

bool CMsnProto::Lists_IsInList(int list, const char* email)
{
	mir_cslock lck(m_csLists);

	MsnContact *p = m_arContacts.find((MsnContact*)&email);
	if (p == nullptr)
		return false;
	if (list == -1)
		return true;
	return (p->list & list) == list;
}

MsnContact* CMsnProto::Lists_Get(const char* email)
{
	mir_cslock lck(m_csLists);
	return m_arContacts.find((MsnContact*)&email);
}

MsnContact* CMsnProto::Lists_Get(MCONTACT hContact)
{
	mir_cslock lck(m_csLists);

	for (auto &it : m_arContacts)
		if (it->hContact == hContact)
			return it;

	return nullptr;
}

MsnPlace* CMsnProto::Lists_GetPlace(const char* wlid)
{
	char *szEmail, *szInst;
	parseWLID(NEWSTR_ALLOCA(wlid), nullptr, &szEmail, &szInst);

	return Lists_GetPlace(szEmail, szInst);
}

MsnPlace* CMsnProto::Lists_GetPlace(const char* szEmail, const char *szInst)
{
	mir_cslock lck(m_csLists);

	if (szInst == nullptr)
		szInst = (char*)sttVoidUid;

	MsnContact* p = m_arContacts.find((MsnContact*)&szEmail);
	if (p == nullptr)
		return nullptr;

	return p->places.find((MsnPlace*)&szInst);
}

MsnPlace* CMsnProto::Lists_AddPlace(const char* email, const char* id, unsigned cap1, unsigned cap2)
{
	mir_cslock lck(m_csLists);

	MsnContact *p = m_arContacts.find((MsnContact*)&email);
	if (p == nullptr)
		return nullptr;

	MsnPlace *pl = p->places.find((MsnPlace*)&id);
	if (pl == nullptr) {
		pl = new MsnPlace;
		pl->id = mir_strdup(id);
		pl->cap1 = cap1;
		pl->cap2 = cap2;
		pl->client = 11;
		*pl->szClientVer = 0;
		pl->p2pMsgId = 0;
		pl->p2pPktNum = 0;
		p->places.insert(pl);
	}

	return pl;
}

MsnContact* CMsnProto::Lists_GetNext(int &i)
{
	mir_cslock lck(m_csLists);

	MsnContact *p = nullptr;
	while (p == nullptr && ++i < m_arContacts.getCount())
		if (m_arContacts[i].hContact)
			p = &m_arContacts[i];

	return p;
}

int CMsnProto::Lists_GetMask(const char* email)
{
	mir_cslock lck(m_csLists);

	MsnContact *p = m_arContacts.find((MsnContact*)&email);
	return p ? p->list : 0;
}

int CMsnProto::Lists_GetNetId(const char* email)
{
	if (email[0] == 0) return NETID_UNKNOWN;

	mir_cslock lck(m_csLists);

	MsnContact *p = m_arContacts.find((MsnContact*)&email);
	return p ? p->netId : NETID_UNKNOWN;
}

int CMsnProto::Lists_Add(int list, int netId, const char* email, MCONTACT hContact, const char* nick, const char* invite)
{
	mir_cslock lck(m_csLists);

	MsnContact* p = m_arContacts.find((MsnContact*)&email);
	if (p == nullptr) {
		p = new MsnContact;
		p->list = list;
		p->netId = netId;
		p->email = _strlwr(mir_strdup(email));
		p->invite = mir_strdup(invite);
		p->nick = mir_strdup(nick);
		p->hContact = hContact;
		p->p2pMsgId = p->cap1 = p->cap2 = 0;
		m_arContacts.insert(p);
	}
	else {
		p->list |= list;
		if (invite) replaceStr(p->invite, invite);
		if (hContact) p->hContact = hContact;
		if (list & LIST_FL) p->netId = netId;
		if (p->netId == NETID_UNKNOWN && netId != NETID_UNKNOWN)
			p->netId = netId;
	}
	return p->list;
}

void CMsnProto::Lists_Remove(int list, const char* email)
{
	mir_cslock lck(m_csLists);

	int i = m_arContacts.getIndex((MsnContact*)&email);
	if (i != -1) {
		MsnContact &p = m_arContacts[i];
		p.list &= ~list;
		if (list & LIST_PL) { mir_free(p.invite); p.invite = nullptr; }
		if (p.list == 0 && p.hContact == NULL)
			m_arContacts.remove(i);
	}
}


void CMsnProto::Lists_Populate(void)
{
	MCONTACT hContact = db_find_first(m_szModuleName);
	while (hContact) {
		MCONTACT hNext = db_find_next(hContact, m_szModuleName);
		char szEmail[MSN_MAX_EMAIL_LEN] = "";
		if (db_get_static(hContact, m_szModuleName, "wlid", szEmail, sizeof(szEmail))) {
			if (db_get_static(hContact, m_szModuleName, "e-mail", szEmail, sizeof(szEmail)) == 0)
				setString(hContact, "wlid", szEmail);
		}
		if (szEmail[0]) {
			bool localList = getByte(hContact, "LocalList", 0) != 0;
			int netId = getWord(hContact, "netId", localList?NETID_MSN:NETID_UNKNOWN);
			if (localList)
				Lists_Add(LIST_LL, netId, szEmail, hContact);
			else
				Lists_Add(0, netId, szEmail, hContact);
		}
		else if (!isChatRoom(hContact)) db_delete_contact(hContact);
		hContact = hNext;
	}
}

void CMsnProto::MSN_CleanupLists(void)
{
	for (auto &it : m_arContacts.rev_iter()) {
		if (it->list & LIST_FL)
			MSN_SetContactDb(it->hContact, it->email);

		if (it->list & LIST_PL) {
			if (it->list & (LIST_AL | LIST_BL))
				MSN_AddUser(NULL, it->email, it->netId, LIST_PL + LIST_REMOVE);
			else
				MSN_AddAuthRequest(it->email, it->nick, it->invite);
		}

		if (it->hContact && !(it->list & (LIST_LL | LIST_FL | LIST_PL)) && it->list != LIST_RL) {
			int count = db_event_count(it->hContact);
			if (count) {
				wchar_t text[256];
				wchar_t *sze = mir_a2u(it->email);
				mir_snwprintf(text, TranslateT("Contact %s has been removed from the server.\nWould you like to keep it as \"Local Only\" contact to preserve history?"), sze);
				mir_free(sze);

				wchar_t title[128];
				mir_snwprintf(title, TranslateT("%s protocol"), m_tszUserName);

				if (MessageBox(nullptr, text, title, MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND) == IDYES) {
					MSN_AddUser(it->hContact, it->email, 0, LIST_LL);
					setByte(it->hContact, "LocalList", 1);
					continue;
				}
			}

			if (!(it->list & (LIST_LL | LIST_FL))) {
				db_delete_contact(it->hContact);
				it->hContact = NULL;
			}
		}

		if (it->list & (LIST_LL | LIST_FL) && it->hContact) {
			wchar_t path[MAX_PATH];
			MSN_GetCustomSmileyFileName(it->hContact, path, _countof(path), "", 0);
			if (path[0]) {
				SMADD_CONT cont;
				cont.cbSize = sizeof(SMADD_CONT);
				cont.hContact = it->hContact;
				cont.type = 0;
				cont.path = path;

				CallService(MS_SMILEYADD_LOADCONTACTSMILEYS, 0, (LPARAM)&cont);
			}
		}
	}
}

void CMsnProto::MSN_CreateContList(void)
{
	bool *used = (bool*)mir_calloc(m_arContacts.getCount()*sizeof(bool));

	CMStringA cxml;

	cxml.AppendFormat("<ml l=\"%d\">", MyOptions.netId == NETID_MSN?1:0);
	{
		mir_cslock lck(m_csLists);

		for (int i = 0; i < m_arContacts.getCount(); i++) {
			if (used[i]) continue;

			const char* lastds = strchr(m_arContacts[i].email, '@');
			bool newdom = true;

			for (int j = 0; j < m_arContacts.getCount(); j++) {
				if (used[j]) continue;

				const MsnContact& C = m_arContacts[j];
				if (C.list == LIST_RL || C.list == LIST_PL || C.list == LIST_LL) {
					used[j] = true;
					continue;
				}

				const char *dom = strchr(C.email, '@');
				if (dom == nullptr && lastds == nullptr) {
					if (newdom) {
						cxml.Append("<skp>");
						newdom = false;
					}
					int list = C.list & ~(LIST_RL | LIST_LL);
					list = LIST_FL | LIST_AL; /* Seems to be always 3 in Skype... */
					cxml.AppendFormat("<c n=\"%s\" t=\"%d\"><s l=\"%d\" n=\"PE\"/><s l=\"%d\" n=\"IM\"/><s l=\"%d\" n=\"SKP\"/><s l=\"%d\" n=\"PUB\"/></c>", C.email, C.netId, list, list, list, list);
					used[j] = true;
				}
			}
			if (!newdom)
				cxml.Append(lastds ? "</d>" : "</skp>");
		}
	}

	cxml.Append("</ml>");
	msnNsThread->sendPacketPayload("PUT", "MSGR\\CONTACTS", "%s", cxml.c_str());

	if (msnP24Ver > 1)
		msnNsThread->sendPacketPayload("PUT", "MSGR\\SUBSCRIPTIONS", "<subscribe><presence><buddies><all /></buddies></presence><messaging><im /><conversations /></messaging><notifications><partners>%s<partner>ABCH</partner></partners></notifications></subscribe>",
		MyOptions.netId==NETID_MSN?"<partner>Hotmail</partner>":"");

	mir_free(used);
}

/////////////////////////////////////////////////////////////////////////////////////////
// MSN Server List Manager dialog procedure

static void AddPrivacyListEntries(HWND hwndList, CMsnProto *proto)
{
	CLCINFOITEM cii = { 0 };
	cii.cbSize = sizeof(cii);
	cii.flags = CLCIIF_BELOWCONTACTS;

	// Delete old info
	HANDLE hItem = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_ROOT, 0);
	while (hItem) {
		HANDLE hItemNext = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXT, (LPARAM)hItem);

		if (IsHContactInfo(hItem))
			SendMessage(hwndList, CLM_DELETEITEM, (WPARAM)hItem, 0);

		hItem = hItemNext;
	}

	// Add new info
	for (auto &cont : proto->m_arContacts) {
		if (!(cont->list & (LIST_FL | LIST_LL))) {
			cii.pszText = (wchar_t*)cont->email;
			hItem = (HANDLE)SendMessage(hwndList, CLM_ADDINFOITEMA, 0, (LPARAM)&cii);

			SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(0, (cont->list & LIST_LL) ? 1 : 0));
			SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(1, (cont->list & LIST_FL) ? 2 : 0));
			SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(2, (cont->list & LIST_AL) ? 3 : 0));
			SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(3, (cont->list & LIST_BL) ? 4 : 0));
			SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(4, (cont->list & LIST_RL) ? 5 : 0));
		}
	}
}

static void SetContactIcons(MCONTACT hItem, HWND hwndList, CMsnProto* proto)
{
	if (!proto->MSN_IsMyContact(hItem)) {
		SendMessage(hwndList, CLM_DELETEITEM, (WPARAM)hItem, 0);
		return;
	}

	char szEmail[MSN_MAX_EMAIL_LEN];
	if (db_get_static(hItem, proto->m_szModuleName, "wlid", szEmail, sizeof(szEmail)) && 
		db_get_static(hItem, proto->m_szModuleName, "e-mail", szEmail, sizeof(szEmail))) {
		SendMessage(hwndList, CLM_DELETEITEM, (WPARAM)hItem, 0);
		return;
	}

	DWORD dwMask = proto->Lists_GetMask(szEmail);
	SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(0, (dwMask & LIST_LL) ? 1 : 0));
	SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(1, (dwMask & LIST_FL) ? 2 : 0));
	SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(2, (dwMask & LIST_AL) ? 3 : 0));
	SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(3, (dwMask & LIST_BL) ? 4 : 0));
	SendMessage(hwndList, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(4, (dwMask & LIST_RL) ? 5 : 0));
}

static void SetAllContactIcons(MCONTACT hItem, HWND hwndList, CMsnProto* proto)
{
	if (hItem == NULL)
		hItem = (MCONTACT)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_ROOT, 0);

	while (hItem) {
		MCONTACT hItemN = (MCONTACT)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXT, (LPARAM)hItem);

		if (IsHContactGroup(hItem)) {
			MCONTACT hItemT = (MCONTACT)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_CHILD, (LPARAM)hItem);
			if (hItemT)
				SetAllContactIcons(hItemT, hwndList, proto);
		}
		else if (IsHContactContact(hItem))
			SetContactIcons(hItem, hwndList, proto);

		hItem = hItemN;
	}
}

static void SaveListItem(MCONTACT hContact, const char* szEmail, int list, int iPrevValue, int iNewValue, CMsnProto* proto)
{
	if (iPrevValue == iNewValue)
		return;

	if (iNewValue == 0) {
		if (list & LIST_FL) {
			DeleteParam param = { proto, hContact };
			DialogBoxParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_DELETECONTACT), nullptr, DlgDeleteContactUI, (LPARAM)&param);
			return;
		}

		list |= LIST_REMOVE;
	}

	proto->MSN_AddUser(hContact, szEmail, proto->Lists_GetNetId(szEmail), list);
}

static void SaveSettings(MCONTACT hItem, HWND hwndList, CMsnProto* proto)
{
	if (hItem == NULL)
		hItem = (MCONTACT)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_ROOT, 0);

	while (hItem) {
		if (IsHContactGroup(hItem)) {
			MCONTACT hItemT = (MCONTACT)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_CHILD, (LPARAM)hItem);
			if (hItemT)
				SaveSettings(hItemT, hwndList, proto);
		}
		else {
			char szEmail[MSN_MAX_EMAIL_LEN];

			if (IsHContactContact(hItem)) {
				if (db_get_static(hItem, proto->m_szModuleName, "wlid", szEmail, sizeof(szEmail)) &&
					db_get_static(hItem, proto->m_szModuleName, "e-mail", szEmail, sizeof(szEmail)))
					continue;
			}
			else if (IsHContactInfo(hItem)) {
				wchar_t buf[MSN_MAX_EMAIL_LEN];
				SendMessage(hwndList, CLM_GETITEMTEXT, (WPARAM)hItem, (LPARAM)buf);
				WideCharToMultiByte(CP_ACP, 0, buf, -1, szEmail, sizeof(szEmail), nullptr, nullptr);

			}

			int dwMask = proto->Lists_GetMask(szEmail);
			SaveListItem(hItem, szEmail, LIST_LL, (dwMask & LIST_LL) ? 1 : 0, SendMessage(hwndList, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(0, 0)), proto);
			SaveListItem(hItem, szEmail, LIST_FL, (dwMask & LIST_FL) ? 2 : 0, SendMessage(hwndList, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(1, 0)), proto);
			SaveListItem(hItem, szEmail, LIST_AL, (dwMask & LIST_AL) ? 3 : 0, SendMessage(hwndList, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(2, 0)), proto);
			SaveListItem(hItem, szEmail, LIST_BL, (dwMask & LIST_BL) ? 4 : 0, SendMessage(hwndList, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(3, 0)), proto);

			int newMask = proto->Lists_GetMask(szEmail);
			int xorMask = newMask ^ dwMask;

			if (xorMask && newMask & (LIST_FL | LIST_LL)) {
				MCONTACT hContact = IsHContactInfo(hItem) ? proto->MSN_HContactFromEmail(szEmail, szEmail, true, false) : hItem;
				proto->MSN_SetContactDb(hContact, szEmail);
			}

			if (xorMask & (LIST_FL | LIST_LL) && !(newMask & (LIST_FL | LIST_LL))) {
				if (!IsHContactInfo(hItem)) {
					db_delete_contact(hItem);
					MsnContact* msc = proto->Lists_Get(szEmail);
					if (msc) msc->hContact = NULL;
				}
			}
		}
		hItem = (MCONTACT)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXT, (LPARAM)hItem);
	}
}

INT_PTR CALLBACK DlgProcMsnServLists(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	CMsnProto *proto = (CMsnProto*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	NMCLISTCONTROL *nmc;

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		{
			SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);

			HIMAGELIST hIml = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_MASK | ILC_COLOR32, 5, 5);
			ImageList_AddSkinIcon(hIml, SKINICON_OTHER_SMALLDOT);

			HICON hIcon = g_plugin.getIcon(IDI_LIST_LC);
			ImageList_AddIcon(hIml, hIcon);
			SendDlgItemMessage(hwndDlg, IDC_ICON_LC, STM_SETICON, (WPARAM)hIcon, 0);

			hIcon = g_plugin.getIcon(IDI_LIST_FL);
			ImageList_AddIcon(hIml, hIcon);
			SendDlgItemMessage(hwndDlg, IDC_ICON_FL, STM_SETICON, (WPARAM)hIcon, 0);

			hIcon = g_plugin.getIcon(IDI_LIST_AL);
			ImageList_AddIcon(hIml, hIcon);
			SendDlgItemMessage(hwndDlg, IDC_ICON_AL, STM_SETICON, (WPARAM)hIcon, 0);

			hIcon = g_plugin.getIcon(IDI_LIST_BL);
			ImageList_AddIcon(hIml, hIcon);
			SendDlgItemMessage(hwndDlg, IDC_ICON_BL, STM_SETICON, (WPARAM)hIcon, 0);

			hIcon = g_plugin.getIcon(IDI_LIST_RL);
			ImageList_AddIcon(hIml, hIcon);
			SendDlgItemMessage(hwndDlg, IDC_ICON_RL, STM_SETICON, (WPARAM)hIcon, 0);

			HWND hwndList = GetDlgItem(hwndDlg, IDC_LIST);

			SendMessage(hwndList, CLM_SETEXTRAIMAGELIST, 0, (LPARAM)hIml);
			SendMessage(hwndList, CLM_SETEXTRACOLUMNS, 5, 0);

			EnableWindow(hwndList, ((CMsnProto*)lParam)->msnLoggedIn);
		}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_LISTREFRESH) {
			HWND hwndList = GetDlgItem(hwndDlg, IDC_LIST);
			SendMessage(hwndList, CLM_AUTOREBUILD, 0, 0);

			EnableWindow(hwndList, proto->msnLoggedIn);
		}
		break;

	case WM_NOTIFY:
		nmc = (NMCLISTCONTROL*)lParam;
		if (nmc->hdr.idFrom == 0 && nmc->hdr.code == (unsigned)PSN_APPLY) {
			HWND hwndList = GetDlgItem(hwndDlg, IDC_LIST);
			SaveSettings(NULL, hwndList, proto);
			SendMessage(hwndList, CLM_AUTOREBUILD, 0, 0);
			EnableWindow(hwndList, proto->msnLoggedIn);
		}
		else if (nmc->hdr.idFrom == IDC_LIST) {
			switch (nmc->hdr.code) {
			case CLN_NEWCONTACT:
				if ((nmc->flags & (CLNF_ISGROUP | CLNF_ISINFO)) == 0)
					SetContactIcons((UINT_PTR)nmc->hItem, nmc->hdr.hwndFrom, proto);
				break;

			case CLN_LISTREBUILT:
				AddPrivacyListEntries(nmc->hdr.hwndFrom, proto);
				SetAllContactIcons(NULL, nmc->hdr.hwndFrom, proto);
				break;

			case NM_CLICK:
				// Make sure we have an extra column, also we can't change RL list
				if (nmc->iColumn == -1 || nmc->iColumn == 4)
					break;

				// Find clicked item
				DWORD hitFlags;
				HANDLE hItem = (HANDLE)SendMessage(nmc->hdr.hwndFrom, CLM_HITTEST, (WPARAM)&hitFlags, MAKELPARAM(nmc->pt.x, nmc->pt.y));
				if (hItem == nullptr || !(IsHContactContact(hItem) || IsHContactInfo(hItem)))
					break;

				// It was not our extended icon
				if (!(hitFlags & CLCHT_ONITEMEXTRA))
					break;

				// Get image in clicked column (0=none, 1=LL, 2=FL, 3=AL, 4=BL, 5=RL)
				int iImage = SendMessage(nmc->hdr.hwndFrom, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(nmc->iColumn, 0));
				iImage = iImage ? 0 : nmc->iColumn + 1;

				SendMessage(nmc->hdr.hwndFrom, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(nmc->iColumn, iImage));
				if (iImage && SendMessage(nmc->hdr.hwndFrom, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(nmc->iColumn ^ 1, 0)) != EMPTY_EXTRA_ICON)
					if (nmc->iColumn == 2 || nmc->iColumn == 3)
						SendMessage(nmc->hdr.hwndFrom, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(nmc->iColumn ^ 1, 0));

				// Activate Apply button
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;
			}
		}
		break;

	case WM_DESTROY:
		HIMAGELIST hIml = (HIMAGELIST)SendDlgItemMessage(hwndDlg, IDC_LIST, CLM_GETEXTRAIMAGELIST, 0, 0);
		ImageList_Destroy(hIml);
		g_plugin.releaseIcon(IDI_LIST_FL);
		g_plugin.releaseIcon(IDI_LIST_AL);
		g_plugin.releaseIcon(IDI_LIST_BL);
		g_plugin.releaseIcon(IDI_LIST_RL);
		break;
	}

	return FALSE;
}
