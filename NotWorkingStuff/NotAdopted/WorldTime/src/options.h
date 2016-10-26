#ifndef _OPTIONS_INC
#define _OPTIONS_INC

#include "resource.h"

#include "timezone.h"
#include "timezone_list.h"

extern bool set_format, hide_proto;
extern TCHAR format_string[512], date_format_string[512], clist_format_string[512];
extern LISTITEM add_edit_item;

INT_PTR CALLBACK DlgProcOptsEdit(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
int OptInit(WPARAM wParam,LPARAM lParam);

void LoadOptions();

#endif
