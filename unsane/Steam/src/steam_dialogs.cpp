﻿#include "common.h"

INT_PTR CALLBACK CSteamProto::GuardProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	GuardParam *guard = (GuardParam*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (message)
	{
	case WM_INITDIALOG:
		TranslateDialogDefault(hwnd);
		{
			guard = (GuardParam*)lParam;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
			// load steam icon
			char iconName[100];
			mir_snprintf(iconName, SIZEOF(iconName), "%s_%s", MODULE, "main");
			SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)Skin_GetIcon(iconName, 16));
			SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)Skin_GetIcon(iconName, 32));
		}
		Utils_RestoreWindowPosition(hwnd, NULL, "STEAM", "GuardWindow");
		return TRUE;

	case WM_CLOSE:
		Skin_ReleaseIcon((HICON)SendMessage(hwnd, WM_SETICON, ICON_BIG, 0));
		Skin_ReleaseIcon((HICON)SendMessage(hwnd, WM_SETICON, ICON_SMALL, 0));
		Utils_SaveWindowPosition(hwnd, NULL, "STEAM", "GuardWindow");
		EndDialog(hwnd, 0);
		break;

	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDC_GETDOMAIN:
			CallService(MS_UTILS_OPENURL, 0, (LPARAM)guard->domain);
			SetFocus(GetDlgItem(hwnd, IDC_TEXT));
			break;

		case IDOK:
			GetDlgItemTextA(hwnd, IDC_TEXT, guard->code, sizeof(guard->code));
			EndDialog(hwnd, IDOK);
			break;

		case IDCANCEL:
			EndDialog(hwnd, IDCANCEL);
			break;
		}
	}
		break;
	}

	return FALSE;
}

INT_PTR CALLBACK CSteamProto::CaptchaProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CaptchaParam *captcha = (CaptchaParam*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (message)
	{
	case WM_INITDIALOG:
		TranslateDialogDefault(hwnd);
		{
			captcha = (CaptchaParam*)lParam;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);
		}
		return TRUE;

	case WM_CLOSE:
		EndDialog(hwnd, 0);
		break;

	case WM_PAINT:
		{
			FI_INTERFACE *fei = 0;

			INT_PTR result = CALLSERVICE_NOTFOUND;
			if (ServiceExists(MS_IMG_GETINTERFACE))
				result = CallService(MS_IMG_GETINTERFACE, FI_IF_VERSION, (LPARAM)&fei);

			if (fei == NULL || result != S_OK) {
				MessageBox(0, TranslateT("Fatal error, image services not found. Avatar services will be disabled."), TranslateT("Avatar Service"), MB_OK);
				return 0;
			}

			FIMEMORY *stream = fei->FI_OpenMemory(captcha->data, captcha->size);
			FREE_IMAGE_FORMAT fif = fei->FI_GetFileTypeFromMemory(stream, 0);
			FIBITMAP *bitmap = fei->FI_LoadFromMemory(fif, stream, 0);
			fei->FI_CloseMemory(stream);

			PAINTSTRUCT ps;
			HDC hDC = BeginPaint(hwnd, &ps);

			//SetStretchBltMode(hDC, COLORONCOLOR);
			StretchDIBits(
				hDC,
				11, 11,
				fei->FI_GetWidth(bitmap) - 13,
				fei->FI_GetHeight(bitmap),
				0, 0,
				fei->FI_GetWidth(bitmap),
				fei->FI_GetHeight(bitmap),
				fei->FI_GetBits(bitmap),
				fei->FI_GetInfo(bitmap),
				DIB_RGB_COLORS, SRCCOPY);

			fei->FI_Unload(bitmap);
			//fei->FI_DeInitialise();

			EndPaint(hwnd, &ps);
		}
		return 0;

	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
			case IDOK:
				GetDlgItemTextA(hwnd, IDC_TEXT, captcha->text, sizeof(captcha->text));
				EndDialog(hwnd, IDOK);
				break;

			case IDCANCEL:
				EndDialog(hwnd, IDCANCEL);
				break;
			}
		}
		break;
	}

	return FALSE;
}

INT_PTR CALLBACK CSteamProto::MainOptionsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CSteamProto *proto = (CSteamProto*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (message)
	{
	case WM_INITDIALOG:
		TranslateDialogDefault(hwnd);
		{
			proto = (CSteamProto*)lParam;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);

			ptrW username(proto->getWStringA("Username"));
			SetDlgItemText(hwnd, IDC_USERNAME, username);

			ptrA password(proto->getStringA("Password"));
			SetDlgItemTextA(hwnd, IDC_PASSWORD, password);

			ptrW groupName(proto->getWStringA(NULL, "DefaultGroup"));
			SetDlgItemText(hwnd, IDC_GROUP, groupName);
			SendDlgItemMessage(hwnd, IDC_GROUP, EM_LIMITTEXT, 64, 0);

			if (proto->IsOnline())
			{
				EnableWindow(GetDlgItem(hwnd, IDC_USERNAME), FALSE);
				EnableWindow(GetDlgItem(hwnd, IDC_PASSWORD), FALSE);
			}
		}
		return TRUE;

	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
			case IDC_USERNAME:
				if ((HWND)lParam == GetFocus())
				{
					EnableWindow(GetDlgItem(hwnd, IDC_USERNAME), !proto->IsOnline());
					if (HIWORD(wParam) != EN_CHANGE) return 0;
					proto->delSetting("SteamID");
					proto->delSetting("Cookies");
					proto->delSetting("TokenSecret");
					wchar_t username[128];
					GetDlgItemText(hwnd, IDC_USERNAME, username, SIZEOF(username));
					SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				}
				break;

			case IDC_PASSWORD:
				if ((HWND)lParam == GetFocus())
				{
					EnableWindow(GetDlgItem(hwnd, IDC_PASSWORD), !proto->IsOnline());
					if (HIWORD(wParam) != EN_CHANGE) return 0;
					proto->delSetting("Cookie");
					proto->delSetting("TokenSecret");
					char password[128];
					GetDlgItemTextA(hwnd, IDC_PASSWORD, password, SIZEOF(password));
					SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				}
				break;

			case IDC_GROUP:
				{
					if ((HIWORD(wParam) != EN_CHANGE || (HWND)lParam != GetFocus()))
						return 0;
					SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				}
				break;
			}
		}
		break;

	case WM_NOTIFY:
		if (reinterpret_cast<NMHDR*>(lParam)->code == PSN_APPLY)
		{
			if (!proto->IsOnline())
			{
				wchar_t username[128];
				GetDlgItemText(hwnd, IDC_USERNAME, username, SIZEOF(username));
				proto->setWString("Username", username);

				char password[128];
				GetDlgItemTextA(hwnd, IDC_PASSWORD, password, SIZEOF(password));
				proto->setString("Password", password);
			}

			wchar_t groupName[128];
			GetDlgItemText(hwnd, IDC_GROUP, groupName, SIZEOF(groupName));
			if (lstrlen(groupName) > 0)
			{
				proto->setWString(NULL, "DefaultGroup", groupName);
				Clist_CreateGroup(0, groupName);
			}
			else
				proto->delSetting(NULL, "DefaultGroup");

			return TRUE;
		}
		break;
	}

	return FALSE;
}

static WNDPROC oldWndProc = NULL;

LRESULT CALLBACK CSteamProto::BlockListOptionsSubProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_LBUTTONDOWN)
	{
		LVHITTESTINFO hi;
		hi.pt.x = LOWORD(lParam); hi.pt.y = HIWORD(lParam);
		ListView_SubItemHitTest(hwnd, &hi);
		if (hi.iSubItem == 1)
		{
			LVITEM lvi = { 0 };
			lvi.mask = LVIF_IMAGE | LVIF_PARAM;
			lvi.stateMask = -1;
			lvi.iItem = hi.iItem;
			if (ListView_GetItem(hwnd, &lvi))
			{
				MCONTACT hContact = lvi.lParam;

				CSteamProto *ppro = GetContactProtoInstance(hContact);

				ptrA token(ppro->getStringA("TokenSecret"));
				ptrA sessionId(ppro->getStringA("SessionID"));
				ptrA steamId(ppro->getStringA("SteamID"));
				ptrA who(ppro->getStringA(hContact, "SteamID"));

				mir_ptr<SteamWebApi::BlockFriendRequest> request(new SteamWebApi::BlockFriendRequest(token, sessionId, steamId, who));
				NETLIBHTTPREQUEST *response = request->Send(ppro->m_hNetlibUser);
				CallService(MS_NETLIB_FREEHTTPREQUESTSTRUCT, 0, (LPARAM)response);

				if (true /* todo: check unblock contact */)
				{
					ppro->delSetting(hContact, "Block");

					wchar_t *nick = (wchar_t*)CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM)hContact, GCDNF_TCHAR);

					ListView_DeleteItem(hwnd, lvi.iItem);

					int nItem = SendMessage(GetDlgItem(GetParent(hwnd), IDC_CONTACTS), CB_ADDSTRING, 0, (LPARAM)nick);
					SendMessage(GetDlgItem(GetParent(hwnd), IDC_CONTACTS), CB_SETITEMDATA, nItem, hContact);
				}
			}
		}
	}

	return CallWindowProc(oldWndProc, hwnd, msg, wParam, lParam);
}

INT_PTR CALLBACK CSteamProto::BlockListOptionsProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	CSteamProto *ppro = (CSteamProto*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	switch (msg)
	{
	case WM_INITDIALOG:
		if (lParam)
		{
			ppro = (CSteamProto*)lParam;
			SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);

			TranslateDialogDefault(hwndDlg);

			HWND hwndList = GetDlgItem(hwndDlg, IDC_LIST);
			{   // IDC_BM_LIST setup
				oldWndProc = (WNDPROC)SetWindowLongPtr(hwndList, GWLP_WNDPROC, (LONG_PTR)BlockListOptionsSubProc);

				HIMAGELIST hIml = ImageList_Create(16, 16, ILC_MASK | ILC_COLOR32, 4, 0);
				//ImageList_AddIconFromIconLib(hIml, "Skype_contact");
				//ImageList_AddIconFromIconLib(hIml, "Skype_delete");
				ListView_SetImageList(hwndList, hIml, LVSIL_SMALL);

				LVCOLUMN lvc = { 0 };
				lvc.mask = LVCF_WIDTH | LVCF_TEXT;
				//lvc.fmt = LVCFMT_JUSTIFYMASK;
				lvc.pszText = TranslateT("Name");
				lvc.cx = 220; // width of column in pixels
				ListView_InsertColumn(hwndList, 0, &lvc);
				//lvc.fmt = LVCFMT_RIGHT;
				lvc.pszText = L"";
				lvc.cx = 32 - GetSystemMetrics(SM_CXVSCROLL); // width of column in pixels
				ListView_InsertColumn(hwndList, 1, &lvc);

				SendMessage(hwndList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_SUBITEMIMAGES | LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

				if (!ppro->IsOnline())
				{
					EnableWindow(hwndList, FALSE);
					EnableWindow(GetDlgItem(hwndDlg, IDC_CONTACTS), FALSE);
				}
			}

			if (ppro->IsOnline())
			{
				mir_cslock lock(ppro->contact_search_lock);

				SendMessage(GetDlgItem(hwndDlg, IDC_CONTACTS), CB_ADDSTRING, 0, (LPARAM)"");

				for (MCONTACT hContact = db_find_first(ppro->m_szModuleName); hContact; hContact = db_find_next(hContact, ppro->m_szModuleName))
				{
					wchar_t *nick = (wchar_t*)CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM)hContact, GCDNF_TCHAR);

					if (ppro->getBool(hContact, "Block", 0))
					{
						LVITEM lvi = { 0 };
						lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
						lvi.iItem = (int)hContact;
						lvi.iImage = 0;
						//lvi.lParam = (LPARAM)new ContactParam(contact, ppro);
						lvi.lParam = hContact;
						lvi.pszText = nick;
						int iRow = ListView_InsertItem(hwndList, &lvi);

						if (iRow != -1)
						{
							lvi.iItem = iRow;
							lvi.mask = LVIF_IMAGE;
							lvi.iSubItem = 1;
							lvi.iImage = 1;
							ListView_SetItem(hwndList, &lvi);
						}
					}
					else
					{
						int nItem = SendMessage(GetDlgItem(hwndDlg, IDC_CONTACTS), CB_ADDSTRING, 0, (LPARAM)nick);
						SendMessage(GetDlgItem(hwndDlg, IDC_CONTACTS), CB_SETITEMDATA, nItem, hContact);
					}
				}
			}

		}
		break;

	case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
			case IDC_CONTACTS:
				EnableWindow(GetDlgItem(hwndDlg, IDC_BLOCK), TRUE);
				break;

			case IDC_BLOCK:
				{
					int nItem = SendMessage(GetDlgItem(hwndDlg, IDC_CONTACTS), CB_GETCURSEL, 0, 0);

					MCONTACT hContact = (MCONTACT)SendMessage(GetDlgItem(hwndDlg, IDC_CONTACTS), CB_GETITEMDATA, nItem, 0);
					if (!hContact)
						break;

					ptrA token(ppro->getStringA("TokenSecret"));
					ptrA sessionId(ppro->getStringA("SessionID"));
					ptrA steamId(ppro->getStringA("SteamID"));
					ptrA who(ppro->getStringA(hContact, "SteamID"));

					mir_ptr<SteamWebApi::BlockFriendRequest> request(new SteamWebApi::BlockFriendRequest(token, sessionId, steamId, who));
					NETLIBHTTPREQUEST *response = request->Send(ppro->m_hNetlibUser);
					CallService(MS_NETLIB_FREEHTTPREQUESTSTRUCT, 0, (LPARAM)response);

					if (true /*todo: block contact*/)
					{
						wchar_t *nick = (wchar_t*)CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM)hContact, GCDNF_TCHAR);

						LVITEM lvi = { 0 };
						lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
						lvi.iItem = hContact;
						lvi.iImage = 0;
						lvi.lParam = hContact;
						lvi.pszText = nick;
						int iRow = ListView_InsertItem(GetDlgItem(hwndDlg, IDC_LIST), &lvi);

						if (iRow != -1)
						{
							lvi.iItem = iRow;
							lvi.mask = LVIF_IMAGE;
							lvi.iSubItem = 1;
							lvi.iImage = 1;
							ListView_SetItem(::GetDlgItem(hwndDlg, IDC_LIST), &lvi);
						}

						SendMessage(GetDlgItem(hwndDlg, IDC_CONTACTS), CB_DELETESTRING, nItem, 0);
					}
				}
				break;
			}
		}
		break;

	case WM_NOTIFY:
		if (reinterpret_cast<NMHDR*>(lParam)->code == PSN_APPLY && !ppro->IsOnline())
		{
			return TRUE;
		}
		break;

		switch (LOWORD(wParam))
		{
		case IDC_LIST:
			if (((LPNMHDR)lParam)->code == NM_DBLCLK)
			{
				HWND hwndList = ::GetDlgItem(hwndDlg, IDC_BM_LIST);
				int iItem = ListView_GetNextItem(hwndList, -1, LVNI_ALL | LVNI_SELECTED);
				if (iItem < 0) break;
				LVITEM lvi = { 0 };
				lvi.mask = LVIF_PARAM | LVIF_GROUPID;
				lvi.stateMask = -1;
				lvi.iItem = iItem;
				if (ListView_GetItem(hwndList, &lvi))
				{
					if (lvi.iGroupId == 1)
					{
						CallService(MS_MSG_SENDMESSAGE, (WPARAM)lvi.lParam, 0);
					}
				}
			}
		}
		break;
	}
	return FALSE;
}