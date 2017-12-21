/*
Plugin of Miranda IM for communicating with users of the AIM protocol.
Copyright (c) 2008-2009 Boris Krasnovskiy

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

void CAimProto::chat_register(void)
{
	GCREGISTER gcr = {};
	gcr.dwFlags = GC_TYPNOTIF | GC_CHANMGR;
	gcr.ptszDispName = m_tszUserName;
	gcr.pszModule = m_szModuleName;
	Chat_Register(&gcr);

	HookProtoEvent(ME_GC_EVENT, &CAimProto::OnGCEvent);
	HookProtoEvent(ME_GC_BUILDMENU, &CAimProto::OnGCMenuHook);
}

void CAimProto::chat_start(const char *id, unsigned short exchange)
{
	wchar_t *idt = mir_a2u(id);
	Chat_NewSession(GCW_CHATROOM, m_szModuleName, idt, idt);

	Chat_AddGroup(m_szModuleName, idt, TranslateT("Me"));
	Chat_AddGroup(m_szModuleName, idt, TranslateT("Others"));

	Chat_Control(m_szModuleName, idt, SESSION_INITDONE);
	Chat_Control(m_szModuleName, idt, SESSION_ONLINE);
	Chat_Control(m_szModuleName, idt, WINDOW_VISIBLE);

	setWord(find_chat_contact(id), "Exchange", exchange);

	mir_free(idt);
}

void CAimProto::chat_event(const char* id, const char* sn, int evt, const wchar_t* msg)
{
	ptrW idt(mir_a2u(id));
	ptrW snt(mir_a2u(sn));

	MCONTACT hContact = contact_from_sn(sn);
	wchar_t *nick = hContact ? (wchar_t*)pcli->pfnGetContactDisplayName(hContact, 0) : snt;

	GCEVENT gce = { m_szModuleName, idt, evt };
	gce.dwFlags = GCEF_ADDTOLOG;
	gce.ptszNick = nick;
	gce.ptszUID = snt;
	gce.bIsMe = _stricmp(sn, m_username) == 0;
	gce.ptszStatus = gce.bIsMe ? TranslateT("Me") : TranslateT("Others");
	gce.ptszText = msg;
	gce.time = time(nullptr);
	Chat_Event(&gce);
}

void CAimProto::chat_leave(const char* id)
{
	ptrW idt(mir_a2u(id));
	Chat_Control(m_szModuleName, idt, SESSION_OFFLINE);
	Chat_Terminate(m_szModuleName, idt);
}

int CAimProto::OnGCEvent(WPARAM, LPARAM lParam)
{
	GCHOOK *gch = (GCHOOK*)lParam;
	if (!gch) return 1;

	if (mir_strcmp(gch->pszModule, m_szModuleName))
		return 0;

	char *id = mir_u2a(gch->ptszID);
	chat_list_item* item = find_chat_by_id(id);
	if (item == nullptr)
		return 0;

	switch (gch->iType) {
	case GC_SESSION_TERMINATE:
		aim_sendflap(item->hconn, 0x04, 0, nullptr, item->seqno);
		Netlib_Shutdown(item->hconn);
		break;

	case GC_USER_MESSAGE:
		if (gch->ptszText && mir_wstrlen(gch->ptszText))
			aim_chat_send_message(item->hconn, item->seqno, T2Utf(gch->ptszText));
		break;

	case GC_USER_CHANMGR:
		DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_CHATROOM_INVITE), nullptr, invite_to_chat_dialog,
			LPARAM(new invite_chat_param(item->id, this)));
		break;

	case GC_USER_PRIVMESS:
		{
			char* sn = mir_u2a(gch->ptszUID);
			MCONTACT hContact = contact_from_sn(sn);
			mir_free(sn);
			CallService(MS_MSG_SENDMESSAGE, hContact, 0);
		}
		break;

	case GC_USER_LOGMENU:
		switch (gch->dwData) {
		case 10:
			DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_CHATROOM_INVITE), nullptr, invite_to_chat_dialog,
				LPARAM(new invite_chat_param(item->id, this)));
			break;

		case 20:
			chat_leave(id);
			break;
		}
		break;

	case GC_USER_NICKLISTMENU:
		{
			char *sn = mir_u2a(gch->ptszUID);
			MCONTACT hContact = contact_from_sn(sn);
			mir_free(sn);

			switch (gch->dwData) {
			case 10:
				CallService(MS_USERINFO_SHOWDIALOG, hContact, 0);
				break;

			case 20:
				CallService(MS_HISTORY_SHOWCONTACTHISTORY, hContact, 0);
				break;

			case 110:
				chat_leave(id);
				break;
			}
		}
		break;

	case GC_USER_TYPNOTIFY:
		break;
	}
	mir_free(id);

	return 0;
}

int CAimProto::OnGCMenuHook(WPARAM, LPARAM lParam)
{
	GCMENUITEMS *gcmi = (GCMENUITEMS*)lParam;
	if (mir_strcmp(gcmi->pszModule, m_szModuleName))
		return 0;

	if (gcmi->Type == MENU_ON_LOG) {
		static const struct gc_item Items[] = {
			{ LPGENW("&Invite user..."), 10, MENU_ITEM, FALSE },
			{ LPGENW("&Leave chat session"), 20, MENU_ITEM, FALSE }
		};
		Chat_AddMenuItems(gcmi->hMenu, _countof(Items), Items);
	}
	else if (gcmi->Type == MENU_ON_NICKLIST) {
		char* sn = mir_u2a(gcmi->pszUID);
		if (!mir_strcmp(m_username, sn)) {
			static const struct gc_item Items[] = {
				{ LPGENW("User &details"), 10, MENU_ITEM, FALSE },
				{ LPGENW("User &history"), 20, MENU_ITEM, FALSE },
				{ L"", 100, MENU_SEPARATOR, FALSE },
				{ LPGENW("&Leave chat session"), 110, MENU_ITEM, FALSE }
			};
			Chat_AddMenuItems(gcmi->hMenu, _countof(Items), Items);
		}
		else {
			static const struct gc_item Items[] = {
				{ LPGENW("User &details"), 10, MENU_ITEM, FALSE },
				{ LPGENW("User &history"), 20, MENU_ITEM, FALSE }
			};
			Chat_AddMenuItems(gcmi->hMenu, _countof(Items), Items);
		}
		mir_free(sn);
	}

	return 0;
}


void   __cdecl CAimProto::chatnav_request_thread(void* param)
{
	chatnav_param *par = (chatnav_param*)param;

	if (wait_conn(m_hChatNavConn, m_hChatNavEvent, 0x0d)) {
		if (par->isroom)
			aim_chatnav_create(m_hChatNavConn, m_chatnav_seqno, par->id, par->exchange);
		else
			aim_chatnav_room_info(m_hChatNavConn, m_chatnav_seqno, par->id, par->exchange, par->instance);
	}
	delete par;
}

chat_list_item* CAimProto::find_chat_by_cid(unsigned short cid)
{
	for (int i = 0; i < m_chat_rooms.getCount(); ++i)
		if (m_chat_rooms[i].cid == cid)
			return &m_chat_rooms[i];

	return nullptr;
}

chat_list_item* CAimProto::find_chat_by_id(char* id)
{
	for (int i = 0; i < m_chat_rooms.getCount(); ++i)
		if (mir_strcmp(m_chat_rooms[i].id, id) == 0)
			return &m_chat_rooms[i];

	return nullptr;
}

chat_list_item* CAimProto::find_chat_by_conn(HANDLE conn)
{
	for (int i = 0; i < m_chat_rooms.getCount(); ++i)
		if (m_chat_rooms[i].hconn == conn)
			return &m_chat_rooms[i];

	return nullptr;
}

void CAimProto::remove_chat_by_ptr(chat_list_item *item)
{
	for (int i = 0; i < m_chat_rooms.getCount(); ++i) {
		if (&m_chat_rooms[i] == item) {
			m_chat_rooms.remove(i);
			break;
		}
	}
}

void CAimProto::shutdown_chat_conn(void)
{
	for (int i = 0; i < m_chat_rooms.getCount(); ++i) {
		chat_list_item &item = m_chat_rooms[i];
		if (item.hconn) {
			aim_sendflap(item.hconn, 0x04, 0, nullptr, item.seqno);
			Netlib_Shutdown(item.hconn);
		}
	}
}

void CAimProto::close_chat_conn(void)
{
	for (int i = 0; i < m_chat_rooms.getCount(); ++i) {
		chat_list_item &item = m_chat_rooms[i];
		if (item.hconn) {
			Netlib_CloseHandle(item.hconn);
			item.hconn = nullptr;
		}
	}
}
