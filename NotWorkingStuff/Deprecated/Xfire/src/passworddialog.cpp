//f�rs passwort dialog - dufte

#include "stdafx.h"
#include "passworddialog.h"

static char nick[255];
BOOL usenick = FALSE;

INT_PTR CALLBACK DlgPWProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM)
{
	static char* pw[255];
	switch (msg) {
	case WM_CLOSE:
		GetDlgItemTextA(hwndDlg, IDC_PWSTRING, (LPSTR)pw, 254);
		if (usenick)
			GetDlgItemTextA(hwndDlg, IDC_PWNICK, (LPSTR)nick, _countof(nick));
		EndDialog(hwndDlg, (INT_PTR)pw);
		break;

	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		//passwort und nick leeren
		pw[0] = 0;
		nick[0] = 0;
		SendMessage(hwndDlg, WM_SETICON, (WPARAM)false, (LPARAM)LoadIcon(hinstance, MAKEINTRESOURCE(IDI_TM)));
		if (!usenick)
			EnableWindow(GetDlgItem(hwndDlg, IDC_PWNICK), FALSE);

		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_BTN4) {
			SendMessage(hwndDlg, WM_CLOSE, 0, 0);
		}
		break;
	}
	return FALSE;
}

void ShowPasswordDialog(char*pw, char*mynick)
{
	usenick = (mynick != NULL);

	char *npw = (char*)DialogBox(hinstance, MAKEINTRESOURCE(IDD_PWDLG), NULL, DlgPWProc);
	mir_strcpy(pw, npw);
	if (mynick)
		mir_strcpy(mynick, (char*)nick);
}
