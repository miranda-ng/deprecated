#include "common.h"
#include "time_display.h"

#define ID_UPDATE_TIMER		101101
#define UPDATE_TIMER		250

#define WM_UPDATE_DATA		(WM_USER + 10)

extern HINSTANCE hInst;

HFONT hBigFont = 0, hNormalFont = 0;

void LoadFonts() 
{
	static const TCHAR facename[] = _T("Tahoma");

	if (!hBigFont) 
	{
		LOGFONT logfont = {0};
		_tcscpy(logfont.lfFaceName, facename);
		logfont.lfWeight = FW_NORMAL;
		logfont.lfHeight = -40;
		hBigFont = CreateFontIndirect(&logfont);
	}
	if (!hNormalFont) 
	{
		LOGFONT logfont = {0};
		_tcscpy(logfont.lfFaceName, facename);
		logfont.lfWeight = FW_NORMAL;
		logfont.lfHeight = -12;
		hNormalFont = CreateFontIndirect(&logfont);
	}
}

void UnloadFonts() {
	if(hBigFont) {
		DeleteObject(hBigFont);
		hBigFont = 0;
	}
	if(hNormalFont) {
		DeleteObject(hNormalFont);
		hNormalFont = 0;
	}
}

typedef struct WindowData_tag {
	MCONTACT hContact;
	int timezone_list_index;
	TCHAR time_buff[32];
	TCHAR date_buff[128];
	TCHAR nick_buff[256];
	TIME_ZONE_INFORMATION tzi;
} WindowData;

INT_PTR CALLBACK DlgProcDisplay(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	RECT r;
	HDC hdc;
	PAINTSTRUCT ps;
	HFONT oldFont;

	switch ( msg ) {
	case WM_INITDIALOG: 
		TranslateDialogDefault( hwndDlg );
		
		SetTimer(hwndDlg, ID_UPDATE_TIMER, UPDATE_TIMER, 0);
		return TRUE;

	case WM_ERASEBKGND: 
		return TRUE;

	case WM_PAINT:
		if(GetUpdateRect(hwndDlg, &r, FALSE)) {
			hdc = BeginPaint(hwndDlg, &ps);

			GetClientRect(hwndDlg, &r);
			FillRect(hdc, &r, GetSysColorBrush(COLOR_WINDOW));

			WindowData *wd = (WindowData *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
			if(wd) {
				RECT br = r, lr = r;
				br.bottom -= (r.bottom - r.top) / 4;
				oldFont = (HFONT)SelectObject(hdc, hBigFont);
				DrawText(hdc, wd->time_buff, (int)_tcslen(wd->time_buff), &br, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
				SelectObject(hdc, hNormalFont);
				lr.top = (r.bottom - r.top) * 3 / 4;
				DrawText(hdc, wd->date_buff, (int)_tcslen(wd->date_buff), &lr, DT_SINGLELINE | DT_VCENTER | DT_CENTER);			
				SelectObject(hdc, oldFont);
			}

			EndPaint(hwndDlg, &ps);
		}
		return TRUE;

	case WM_TIMER:
		SendMessage(hwndDlg, WM_UPDATE_DATA, 0, 0);
		return TRUE;

	case WM_UPDATE_DATA:
		{
			SYSTEMTIME st, other_st;
			WindowData *wd = (WindowData *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

			MyGetSystemTime(&st);
			MySystemTimeToTzSpecificLocalTime(&wd->tzi, &st, &other_st);
			//GetTimeFormat(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &other_st, 0, wd->time_buff, 32);
			GetTimeFormat(LOCALE_USER_DEFAULT, 0, &other_st, 0, wd->time_buff, 32);
			GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &other_st, 0, wd->date_buff, 128);

			if(IsIconic(hwndDlg)) {
				TCHAR buff[255 + 32];
				_tcscpy(buff, wd->nick_buff);
				_tcscat(buff, _T(" - "));
				_tcscat(buff, wd->time_buff);
				SetWindowText(hwndDlg, buff);
			} else
				SetWindowText(hwndDlg, wd->nick_buff);
		}
		InvalidateRect(hwndDlg, 0, FALSE);
		return TRUE;

	case WM_CLOSE: 
		DestroyWindow(hwndDlg);
		return TRUE;

	case WM_DESTROY:
		{
			KillTimer(hwndDlg, ID_UPDATE_TIMER);
			WindowData *wd = (WindowData *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
			MCONTACT hContact = wd->hContact;
			if(CallService(MS_DB_CONTACT_IS, (WPARAM)hContact, 0))
				Utils_SaveWindowPosition(hwndDlg, hContact, PROTO, "timewnd");
			db_unset(hContact, PROTO, "WindowHandle");
			delete wd;
		}
		break;
	}

	return FALSE;
}


int show_time(MCONTACT hContact) {
	HWND hwnd = (HWND)db_get_dw(hContact, PROTO, "WindowHandle", 0);
	if(hwnd) {
		ShowWindow(hwnd, SW_SHOW);
		SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		return 0;
	}

	hwnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_DISPLAY), GetDesktopWindow(), DlgProcDisplay);
	
	WindowData *wd = new WindowData;
	wd->hContact = hContact;
	wd->timezone_list_index = db_get_dw(hContact, PROTO, "TimezoneListIndex", -1);
	wd->time_buff[0] = 0;
	wd->date_buff[0] = 0;

	wd->tzi.Bias = timezone_list[wd->timezone_list_index].TZI.Bias;
	wd->tzi.DaylightBias = timezone_list[wd->timezone_list_index].TZI.DaylightBias;
	wd->tzi.DaylightDate = timezone_list[wd->timezone_list_index].TZI.DaylightDate;
	wd->tzi.StandardBias = timezone_list[wd->timezone_list_index].TZI.StandardBias;
	wd->tzi.StandardDate = timezone_list[wd->timezone_list_index].TZI.StandardDate;

	DBVARIANT dbv;
	if(!db_get_ts(wd->hContact, PROTO, "Nick", &dbv)) {
		_tcsncpy(wd->nick_buff, dbv.ptszVal, 255);
		SetWindowText(hwnd, dbv.ptszVal);
		db_free(&dbv);
	}

	db_set_dw(hContact, PROTO, "WindowHandle", (DWORD)hwnd);
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)wd);
	SendMessage(hwnd, WM_UPDATE_DATA, 0, 0);

	if(CallService(MS_DB_CONTACT_IS, (WPARAM)hContact, 0))
		Utils_RestoreWindowPosition(hwnd, hContact, PROTO, "timewnd");

	if(!IsWindowVisible(hwnd)) {
		ShowWindow(hwnd, SW_SHOW);
		SendMessage(hwnd, WM_UPDATE_DATA, 0, 0);
	}

	return 0;
}

// save window positions, close windows
void time_windows_cleanup() {
	HWND hwnd;
	char *proto;
	MCONTACT hContact = db_find_first();
	while ( hContact != NULL )
	{
		proto = (char *)CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM)hContact, 0);
		if (proto &&  strcmp( PROTO, proto) == 0) {
			if((hwnd = (HWND)db_get_dw(hContact, PROTO, "WindowHandle", 0)) != 0) {
				DestroyWindow(hwnd);
				db_set_b(hContact, PROTO, "WindowWasOpen", 1);
			}
		}

		hContact = db_find_next(hContact);
	}	
	UnloadFonts();
}

// restore windows that were open when cleanup was called last
void time_windows_init() {
	LoadFonts();

	MCONTACT hContact = db_find_first();
	while ( hContact != NULL )
	{
		char* proto = (char *)CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM)hContact, 0);
		if (proto &&  strcmp( PROTO, proto) == 0) 
		{
			if (db_get_b(hContact, PROTO, "WindowWasOpen", 0) != 0) 
			{
				show_time(hContact);
				db_set_b(hContact, PROTO, "WindowWasOpen", 0);
			}

			DBVARIANT dbv;
			if (!db_get_ts(hContact, PROTO, "TZName", &dbv)) 
			{
				int list_index = db_get_dw(hContact, PROTO, "TimezoneListIndex", -1);
				if (list_index < 0 || list_index >= timezone_list.getCount()) 
					list_index = 0;

				if (_tcscmp(timezone_list[list_index].tcName, dbv.ptszVal)) 
				{
					for (int j = 0; j < timezone_list.getCount(); ++j) 
					{
						if (!_tcscmp(timezone_list[j].tcName, dbv.ptszVal))
						{
							list_index = j;
							break;
						}
					}
					db_set_dw(hContact, PROTO, "TimezoneListIndex", list_index);
				}
				db_free(&dbv);
			}
		}

		hContact = db_find_next(hContact);
	}	
}
