#ifndef _TIMEZONE_LIST_INC
#define _TIMEZONE_LIST_INC

#include "timezone.h"
#include "time_convert.h"

#define MAX_NAME_LENGTH	256

typedef struct tagLISTITEM {
	int cbSize;
	MCONTACT hContact;
	TCHAR pszText[MAX_NAME_LENGTH];
	int timezone_list_index;
} LISTITEM;

typedef OBJLIST<LISTITEM> ITEMLIST;

extern ITEMLIST listbox_items;

void load_listbox_items();
void save_listbox_items();
void copy_listbox_items(ITEMLIST &dest, ITEMLIST &src);

#endif
