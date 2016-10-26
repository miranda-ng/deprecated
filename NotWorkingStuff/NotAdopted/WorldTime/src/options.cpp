#include "common.h"
#include "options.h"

bool set_format = false, hide_proto = false;
TCHAR format_string[512], date_format_string[512], clist_format_string[512];

ITEMLIST temp_listbox_items(10);

void fill_timezone_list_control(HWND hwndDlg) {
	
	HWND hw = GetDlgItem(hwndDlg, IDC_LIST_TIMES);
	SendMessage(hw, LB_RESETCONTENT, 0, 0);

	if(IsDlgButtonChecked(hwndDlg, IDC_RAD_ALPHA)) {

		for(int i = 0; i < timezone_list.getCount(); i++) {
			SendMessage(hw, LB_INSERTSTRING, (WPARAM)-1, (LPARAM)timezone_list[i].tcName);
			SendMessage(hw, LB_SETITEMDATA, (WPARAM)i, (LPARAM)timezone_list[i].list_index);
		}
	} else {
		for(int i =0; i < geo_timezone_list.getCount(); ++i) {
			SendMessage(hw, LB_INSERTSTRING, (WPARAM)-1, (LPARAM)timezone_list[i].tcName);
			SendMessage(hw, LB_SETITEMDATA, (WPARAM)i, (LPARAM)timezone_list[i].list_index);
		}
	}
}

LISTITEM add_edit_item;

INT_PTR CALLBACK DlgProcOptsEdit(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	HWND hw;
	int sel;
	TCHAR buf[MAX_NAME_LENGTH];

	switch ( msg ) {
	case WM_INITDIALOG: 
		{
			TranslateDialogDefault( hwndDlg );

			CheckDlgButton(hwndDlg, IDC_RAD_ALPHA, 1);
			CheckDlgButton(hwndDlg, IDC_RAD_GEO, 0);

			fill_timezone_list_control(hwndDlg);

			if(add_edit_item.timezone_list_index != -1) {
				hw = GetDlgItem(hwndDlg, IDC_LIST_TIMES);
				SendMessage(hw, LB_SELECTSTRING, (WPARAM)-1, (LPARAM)timezone_list[add_edit_item.timezone_list_index].tcName);
				SetDlgItemText(hwndDlg, IDC_ED_LABEL, add_edit_item.pszText);

				hw = GetDlgItem(hwndDlg, IDC_ED_LABEL);
				EnableWindow(hw, TRUE);
				hw = GetDlgItem(hwndDlg, IDOK);
				EnableWindow(hw, TRUE);

				hw = GetDlgItem(hwndDlg, IDC_ED_DISP);
				SetWindowText(hw, timezone_list[add_edit_item.timezone_list_index].tcDisp);
			}
		}
		break;

	case WM_COMMAND: 
		if (HIWORD( wParam ) == LBN_SELCHANGE && LOWORD(wParam) == IDC_LIST_TIMES) {
			hw = GetDlgItem(hwndDlg, IDC_LIST_TIMES);
			sel = SendMessage(hw, LB_GETCURSEL, 0, 0);
			if(sel != LB_ERR) {
				hw = GetDlgItem(hwndDlg, IDC_ED_LABEL);
				EnableWindow(hw, sel != -1);
				if(sel == -1)
					add_edit_item.timezone_list_index = -1;
				else {
					hw = GetDlgItem(hwndDlg, IDC_LIST_TIMES);
					add_edit_item.timezone_list_index = (int)SendMessage(hw, LB_GETITEMDATA, sel, 0);

					hw = GetDlgItem(hwndDlg, IDC_ED_DISP);
					SetWindowText(hw, timezone_list[add_edit_item.timezone_list_index].tcDisp);
				}
			}
		}

		if ( HIWORD( wParam ) == EN_CHANGE && ( HWND )lParam == GetFocus()) {
			switch( LOWORD( wParam )) {
			case IDC_ED_LABEL:
				GetDlgItemText(hwndDlg, IDC_ED_LABEL, buf, MAX_NAME_LENGTH);
				hw = GetDlgItem(hwndDlg, IDOK);
				EnableWindow(hw, (_tcslen(buf) > 0));
				_tcsncpy(add_edit_item.pszText, buf, MAX_NAME_LENGTH);
			}
		}

		if ( HIWORD( wParam ) == BN_CLICKED ) {
			switch( LOWORD( wParam )) {
			case IDC_RAD_ALPHA:
				CheckDlgButton(hwndDlg, IDC_RAD_GEO, 0);
				fill_timezone_list_control(hwndDlg);
				hw = GetDlgItem(hwndDlg, IDC_LIST_TIMES);
				SendMessage(hw, LB_SELECTSTRING, (WPARAM)-1, (LPARAM)timezone_list[add_edit_item.timezone_list_index].tcName);
				break;
			case IDC_RAD_GEO:
				CheckDlgButton(hwndDlg, IDC_RAD_ALPHA, 0);
				fill_timezone_list_control(hwndDlg);
				hw = GetDlgItem(hwndDlg, IDC_LIST_TIMES);
				SendMessage(hw, LB_SELECTSTRING, (WPARAM)-1, (LPARAM)timezone_list[add_edit_item.timezone_list_index].tcName);
				break;
			case IDOK:
				EndDialog(hwndDlg, IDOK);
				break;
			case IDCANCEL:
				EndDialog(hwndDlg, IDCANCEL);
				break;
			}
		}
		break;
	}
	return FALSE;
}

static INT_PTR CALLBACK DlgProcOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {

	HWND hw;
	int sel, index;

	switch ( msg ) {
	case WM_INITDIALOG: {
		TranslateDialogDefault( hwndDlg );

		load_listbox_items();
		copy_listbox_items(temp_listbox_items, listbox_items);
		hw = GetDlgItem(hwndDlg, IDC_LIST_TIMES2);

		for(index = 0; index < temp_listbox_items.getCount(); index++) {
			sel = SendMessage(hw, LB_INSERTSTRING, (WPARAM)-1, (LPARAM)temp_listbox_items[index].pszText);
		}

		bool set_format = (db_get_b(NULL, "WorldTime", "EnableTimeFormat", 0) == 1);
		CheckDlgButton(hwndDlg, IDC_CHK_FORMAT, set_format ? 1 : 0);
		bool hide_proto = (db_get_b(NULL, "WorldTime", "HideProtocol", 0) == 1);
		CheckDlgButton(hwndDlg, IDC_CHK_HIDE, hide_proto ? 1 : 0);
		DBVARIANT dbv;
		if(!db_get_ts(NULL, "WorldTime", "TimeFormat", &dbv)) {
			_tcscpy(format_string, dbv.ptszVal);
			db_free(&dbv);
		}
		SetDlgItemText(hwndDlg, IDC_ED_FORMAT, format_string);
		if(!db_get_ts(NULL, "WorldTime", "DateFormat", &dbv)) {
			_tcscpy(date_format_string, dbv.ptszVal);
			db_free(&dbv);
		}
		SetDlgItemText(hwndDlg, IDC_ED_DATE_FORMAT, date_format_string);
		if(!db_get_ts(NULL, "WorldTime", "CListFormat", &dbv)) {
			_tcscpy(clist_format_string, dbv.ptszVal);
			db_free(&dbv);
		}
		SetDlgItemText(hwndDlg, IDC_ED_CLIST_FORMAT, clist_format_string);

		if(!set_format) {
			hw = GetDlgItem(hwndDlg, IDC_ED_FORMAT);
			EnableWindow(hw, FALSE);
			hw = GetDlgItem(hwndDlg, IDC_ED_DATE_FORMAT);
			EnableWindow(hw, FALSE);
		}

		return TRUE;
	}
	case WM_COMMAND:
		if ( HIWORD( wParam ) == EN_CHANGE && ( HWND )lParam == GetFocus()) {
			switch( LOWORD( wParam )) {
			case IDC_ED_FORMAT:
			case IDC_ED_DATE_FORMAT:
			case IDC_ED_CLIST_FORMAT:
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			}	
		}

		if (HIWORD( wParam ) == LBN_SELCHANGE && LOWORD(wParam) == IDC_LIST_TIMES2) {
			hw = GetDlgItem(hwndDlg, IDC_LIST_TIMES2);
			sel = SendMessage(hw, LB_GETCURSEL, 0, 0);
			if(sel != LB_ERR) {
				hw = GetDlgItem(hwndDlg, IDC_BTN_REM);
				EnableWindow(hw, sel != -1);
				hw = GetDlgItem(hwndDlg, IDC_BTN_EDIT);
				EnableWindow(hw, sel != -1);
			}
		}

		if ( HIWORD( wParam ) == BN_CLICKED ) {
			switch( LOWORD( wParam )) {
			case IDC_BTN_EDIT:
				hw = GetDlgItem(hwndDlg, IDC_LIST_TIMES2);
				sel = SendMessage(hw, LB_GETCURSEL, 0, 0);
				if(sel != LB_ERR && sel != -1) {

					add_edit_item = temp_listbox_items[sel];
					if(DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG2), hwndDlg, DlgProcOptsEdit) == IDOK) {
						temp_listbox_items[sel] = add_edit_item;
						
						SendMessage(hw, LB_DELETESTRING, (WPARAM)sel, (LPARAM)0);
						SendMessage(hw, LB_INSERTSTRING, (WPARAM)sel, (LPARAM)add_edit_item.pszText);
						SendMessage(hw, LB_SETCURSEL, (WPARAM)sel, 0);

						SendMessage( GetParent( hwndDlg ), PSM_CHANGED, 0, 0 );
					}
				}
				break;

			case IDC_BTN_ADD:

				add_edit_item.pszText[0] = '\0';
				add_edit_item.timezone_list_index = -1;
				add_edit_item.hContact = 0;

				if(DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG2), hwndDlg, DlgProcOptsEdit) == IDOK) {
					temp_listbox_items.insert(new LISTITEM(add_edit_item));

					hw = GetDlgItem(hwndDlg, IDC_LIST_TIMES2);
					sel = SendMessage(hw, LB_INSERTSTRING, (WPARAM)-1, (LPARAM)add_edit_item.pszText);
					SendMessage(hw, LB_SETCURSEL, (WPARAM)sel, 0);

					hw = GetDlgItem(hwndDlg, IDC_BTN_REM);
					EnableWindow(hw, TRUE);
					hw = GetDlgItem(hwndDlg, IDC_BTN_EDIT);
					EnableWindow(hw, TRUE);

					SendMessage( GetParent( hwndDlg ), PSM_CHANGED, 0, 0 );
				}
				break;

			case IDC_BTN_REM:
				hw = GetDlgItem(hwndDlg, IDC_LIST_TIMES2);
				sel = SendMessage(hw, LB_GETCURSEL, 0, 0);
				if(sel != LB_ERR) {
					SendMessage(hw, LB_DELETESTRING, (WPARAM)sel, 0);

					temp_listbox_items.remove(sel);

					hw = GetDlgItem(hwndDlg, IDC_BTN_REM);
					EnableWindow(hw, FALSE);
					hw = GetDlgItem(hwndDlg, IDC_BTN_EDIT);
					EnableWindow(hw, FALSE);
					SendMessage( GetParent( hwndDlg ), PSM_CHANGED, 0, 0 );
				}
				break;
			case IDC_CHK_FORMAT:
				hw = GetDlgItem(hwndDlg, IDC_ED_FORMAT);
				EnableWindow(hw, IsDlgButtonChecked(hwndDlg, IDC_CHK_FORMAT));
				hw = GetDlgItem(hwndDlg, IDC_ED_DATE_FORMAT);
				EnableWindow(hw, IsDlgButtonChecked(hwndDlg, IDC_CHK_FORMAT));
				SendMessage( GetParent( hwndDlg ), PSM_CHANGED, 0, 0 );
				break;
			case IDC_CHK_HIDE:
				SendMessage( GetParent( hwndDlg ), PSM_CHANGED, 0, 0 );
				break;
			}
		}
		break;

	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->code == PSN_APPLY ) {
			set_format = IsDlgButtonChecked(hwndDlg, IDC_CHK_FORMAT) == BST_CHECKED;
			db_set_b(NULL, "WorldTime", "EnableTimeFormat", set_format ? 1 : 0);
			hide_proto = IsDlgButtonChecked(hwndDlg, IDC_CHK_HIDE) == BST_CHECKED;
			db_set_b(NULL, "WorldTime", "HideProtocol", hide_proto ? 1 : 0);

			TCHAR buf[512];
			GetDlgItemText(hwndDlg, IDC_ED_FORMAT, buf, 512);
			db_set_ts(NULL, "WorldTime", "TimeFormat", buf);
			_tcsncpy(format_string, buf, 512);

			GetDlgItemText(hwndDlg, IDC_ED_DATE_FORMAT, buf, 512);
			db_set_ts(NULL, "WorldTime", "DateFormat", buf);
			_tcsncpy(date_format_string, buf, 512);
	
			GetDlgItemText(hwndDlg, IDC_ED_CLIST_FORMAT, buf, 512);
			db_set_ts(NULL, "WorldTime", "CListFormat", buf);
			_tcsncpy(clist_format_string, buf, 512);
	
			copy_listbox_items(listbox_items, temp_listbox_items);
			save_listbox_items();
			copy_listbox_items(temp_listbox_items, listbox_items); // copy back new hContact values
			return TRUE;
		}
		break;
	}

	return FALSE;
}

int OptInit(WPARAM wParam,LPARAM lParam)
{
	OPTIONSDIALOGPAGE odp = { 0 };
	odp.position					= -790000000;
	odp.hInstance					= hInst;
	odp.pszTemplate					= MAKEINTRESOURCEA(IDD_DIALOG1);
	odp.pszTitle					= "World Time";
	odp.pszGroup					= "Plugins";
	odp.flags						= ODPF_BOLDGROUPS;
	odp.pfnDlgProc					= DlgProcOpts;
	Options_AddPage(wParam, &odp); //add page to options menu pages

	return 0;
}

void LoadOptions() {
	set_format = (db_get_b(NULL, "WorldTime", "EnableTimeFormat", 0) == 1);
	hide_proto = (db_get_b(NULL, "WorldTime", "HideProtocol", 0) == 1);

	DBVARIANT dbv;
	if(!db_get_ts(NULL, "WorldTime", "TimeFormat", &dbv)) {
		_tcsncpy(format_string, dbv.ptszVal, 512);
		db_free(&dbv);
	} else
		_tcscpy(format_string, _T("HH:mm"));
	if(!db_get_ts(NULL, "WorldTime", "DateFormat", &dbv)) {
		_tcsncpy(date_format_string, dbv.ptszVal, 512);
		db_free(&dbv);
	} else
		_tcscpy(date_format_string, _T("d/M"));

	if(!db_get_ts(NULL, "WorldTime", "CListFormat", &dbv)) {
		_tcsncpy(clist_format_string, dbv.ptszVal, 512);
		db_free(&dbv);
	} else
		_tcscpy(clist_format_string, _T("%n: %t %d"));
}
