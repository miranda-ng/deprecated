#include "stdafx.h"

INT_PTR SnapToList(WPARAM wParam, LPARAM Align)
{
	HWND hWnd = (HWND)wParam;
	HWND hWndList = pcli->hwndContactList;

	RECT WndRect, ListRect;
	GetWindowRect(hWnd, &WndRect);
	GetWindowRect(hWndList, &ListRect);

	RECT AlignRect = ListRect;
	if ((!(MS_MW_STL_List_Left & Align)) && (MS_MW_STL_List_Right & Align))
		AlignRect.left = AlignRect.right;
	else if ((MS_MW_STL_List_Left & Align) && (!(MS_MW_STL_List_Right & Align)))
		AlignRect.right = AlignRect.left;

	if ((!(MS_MW_STL_List_Top & Align)) && (MS_MW_STL_List_Bottom & Align))
		AlignRect.top = AlignRect.bottom;
	else if ((MS_MW_STL_List_Top & Align) && (!(MS_MW_STL_List_Bottom & Align)))
		AlignRect.bottom = AlignRect.top;

	RECT ResultRect = WndRect;
	if ((MS_MW_STL_Wnd_Left & Align) && (MS_MW_STL_Wnd_Right & Align)) {
		ResultRect.left = AlignRect.left;
		ResultRect.right = AlignRect.right;
	}
	else if ((!(MS_MW_STL_Wnd_Left & Align)) && (MS_MW_STL_Wnd_Right & Align)) {
		ResultRect.left = AlignRect.right - (WndRect.right - WndRect.left);
		ResultRect.right = AlignRect.right;
	}
	else if ((MS_MW_STL_Wnd_Left & Align) && (!(MS_MW_STL_Wnd_Right & Align))) {
		ResultRect.left = AlignRect.left;
		ResultRect.right = AlignRect.left + (WndRect.right - WndRect.left);
	}

	if ((MS_MW_STL_Wnd_Top & Align) && (MS_MW_STL_Wnd_Bottom & Align)) {
		ResultRect.top = AlignRect.top;
		ResultRect.bottom = AlignRect.bottom;
	}
	else if ((!(MS_MW_STL_Wnd_Top & Align)) && (MS_MW_STL_Wnd_Bottom & Align)) {
		ResultRect.top = AlignRect.bottom - (WndRect.bottom - WndRect.top);
		ResultRect.bottom = AlignRect.bottom;
	}
	else if ((MS_MW_STL_Wnd_Top & Align) && (!(MS_MW_STL_Wnd_Bottom & Align))) {
		ResultRect.top = AlignRect.top;
		ResultRect.bottom = AlignRect.top + (WndRect.bottom - WndRect.top);
	}

	MoveWindow(hWnd, ResultRect.left, ResultRect.top, ResultRect.right - ResultRect.left, ResultRect.bottom - ResultRect.top, true);
	return 0;
}
