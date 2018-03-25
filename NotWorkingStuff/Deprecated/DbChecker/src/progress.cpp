/*
Miranda Database Tool
Copyright (C) 2001-2005  Richard Hughes

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "stdafx.h"

#define WM_PROCESSINGDONE  (WM_USER+1)

void __cdecl WorkerThread(void *unused);
static HWND hwndStatus, hdlgProgress, hwndBar;
static bool bShortModeDone;
HANDLE hEventRun = nullptr, hEventAbort = nullptr;
int errorCount;
LRESULT wizardResult;

void AddToStatus(int flags, const wchar_t* fmt, ...)
{
	va_list vararg;
	va_start(vararg, fmt);

	wchar_t str[256];
	mir_vsnwprintf(str, _countof(str), fmt, vararg);
	va_end(vararg);

	int i = SendMessage(hwndStatus, LB_ADDSTRING, 0, (LPARAM)str);
	SendMessage(hwndStatus, LB_SETITEMDATA, i, flags);
	InvalidateRect(hwndStatus, nullptr, FALSE);
	SendMessage(hwndStatus, LB_SETTOPINDEX, i, 0);

#ifdef _DEBUG
	OutputDebugString(str);
	OutputDebugStringA("\n");
#endif

	switch (flags & STATUS_CLASSMASK) {
	case STATUS_ERROR:
	case STATUS_FATAL:
		errorCount++;
	}
}

void SetProgressBar(int perThou)
{
	SendMessage(hwndBar, PBM_SETPOS, perThou, 0);
}

void ProcessingDone(void)
{
	SendMessage(hdlgProgress, WM_PROCESSINGDONE, 0, 0);
}

INT_PTR CALLBACK ProgressDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int fontHeight, listWidth;
	static int manualAbort;
	static HFONT hBoldFont = nullptr;

	INT_PTR bReturn;
	if (DoMyControlProcessing(hdlg, message, wParam, lParam, &bReturn))
		return bReturn;

	switch (message) {
	case WM_INITDIALOG:
		EnableWindow(GetDlgItem(GetParent(hdlg), IDOK), FALSE);
		hdlgProgress = hdlg;
		hwndStatus = GetDlgItem(hdlg, IDC_STATUS);
		errorCount = 0;
		bShortModeDone = false;
		hwndBar = GetDlgItem(hdlg, IDC_PROGRESS);
		SendMessage(hwndBar, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
		{
			HDC hdc;
			HFONT hFont, hoFont;
			SIZE s;
			hdc = GetDC(nullptr);
			hFont = (HFONT)SendMessage(hdlg, WM_GETFONT, 0, 0);
			hoFont = (HFONT)SelectObject(hdc, hFont);
			GetTextExtentPoint32(hdc, L"x", 1, &s);
			SelectObject(hdc, hoFont);
			ReleaseDC(nullptr, hdc);
			fontHeight = s.cy;

			RECT rc;
			GetClientRect(GetDlgItem(hdlg, IDC_STATUS), &rc);
			listWidth = rc.right;

			LOGFONT lf;
			GetObject((HFONT)SendDlgItemMessage(hdlg, IDC_STATUS, WM_GETFONT, 0, 0), sizeof(lf), &lf);
			lf.lfWeight = FW_BOLD;
			hBoldFont = CreateFontIndirect(&lf);
		}
		manualAbort = 0;
		hEventRun = CreateEvent(nullptr, TRUE, TRUE, nullptr);
		hEventAbort = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		TranslateDialogDefault(hdlg);
		_beginthread(WorkerThread, 0, nullptr);
		return TRUE;

	case WM_MEASUREITEM:
		{
			LPMEASUREITEMSTRUCT mis = (LPMEASUREITEMSTRUCT)lParam;
			mis->itemWidth = listWidth;
			mis->itemHeight = fontHeight;
		}
		return TRUE;

	case WM_DRAWITEM:
		wchar_t str[256];
		{
			LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
			int bold = 0;
			HFONT hoFont = nullptr;
			if ((int)dis->itemID == -1) break;
			SendMessage(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)str);
			switch (dis->itemData & STATUS_CLASSMASK) {
			case STATUS_MESSAGE:
				SetTextColor(dis->hDC, RGB(0, 0, 0));
				break;
			case STATUS_WARNING:
				SetTextColor(dis->hDC, RGB(192, 128, 0));
				break;
			case STATUS_ERROR:
				SetTextColor(dis->hDC, RGB(192, 0, 0));
				break;
			case STATUS_FATAL:
				bold = 1;
				SetTextColor(dis->hDC, RGB(192, 0, 0));
				break;
			case STATUS_SUCCESS:
				bold = 1;
				SetTextColor(dis->hDC, RGB(0, 192, 0));
				break;
			}
			if (bold) hoFont = (HFONT)SelectObject(dis->hDC, hBoldFont);
			ExtTextOut(dis->hDC, dis->rcItem.left, dis->rcItem.top, ETO_CLIPPED | ETO_OPAQUE, &dis->rcItem, str, (UINT)mir_wstrlen(str), nullptr);
			if (bold) SelectObject(dis->hDC, hoFont);
		}
		return TRUE;

	case WM_PROCESSINGDONE:
		SetProgressBar(1000);
		if (bShortMode) {
			EnableWindow(GetDlgItem(GetParent(hdlg), IDC_BACK), FALSE);
			EnableWindow(GetDlgItem(GetParent(hdlg), IDOK), FALSE);
			SetDlgItemText(GetParent(hdlg), IDCANCEL, TranslateT("&Finish"));
			bShortModeDone = true;
			if (bAutoExit)
				PostMessage(GetParent(hdlg), WM_COMMAND, IDCANCEL, 0);
		}
		else {
			AddToStatus(STATUS_SUCCESS, TranslateT("Click Next to continue"));
			EnableWindow(GetDlgItem(GetParent(hdlg), IDOK), TRUE);
		}

		if (manualAbort == 1)
			EndDialog(GetParent(hdlg), 0);
		else if (manualAbort == 2) {
			if (opts.bCheckOnly)
				PostMessage(GetParent(hdlg), WZM_GOTOPAGE, IDD_FILEACCESS, (LPARAM)FileAccessDlgProc);
			else {
				PostMessage(GetParent(hdlg), WZM_GOTOPAGE, IDD_CLEANING, (LPARAM)CleaningDlgProc);
				CloseHandle(opts.hOutFile);
				opts.hOutFile = nullptr;
			}
			break;
		}
		break;

	case WZN_CANCELCLICKED:
		if (bShortModeDone) {
			if (!errorCount) {
				if (bLaunchMiranda)
					Profile_SetDefault(opts.filename);
				wizardResult = 1;
			}
			return TRUE;
		}

		ResetEvent(hEventRun);
		if (IsWindowEnabled(GetDlgItem(GetParent(hdlg), IDOK)))
			break;

		if (MessageBox(hdlg, TranslateT("Processing has not yet completed, if you cancel now then the changes that have currently been made will be rolled back and the original database will be restored. Do you still want to cancel?"), TranslateT("Miranda Database Tool"), MB_YESNO) == IDYES) {
			manualAbort = 1;
			SetEvent(hEventAbort);
		}
		SetEvent(hEventRun);
		SetWindowLongPtr(hdlg, DWLP_MSGRESULT, TRUE);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BACK:
			ResetEvent(hEventRun);
			if (!IsWindowEnabled(GetDlgItem(GetParent(hdlg), IDOK))) {
				if (MessageBox(hdlg, TranslateT("Processing has not yet completed, if you go back now then the changes that have currently been made will be rolled back and the original database will be restored. Do you still want to go back?"), TranslateT("Miranda Database Tool"), MB_YESNO) == IDYES) {
					manualAbort = 2;
					SetEvent(hEventAbort);
				}
				SetEvent(hEventRun);
				break;
			}
			SetEvent(hEventRun);
			if (opts.bCheckOnly)
				PostMessage(GetParent(hdlg), WZM_GOTOPAGE, IDD_FILEACCESS, (LPARAM)FileAccessDlgProc);
			else
				PostMessage(GetParent(hdlg), WZM_GOTOPAGE, IDD_CLEANING, (LPARAM)CleaningDlgProc);
			break;

		case IDOK:
			PostMessage(GetParent(hdlg), WZM_GOTOPAGE, IDD_FINISHED, (LPARAM)FinishedDlgProc);
			break;
		}
		break;

	case WM_DESTROY:
		if (hEventAbort) {
			CloseHandle(hEventAbort);
			hEventAbort = nullptr;
		}
		if (hEventRun) {
			CloseHandle(hEventRun);
			hEventRun = nullptr;
		}
		if (hBoldFont) {
			DeleteObject(hBoldFont);
			hBoldFont = nullptr;
		}
		break;
	}
	return FALSE;
}
