#include "common.h"
#include "timezone_list.h"

ITEMLIST listbox_items(5);

void load_listbox_items() {

	MCONTACT hContact = db_find_first();
	LISTITEM pa;
	DBVARIANT dbv;
	char *proto;

	listbox_items.destroy();
	while ( hContact != NULL ) {
		proto = ( char* )CallService( MS_PROTO_GETCONTACTBASEPROTO, ( WPARAM )hContact,0 );
		if ( proto && !strcmp( PROTO, proto)) {
			pa.hContact = hContact;
			if(!db_get_ts(pa.hContact, PROTO, "TZName", &dbv)) {
				for (int j = 0; j < timezone_list.getCount(); ++j) {
					if(!_tcscmp(timezone_list[j].tcName, dbv.ptszVal)) {
						pa.timezone_list_index = timezone_list[j].list_index;
						break;
					}
				}
				db_free(&dbv);
			} else
				pa.timezone_list_index = db_get_dw(pa.hContact, PROTO, "TimezoneListIndex", -1);
			if(!db_get_ts(pa.hContact, PROTO, "Nick", &dbv)) {
				_tcsncpy(pa.pszText, dbv.ptszVal, MAX_NAME_LENGTH);
				db_free(&dbv);
			}
			listbox_items.insert(new LISTITEM(pa));
		}

		hContact = db_find_next(hContact);
	}	
}

void save_listbox_items() {
	bool is_contact;


	for(int i = 0; i < listbox_items.getCount(); ++i) {
		is_contact = (int)CallService(MS_DB_CONTACT_IS, (WPARAM)listbox_items[i].hContact, 0) == 1;

		if(!is_contact) {
			listbox_items[i].hContact = (MCONTACT)CallService(MS_DB_CONTACT_ADD, 0, 0);
			CallService( MS_PROTO_ADDTOCONTACT, ( WPARAM )listbox_items[i].hContact, ( LPARAM )PROTO );	
			CallService(MS_IGNORE_IGNORE, (WPARAM)listbox_items[i].hContact, (WPARAM)IGNOREEVENT_USERONLINE);
		}

		db_set_ts(listbox_items[i].hContact, PROTO, "Nick", listbox_items[i].pszText);
		db_set_dw(listbox_items[i].hContact, PROTO, "TimezoneListIndex", listbox_items[i].timezone_list_index);
		db_set_w(listbox_items[i].hContact, PROTO, "Status", ID_STATUS_ONLINE);
		db_set_ts(listbox_items[i].hContact, PROTO, "TZName", timezone_list[listbox_items[i].timezone_list_index].tcName);
	}
	db_set_w(0, PROTO, "DataVersion", 1);

	// remove contacts in DB that have been removed from the list
	MCONTACT hContact = db_find_first();
	char *proto;
	bool found;
	while ( hContact != NULL ) {
		proto = ( char* )CallService( MS_PROTO_GETCONTACTBASEPROTO, ( WPARAM )hContact,0 );
		if ( proto && !strcmp( PROTO, proto)) {
			found = false;
			for(int i = 0; i < listbox_items.getCount(); i++) {
				if(listbox_items[i].hContact == hContact) {
					found = true;
				}
			}

			if(!found) {
				// remove prot first, so that our contact deleted event handler (if present) isn't activated
				MCONTACT oldHContact = hContact;
				hContact = db_find_next(hContact);

				CallService(MS_PROTO_REMOVEFROMCONTACT, (WPARAM)oldHContact, (LPARAM)PROTO);
				CallService(MS_DB_CONTACT_DELETE, (WPARAM)oldHContact, 0);
			} else
				hContact = db_find_next(hContact);
		} else
			hContact = db_find_next(hContact);
	}	
}

void copy_listbox_items(ITEMLIST &dest, ITEMLIST &src)
{
	dest.destroy();
	for (int i=0; i < src.getCount(); ++i)
		dest.insert(new LISTITEM(src[i]));
}


