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

#include <commdlg.h>


/////////////////////////////////////////////////////////////////////////////////////////
// Icons init

static IconItem iconList[] =
{
	{	LPGEN("Protocol icon"),    "main",        IDI_MSN        },
	{	LPGEN("Hotmail Inbox"),    "inbox",       IDI_INBOX      },
	{	LPGEN("Profile"),          "profile",     IDI_PROFILE    },
	{	LPGEN("MSN Services"),     "services",    IDI_SERVICES   },
	{	LPGEN("Block user"),       "block",       IDI_MSNBLOCK   },
	{	LPGEN("Invite to chat"),   "invite",      IDI_INVITE     },
	{	LPGEN("Start Netmeeting"), "netmeeting",  IDI_NETMEETING },
	{	LPGEN("Contact list"),     "list_fl",     IDI_LIST_FL    },
	{	LPGEN("Allowed list"),     "list_al",     IDI_LIST_AL    },
	{	LPGEN("Blocked list"),     "list_bl",     IDI_LIST_BL    },
	{	LPGEN("Relative list"),    "list_rl",     IDI_LIST_RL    },
	{	LPGEN("Local list"),       "list_lc",     IDI_LIST_LC    },
};

void MsnInitIcons(void)
{
	g_plugin.registerIcon("Protocols/MSN", iconList, "MSN");
}

/////////////////////////////////////////////////////////////////////////////////////////
// MSN Options dialog procedure

static INT_PTR CALLBACK DlgProcMsnOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		{
			SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
			CMsnProto* proto = (CMsnProto*)lParam;

			SetDlgItemTextA(hwndDlg, IDC_HANDLE, proto->MyOptions.szEmail);

			char szPassword[100];
			if (!db_get_static(NULL, proto->m_szModuleName, "Password", szPassword, sizeof(szPassword))) {
				szPassword[99] = 0;
				SetDlgItemTextA(hwndDlg, IDC_PASSWORD, szPassword);
			}
			SendDlgItemMessage(hwndDlg, IDC_PASSWORD, EM_SETLIMITTEXT, 99, 0);

			HWND wnd = GetDlgItem(hwndDlg, IDC_HANDLE2);
			DBVARIANT dbv;
			if (!proto->getWString("Nick", &dbv)) {
				SetWindowText(wnd, dbv.pwszVal);
				db_free(&dbv);
			}
			EnableWindow(wnd, proto->msnLoggedIn);
			EnableWindow(GetDlgItem(hwndDlg, IDC_MOBILESEND), proto->msnLoggedIn &&
				proto->getByte("MobileEnabled", 0) && proto->getByte("MobileAllowed", 0));

			CheckDlgButton(hwndDlg, IDC_MOBILESEND, proto->getByte("MobileAllowed", 0) ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_SENDFONTINFO, proto->getByte("SendFontInfo", 1) ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_MANAGEGROUPS, proto->getByte("ManageServer", 1) ? BST_CHECKED : BST_UNCHECKED);

			int tValue = proto->getByte("RunMailerOnHotmail", 0);
			CheckDlgButton(hwndDlg, IDC_RUN_APP_ON_HOTMAIL, tValue ? BST_CHECKED : BST_UNCHECKED);
			EnableWindow(GetDlgItem(hwndDlg, IDC_MAILER_APP), tValue);
			EnableWindow(GetDlgItem(hwndDlg, IDC_ENTER_MAILER_APP), tValue);

			char tBuffer[MAX_PATH];
			if (!db_get_static(NULL, proto->m_szModuleName, "MailerPath", tBuffer, sizeof(tBuffer)))
				SetDlgItemTextA(hwndDlg, IDC_MAILER_APP, tBuffer);

			if (!proto->msnLoggedIn) {
				EnableWindow(GetDlgItem(hwndDlg, IDC_MANAGEGROUPS), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_DISABLE_ANOTHER_CONTACTS), FALSE);
			}
			else CheckDlgButton(hwndDlg, IDC_DISABLE_ANOTHER_CONTACTS, proto->msnOtherContactsBlocked ? BST_CHECKED : BST_UNCHECKED);
		}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_NEWMSNACCOUNTLINK) {
			Utils_OpenUrl("https://signup.live.com");
			return TRUE;
		}

		if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == GetFocus()) {
			switch (LOWORD(wParam)) {
			case IDC_HANDLE:        case IDC_PASSWORD: case IDC_HANDLE2:
			case IDC_GATEWAYSERVER: case IDC_YOURHOST: case IDC_DIRECTSERVER:
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			}
		}

		if (HIWORD(wParam) == BN_CLICKED) {
			switch (LOWORD(wParam)) {
			case IDC_SENDFONTINFO:
			case IDC_DISABLE_ANOTHER_CONTACTS:
			case IDC_MOBILESEND:
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;

			case IDC_MANAGEGROUPS:
				if (IsDlgButtonChecked(hwndDlg, IDC_MANAGEGROUPS)) {
					if (IDYES == MessageBox(hwndDlg,
						TranslateT("Server groups import may change your contact list layout after next login. Do you want to upload your groups to the server?"),
						TranslateT("MSN Protocol"), MB_YESNOCANCEL)) {
						CMsnProto* proto = (CMsnProto*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
						proto->MSN_UploadServerGroups(nullptr);
					}
				}
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;

			case IDC_RUN_APP_ON_HOTMAIL:
				{
					BOOL tIsChosen = IsDlgButtonChecked(hwndDlg, IDC_RUN_APP_ON_HOTMAIL);
					EnableWindow(GetDlgItem(hwndDlg, IDC_MAILER_APP), tIsChosen);
					EnableWindow(GetDlgItem(hwndDlg, IDC_ENTER_MAILER_APP), tIsChosen);
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				}
				break;

			case IDC_ENTER_MAILER_APP:
				char szFile[MAX_PATH + 2];
				{
					HWND tEditField = GetDlgItem(hwndDlg, IDC_MAILER_APP);

					GetWindowTextA(tEditField, szFile, _countof(szFile));

					size_t tSelectLen = 0;

					if (szFile[0] == '\"') {
						char* p = strchr(szFile + 1, '\"');
						if (p != nullptr) {
							*p = '\0';
							memmove(szFile, szFile + 1, mir_strlen(szFile));
							tSelectLen += 2;
							goto LBL_Continue;
						}
					}

					char* p = strchr(szFile, ' ');
					if (p != nullptr) *p = '\0';
LBL_Continue:
					tSelectLen += mir_strlen(szFile);

					OPENFILENAMEA ofn = { 0 };
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = hwndDlg;
					ofn.nMaxFile = _countof(szFile);
					ofn.lpstrFile = szFile;
					ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
					if (GetOpenFileNameA(&ofn) != TRUE)
						break;

					if (strchr(szFile, ' ') != nullptr) {
						char tmpBuf[MAX_PATH + 2];
						mir_snprintf(tmpBuf, "\"%s\"", szFile);
						mir_strcpy(szFile, tmpBuf);
					}

					SendMessage(tEditField, EM_SETSEL, 0, tSelectLen);
					SendMessageA(tEditField, EM_REPLACESEL, TRUE, LPARAM(szFile));
					SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				}
			}
		}
		break;

	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->code == (UINT)PSN_APPLY) {
			bool reconnectRequired = false;
			wchar_t screenStr[MAX_PATH];
			char  password[100], szEmail[MSN_MAX_EMAIL_LEN];
			DBVARIANT dbv;

			CMsnProto* proto = (CMsnProto*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

			GetDlgItemTextA(hwndDlg, IDC_HANDLE, szEmail, _countof(szEmail));
			if (mir_strcmp(_strlwr(szEmail), proto->MyOptions.szEmail)) {
				reconnectRequired = true;
				mir_strcpy(proto->MyOptions.szEmail, szEmail);
				proto->setString("e-mail", szEmail);
				proto->setString("wlid", szEmail);
				proto->setDword("netId", (proto->MyOptions.netId = proto->GetMyNetID()));
			}

			GetDlgItemTextA(hwndDlg, IDC_PASSWORD, password, _countof(password));
			if (!proto->getString("Password", &dbv)) {
				if (mir_strcmp(password, dbv.pszVal)) {
					reconnectRequired = true;
					proto->setString("Password", password);
				}
				db_free(&dbv);
			}
			else {
				reconnectRequired = true;
				proto->setString("Password", password);
			}

			proto->setByte("SendFontInfo", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_SENDFONTINFO));
			proto->setByte("RunMailerOnHotmail", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_RUN_APP_ON_HOTMAIL));
			proto->setByte("ManageServer", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_MANAGEGROUPS));

			GetDlgItemText(hwndDlg, IDC_MAILER_APP, screenStr, _countof(screenStr));
			proto->setWString("MailerPath", screenStr);

			if (reconnectRequired && proto->msnLoggedIn)
				MessageBox(hwndDlg,
				TranslateT("These changes will take effect the next time you connect to the MSN Messenger network."),
				TranslateT("MSN options"), MB_OK);

			proto->LoadOptions();
			return TRUE;
		}
		break;
	}

	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// MSN Connection Options dialog procedure

static INT_PTR CALLBACK DlgProcMsnConnOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DBVARIANT dbv;

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		{
			SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
			CMsnProto* proto = (CMsnProto*)lParam;

			if (!proto->getString("DirectServer", &dbv)) {
				SetDlgItemTextA(hwndDlg, IDC_DIRECTSERVER, dbv.pszVal);
				db_free(&dbv);
			}
			else SetDlgItemTextA(hwndDlg, IDC_DIRECTSERVER, MSN_DEFAULT_LOGIN_SERVER);

			if (!proto->getString("GatewayServer", &dbv)) {
				SetDlgItemTextA(hwndDlg, IDC_GATEWAYSERVER, dbv.pszVal);
				db_free(&dbv);
			}
			else SetDlgItemTextA(hwndDlg, IDC_GATEWAYSERVER, MSN_DEFAULT_GATEWAY);

			CheckDlgButton(hwndDlg, IDC_SLOWSEND, proto->getByte("SlowSend", 0) ? BST_CHECKED : BST_UNCHECKED);

			SendDlgItemMessage(hwndDlg, IDC_HOSTOPT, CB_ADDSTRING, 0, (LPARAM)TranslateT("Automatically obtain host/port"));
			SendDlgItemMessage(hwndDlg, IDC_HOSTOPT, CB_ADDSTRING, 0, (LPARAM)TranslateT("Manually specify host/port"));
			SendDlgItemMessage(hwndDlg, IDC_HOSTOPT, CB_ADDSTRING, 0, (LPARAM)TranslateT("Disable"));

			unsigned gethst = proto->getByte("AutoGetHost", 1);
			if (gethst < 2) gethst = !gethst;

			char ipaddr[256] = "";
			if (gethst == 1)
				if (db_get_static(NULL, proto->m_szModuleName, "YourHost", ipaddr, sizeof(ipaddr)))
					gethst = 0;

			if (gethst == 0)
				strncpy_s(ipaddr, (proto->msnLoggedIn ? proto->MyConnection.GetMyExtIPStr() : ""), _TRUNCATE);

			SendDlgItemMessage(hwndDlg, IDC_HOSTOPT, CB_SETCURSEL, gethst, 0);
			if (ipaddr[0])
				SetDlgItemTextA(hwndDlg, IDC_YOURHOST, ipaddr);
			else
				SetDlgItemText(hwndDlg, IDC_YOURHOST, TranslateT("IP info available only after login"));
			EnableWindow(GetDlgItem(hwndDlg, IDC_YOURHOST), gethst == 1);
		}
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_RESETSERVER:
			SetDlgItemTextA(hwndDlg, IDC_DIRECTSERVER, MSN_DEFAULT_LOGIN_SERVER);
			SetDlgItemTextA(hwndDlg, IDC_GATEWAYSERVER, MSN_DEFAULT_GATEWAY);
			SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			break;
		}

		if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == GetFocus())
			switch (LOWORD(wParam)) {
			case IDC_DIRECTSERVER:
			case IDC_GATEWAYSERVER:
			case IDC_YOURHOST:
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
		}

		if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_HOSTOPT) {
			unsigned gethst = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
			EnableWindow(GetDlgItem(hwndDlg, IDC_YOURHOST), gethst == 1);
			SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
		}

		if (HIWORD(wParam) == BN_CLICKED) {
			switch (LOWORD(wParam)) {
			case IDC_SLOWSEND:
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				break;
			}
		}
		break;

	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->code == (UINT)PSN_APPLY) {
			char str[MAX_PATH];

			CMsnProto* proto = (CMsnProto*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

			GetDlgItemTextA(hwndDlg, IDC_DIRECTSERVER, str, _countof(str));
			if (mir_strcmp(str, MSN_DEFAULT_LOGIN_SERVER))
				proto->setString("DirectServer", str);
			else
				proto->delSetting("DirectServer");

			GetDlgItemTextA(hwndDlg, IDC_GATEWAYSERVER, str, _countof(str));
			if (mir_strcmp(str, MSN_DEFAULT_GATEWAY))
				proto->setString("GatewayServer", str);
			else
				proto->delSetting("GatewayServer");

			proto->setByte("SlowSend", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_SLOWSEND));
			if (proto->getByte("SlowSend", FALSE)) {
				if (db_get_dw(0, "SRMsg", "MessageTimeout", 60000) < 60000 ||
					db_get_dw(0, "SRMM", "MessageTimeout", 60000) < 60000) {
					MessageBox(nullptr, TranslateT("MSN Protocol requires message timeout to be not less then 60 sec. Correct the timeout value."),
						TranslateT("MSN Protocol"), MB_OK | MB_ICONINFORMATION);
				}
			}
			
			unsigned gethst = SendDlgItemMessage(hwndDlg, IDC_HOSTOPT, CB_GETCURSEL, 0, 0);
			if (gethst < 2) gethst = !gethst;
			proto->setByte("AutoGetHost", (BYTE)gethst);

			if (gethst == 0) {
				GetDlgItemTextA(hwndDlg, IDC_YOURHOST, str, _countof(str));
				proto->setString("YourHost", str);
			}
			else proto->delSetting("YourHost");

			proto->LoadOptions();
			return TRUE;
		}
	}

	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Popup Options Dialog: style, position, color, font...

static INT_PTR CALLBACK DlgProcHotmailPopupOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static bool bEnabled;
	CMsnProto* proto = (CMsnProto*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		bEnabled = false;

		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
		proto = (CMsnProto*)lParam;
		CheckDlgButton(hwndDlg, IDC_DISABLEHOTMAILPOPUP, proto->getByte("DisableHotmail", 0) ? BST_UNCHECKED : BST_CHECKED);
		CheckDlgButton(hwndDlg, IDC_DISABLEHOTMAILTRAY, proto->getByte("DisableHotmailTray", 1) ? BST_UNCHECKED : BST_CHECKED);
		CheckDlgButton(hwndDlg, IDC_DISABLEHOTMAILCL, proto->getByte("DisableHotmailCL", 0) ? BST_UNCHECKED : BST_CHECKED);
		CheckDlgButton(hwndDlg, IDC_DISABLEHOTJUNK, proto->getByte("DisableHotmailJunk", 0) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_NOTIFY_ENDSESSION, proto->getByte("EnableSessionPopup", 0) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_NOTIFY_FIRSTMSG, proto->getByte("EnableDeliveryPopup", 0) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_ERRORS_USING_POPUPS, proto->getByte("ShowErrorsAsPopups", 0) ? BST_CHECKED : BST_UNCHECKED);

		bEnabled = true;
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_DISABLEHOTMAILPOPUP:
		case IDC_DISABLEHOTMAILTRAY:
		case IDC_DISABLEHOTMAILCL:
		case IDC_DISABLEHOTJUNK:
		case IDC_NOTIFY_ENDSESSION:
		case IDC_NOTIFY_FIRSTMSG:
		case IDC_ERRORS_USING_POPUPS:
			if (bEnabled)
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			break;
		}
		break;

	case WM_NOTIFY: //Here we have pressed either the OK or the APPLY button.
		switch (((LPNMHDR)lParam)->idFrom) {
		case 0:
			switch (((LPNMHDR)lParam)->code) {
			case PSN_RESET:
				proto->LoadOptions();
				return TRUE;

			case PSN_APPLY:
				proto->MyOptions.ShowErrorsAsPopups = IsDlgButtonChecked(hwndDlg, IDC_ERRORS_USING_POPUPS) != 0;
				proto->setByte("ShowErrorsAsPopups", proto->MyOptions.ShowErrorsAsPopups);

				proto->setByte("DisableHotmail", IsDlgButtonChecked(hwndDlg, IDC_DISABLEHOTMAILPOPUP) ? FALSE : TRUE);
				proto->setByte("DisableHotmailCL", IsDlgButtonChecked(hwndDlg, IDC_DISABLEHOTMAILCL) ? FALSE : TRUE);
				proto->setByte("DisableHotmailTray", IsDlgButtonChecked(hwndDlg, IDC_DISABLEHOTMAILTRAY) ? FALSE : TRUE);
				proto->setByte("DisableHotmailJunk", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_DISABLEHOTJUNK));
				proto->setByte("EnableDeliveryPopup", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_NOTIFY_FIRSTMSG));
				proto->setByte("EnableSessionPopup", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_NOTIFY_ENDSESSION));

				MCONTACT hContact = proto->MSN_HContactFromEmail(proto->MyOptions.szEmail);
				if (hContact)
					proto->displayEmailCount(hContact);
				return TRUE;
			}
			break;
		}
		break;
	}

	return FALSE;
}

static INT_PTR CALLBACK DlgProcAccMgrUI(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		{
			SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);

			CMsnProto* proto = (CMsnProto*)lParam;
			SetDlgItemTextA(hwndDlg, IDC_HANDLE, proto->MyOptions.szEmail);

			char szPassword[100];
			if (!db_get_static(NULL, proto->m_szModuleName, "Password", szPassword, sizeof(szPassword))) {
				szPassword[99] = 0;
				SetDlgItemTextA(hwndDlg, IDC_PASSWORD, szPassword);
			}
			SendDlgItemMessage(hwndDlg, IDC_PASSWORD, EM_SETLIMITTEXT, 99, 0);

			DBVARIANT dbv;
			if (!proto->getWString("Place", &dbv)) {
				SetDlgItemText(hwndDlg, IDC_PLACE, dbv.pwszVal);
				db_free(&dbv);
			}
		}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_NEWMSNACCOUNTLINK) {
			Utils_OpenUrl("https://signup.live.com");
			return TRUE;
		}

		if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == GetFocus()) {
			switch (LOWORD(wParam)) {
			case IDC_HANDLE:
			case IDC_PASSWORD:
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			}
		}
		break;

	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->code == (UINT)PSN_APPLY) {
			char  password[100], szEmail[MSN_MAX_EMAIL_LEN];
			DBVARIANT dbv;

			CMsnProto* proto = (CMsnProto*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

			GetDlgItemTextA(hwndDlg, IDC_HANDLE, szEmail, _countof(szEmail));
			if (mir_strcmp(szEmail, proto->MyOptions.szEmail)) {
				mir_strcpy(proto->MyOptions.szEmail, szEmail);
				proto->setString("e-mail", szEmail);
				proto->setString("wlid", szEmail);
				proto->setDword("netId", (proto->MyOptions.netId = proto->GetMyNetID()));
			}

			GetDlgItemTextA(hwndDlg, IDC_PASSWORD, password, _countof(password));
			if (!proto->getString("Password", &dbv)) {
				if (mir_strcmp(password, dbv.pszVal))
					proto->setString("Password", password);
				db_free(&dbv);
			}
			else proto->setString("Password", password);

			wchar_t szPlace[64];
			GetDlgItemText(hwndDlg, IDC_PLACE, szPlace, _countof(szPlace));
			if (szPlace[0])
				proto->setWString("Place", szPlace);
			else
				proto->delSetting("Place");

			return TRUE;
		}
		break;
	}

	return FALSE;
}

INT_PTR CALLBACK DlgDeleteContactUI(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
		return TRUE;

	case WM_CLOSE:
		EndDialog(hwndDlg, 0);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {
			int isBlock = IsDlgButtonChecked(hwndDlg, IDC_REMOVEBLOCK);
			int isHot = IsDlgButtonChecked(hwndDlg, IDC_REMOVEHOT);

			DeleteParam *param = (DeleteParam*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

			char szEmail[MSN_MAX_EMAIL_LEN];
			if (!db_get_static(param->hContact, param->proto->m_szModuleName, "wlid", szEmail, sizeof(szEmail)) ||
				!db_get_static(param->hContact, param->proto->m_szModuleName, "e-mail", szEmail, sizeof(szEmail))) {
				param->proto->MSN_AddUser(param->hContact, szEmail, 0, LIST_FL | (isHot ? LIST_REMOVE : LIST_REMOVENH));
				if (isBlock) {
					param->proto->MSN_AddUser(param->hContact, szEmail, 0, LIST_AL | LIST_REMOVE);
					param->proto->MSN_AddUser(param->hContact, szEmail, 0, LIST_BL);
				}
			}
			EndDialog(hwndDlg, 1);
		}
		break;
	}

	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Initialize options pages

INT_PTR CALLBACK DlgProcMsnServLists(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

int CMsnProto::OnOptionsInit(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE odp = {};
	odp.position = -790000000;
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPT_MSN);
	odp.szTitle.w = m_tszUserName;
	odp.szGroup.w = LPGENW("Network");
	odp.szTab.w = LPGENW("Account");
	odp.flags = ODPF_BOLDGROUPS | ODPF_UNICODE | ODPF_DONTTRANSLATE;
	odp.pfnDlgProc = DlgProcMsnOpts;
	odp.dwInitParam = (LPARAM)this;
	g_plugin.addOptions(wParam, &odp);

	odp.szTab.w = LPGENW("Connection");
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPT_MSN_CONN);
	odp.pfnDlgProc = DlgProcMsnConnOpts;
	g_plugin.addOptions(wParam, &odp);

	odp.szTab.w = LPGENW("Server list");
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_LISTSMGR);
	odp.pfnDlgProc = DlgProcMsnServLists;
	g_plugin.addOptions(wParam, &odp);

	odp.szTab.w = LPGENW("Notifications");
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPT_NOTIFY);
	odp.pfnDlgProc = DlgProcHotmailPopupOpts;
	g_plugin.addOptions(wParam, &odp);

	return 0;
}

INT_PTR CMsnProto::SvcCreateAccMgrUI(WPARAM, LPARAM lParam)
{
	return (INT_PTR)CreateDialogParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_ACCMGRUI),
		(HWND)lParam, DlgProcAccMgrUI, (LPARAM)this);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Load resident option values into memory

void CMsnProto::LoadOptions(void)
{
	memset(&MyOptions, 0, sizeof(MyOptions));

	//Popup Options
	MyOptions.ManageServer = getByte("ManageServer", TRUE) != 0;
	MyOptions.ShowErrorsAsPopups = getByte("ShowErrorsAsPopups", TRUE) != 0;
	MyOptions.SlowSend = getByte("SlowSend", FALSE) != 0;
	if (db_get_static(NULL, m_szModuleName, "wlid", MyOptions.szEmail, sizeof(MyOptions.szEmail)))
	{
		if (db_get_static(NULL, m_szModuleName, "e-mail", MyOptions.szEmail, sizeof(MyOptions.szEmail)))
			MyOptions.szEmail[0] = 0;
		else setString("wlid", MyOptions.szEmail);
	}
	_strlwr(MyOptions.szEmail);
	MyOptions.netId = getDword("netId", GetMyNetID());

	if (db_get_static(NULL, m_szModuleName, "MachineGuid", MyOptions.szMachineGuid, sizeof(MyOptions.szMachineGuid))) {
		char* uuid = getNewUuid();
		mir_strcpy(MyOptions.szMachineGuid, uuid);
		setString("MachineGuid", MyOptions.szMachineGuid);
		mir_free(uuid);
	}
	mir_strcpy(MyOptions.szMachineGuidP2P, MyOptions.szMachineGuid);
	_strlwr(MyOptions.szMachineGuidP2P);
}
