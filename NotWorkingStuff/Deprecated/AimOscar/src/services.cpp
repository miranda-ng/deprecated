/*
Plugin of Miranda IM for communicating with users of the AIM protocol.
Copyright (c) 2008-2012 Boris Krasnovskiy
Copyright (C) 2005-2006 Aaron Myles Landwehr

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

INT_PTR CAimProto::GetMyAwayMsg(WPARAM wParam, LPARAM lParam)
{
	char** msgptr = get_status_msg_loc(wParam ? wParam : m_iStatus);
	if (msgptr == nullptr)	return 0;

	return (lParam & SGMA_UNICODE) ? (INT_PTR)mir_utf8decodeW(*msgptr) : (INT_PTR)mir_utf8decodeA(*msgptr);
}

int CAimProto::OnIdleChanged(WPARAM, LPARAM lParam)
{
	if (m_state != 1) {
		m_idle = 0;
		return 0;
	}

	if (m_instantidle) //ignore- we are instant idling at the moment
		return 0;

	bool bIdle = (lParam & IDF_ISIDLE) != 0;
	bool bPrivacy = (lParam & IDF_PRIVACY) != 0;

	if (bPrivacy && m_idle) {
		aim_set_idle(m_hServerConn, m_seqno, 0);
		return 0;
	}

	if (bPrivacy)
		return 0;

	if (bIdle)  //don't want to change idle time if we are already idle
	{
		MIRANDA_IDLE_INFO mii;
		Idle_GetInfo(mii);

		m_idle = 1;
		aim_set_idle(m_hServerConn, m_seqno, mii.idleTime * 60);
	}
	else aim_set_idle(m_hServerConn, m_seqno, 0);

	return 0;
}

int CAimProto::OnWindowEvent(WPARAM, LPARAM lParam)
{
	MessageWindowEventData* msgEvData = (MessageWindowEventData*)lParam;

	if (msgEvData->uType == MSG_WINDOW_EVT_CLOSE) {
		if (m_state != 1 || !is_my_contact(msgEvData->hContact))
			return 0;

		if (getWord(msgEvData->hContact, AIM_KEY_ST, ID_STATUS_OFFLINE) == ID_STATUS_ONTHEPHONE)
			return 0;

		DBVARIANT dbv;
		if (!getBool(msgEvData->hContact, AIM_KEY_BLS, false) && !getString(msgEvData->hContact, AIM_KEY_SN, &dbv)) {
			if (_stricmp(dbv.pszVal, SYSTEM_BUDDY))
				aim_typing_notification(m_hServerConn, m_seqno, dbv.pszVal, 0x000f);
			db_free(&dbv);
		}
	}
	return 0;
}

INT_PTR CAimProto::GetProfile(WPARAM wParam, LPARAM)
{
	if (m_state != 1)
		return 0;

	DBVARIANT dbv;
	if (!getString(wParam, AIM_KEY_SN, &dbv)) {
		m_request_HTML_profile = 1;
		aim_query_profile(m_hServerConn, m_seqno, dbv.pszVal);
		db_free(&dbv);
	}
	return 0;
}

INT_PTR CAimProto::GetHTMLAwayMsg(WPARAM wParam, LPARAM)
{
	if (m_state != 1)
		return 0;

	DBVARIANT dbv;
	if (!getString(wParam, AIM_KEY_SN, &dbv)) {
		m_request_away_message = 1;
		aim_query_away_message(m_hServerConn, m_seqno, dbv.pszVal);
	}
	return 0;
}

int CAimProto::OnDbSettingChanged(WPARAM hContact, LPARAM lParam)
{
	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING*)lParam;

	if (strcmp(cws->szModule, MOD_KEY_CL) == 0 && m_state == 1 && hContact) {
		if (strcmp(cws->szSetting, AIM_KEY_NL) == 0) {
			if (cws->value.type == DBVT_DELETED) {
				DBVARIANT dbv;
				if (!db_get_utf(hContact, MOD_KEY_CL, OTH_KEY_GP, &dbv) && dbv.pszVal[0]) {
					add_contact_to_group(hContact, dbv.pszVal);
					db_free(&dbv);
				}
				else
					add_contact_to_group(hContact, AIM_DEFAULT_GROUP);
			}
		}
		else if (strcmp(cws->szSetting, "MyHandle") == 0) {
			char *name;
			switch (cws->value.type) {
			case DBVT_DELETED:
				set_local_nick(hContact, nullptr, nullptr);
				break;

			case DBVT_ASCIIZ:
				name = mir_utf8encode(cws->value.pszVal);
				set_local_nick(hContact, name, nullptr);
				mir_free(name);
				break;

			case DBVT_UTF8:
				set_local_nick(hContact, cws->value.pszVal, nullptr);
				break;

			case DBVT_WCHAR:
				name = mir_utf8encodeW(cws->value.pwszVal);
				set_local_nick(hContact, name, nullptr);
				mir_free(name);
				break;
			}
		}
	}

	return 0;
}

int CAimProto::OnContactDeleted(WPARAM hContact, LPARAM)
{
	if (m_state != 1)
		return 0;

	if (db_get_b(hContact, MOD_KEY_CL, AIM_KEY_NL, 0))
		return 0;

	DBVARIANT dbv;
	if (!getString(hContact, AIM_KEY_SN, &dbv)) {
		for (int i = 1;; ++i) {
			unsigned short item_id = getBuddyId(hContact, i);
			if (item_id == 0) break;

			unsigned short group_id = getGroupId(hContact, i);
			if (group_id) {
				bool is_not_in_list = getBool(hContact, AIM_KEY_NIL, false);
				aim_ssi_update(m_hServerConn, m_seqno, true);
				aim_delete_contact(m_hServerConn, m_seqno, dbv.pszVal, item_id, group_id, 0, is_not_in_list);
				char *group = m_group_list.find_name(group_id);
				update_server_group(group, group_id);
				aim_ssi_update(m_hServerConn, m_seqno, false);
			}
		}
		db_free(&dbv);
	}
	return 0;
}


int CAimProto::OnGroupChange(WPARAM hContact, LPARAM lParam)
{
	if (m_state != 1 || !getByte(AIM_KEY_MG, 1))
		return 0;

	CLISTGROUPCHANGE *grpchg = (CLISTGROUPCHANGE*)lParam;

	if (hContact == NULL) {
		if (grpchg->pszNewName == nullptr && grpchg->pszOldName != nullptr) {
			T2Utf szOldName(grpchg->pszOldName);
			unsigned short group_id = m_group_list.find_id(szOldName);
			if (group_id) {
				aim_delete_contact(m_hServerConn, m_seqno, szOldName, 0, group_id, 1, false);
				m_group_list.remove_by_id(group_id);
				update_server_group("", 0);
			}
		}
		else if (grpchg->pszNewName != nullptr && grpchg->pszOldName != nullptr) {
			unsigned short group_id = m_group_list.find_id(T2Utf(grpchg->pszOldName));
			if (group_id)
				update_server_group(T2Utf(grpchg->pszNewName), group_id);
		}
	}
	else {
		if (is_my_contact(hContact) && getBuddyId(hContact, 1) && !db_get_b(hContact, MOD_KEY_CL, AIM_KEY_NL, 0)) {
			if (grpchg->pszNewName)
				add_contact_to_group(hContact, T2Utf(grpchg->pszNewName));
			else
				add_contact_to_group(hContact, AIM_DEFAULT_GROUP);
		}
	}
	return 0;
}

INT_PTR CAimProto::AddToServerList(WPARAM hContact, LPARAM)
{
	if (m_state != 1)
		return 0;

	DBVARIANT dbv;
	if (!db_get_utf(hContact, MOD_KEY_CL, OTH_KEY_GP, &dbv) && dbv.pszVal[0]) {
		add_contact_to_group(hContact, dbv.pszVal);
		db_free(&dbv);
	}
	else add_contact_to_group(hContact, AIM_DEFAULT_GROUP);
	return 0;
}

INT_PTR CAimProto::BlockBuddy(WPARAM hContact, LPARAM)
{
	if (m_state != 1)
		return 0;

	unsigned short item_id;
	DBVARIANT dbv;
	if (getString(hContact, AIM_KEY_SN, &dbv))
		return 0;

	switch (m_pd_mode) {
	case 1:
		m_pd_mode = 4;
		aim_set_pd_info(m_hServerConn, m_seqno);

	case 4:
		item_id = m_block_list.find_id(dbv.pszVal);
		if (item_id != 0) {
			m_block_list.remove_by_id(item_id);
			aim_delete_contact(m_hServerConn, m_seqno, dbv.pszVal, item_id, 0, 3, false);
		}
		else {
			item_id = m_block_list.add(dbv.pszVal);
			aim_add_contact(m_hServerConn, m_seqno, dbv.pszVal, item_id, 0, 3, nullptr);
		}
		break;

	case 2:
		m_pd_mode = 3;
		aim_set_pd_info(m_hServerConn, m_seqno);

	case 3:
		item_id = m_allow_list.find_id(dbv.pszVal);
		if (item_id != 0) {
			m_allow_list.remove_by_id(item_id);
			aim_delete_contact(m_hServerConn, m_seqno, dbv.pszVal, item_id, 0, 2, false);
		}
		else {
			item_id = m_allow_list.add(dbv.pszVal);
			aim_add_contact(m_hServerConn, m_seqno, dbv.pszVal, item_id, 0, 2);
		}
		break;
	}
	db_free(&dbv);

	return 0;
}

INT_PTR CAimProto::JoinChatUI(WPARAM, LPARAM)
{
	DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_CHAT), nullptr, join_chat_dialog, LPARAM(this));
	return 0;
}

INT_PTR CAimProto::OnJoinChat(WPARAM hContact, LPARAM)
{
	if (m_state != 1)
		return 0;

	DBVARIANT dbv;
	if (!getString(hContact, "ChatRoomID", &dbv)) {
		chatnav_param* par = new chatnav_param(dbv.pszVal, getWord(hContact, "Exchange", 4));
		ForkThread(&CAimProto::chatnav_request_thread, par);
		db_free(&dbv);
	}
	return 0;
}

INT_PTR CAimProto::OnLeaveChat(WPARAM wParam, LPARAM)
{
	if (m_state != 1)
		return 0;

	MCONTACT hContact = wParam;

	DBVARIANT dbv;
	if (!getString(hContact, "ChatRoomID", &dbv)) {
		chat_leave(dbv.pszVal);
		db_free(&dbv);
	}
	return 0;
}

INT_PTR CAimProto::InstantIdle(WPARAM, LPARAM)
{
	DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_IDLE), nullptr, instant_idle_dialog, LPARAM(this));
	return 0;
}

INT_PTR CAimProto::ManageAccount(WPARAM, LPARAM)
{
	ShellExecuteA(nullptr, "open", "https://my.screenname.aol.com", nullptr, nullptr, SW_SHOW);
	return 0;
}

INT_PTR CAimProto::GetAvatarInfo(WPARAM wParam, LPARAM lParam)
{
	PROTO_AVATAR_INFORMATION *pai = (PROTO_AVATAR_INFORMATION*)lParam;

	pai->filename[0] = 0;
	pai->format = PA_FORMAT_UNKNOWN;

	if (getByte(AIM_KEY_DA, 0))
		return GAIR_NOAVATAR;

	switch (get_avatar_filename(pai->hContact, pai->filename, _countof(pai->filename), nullptr)) {
	case GAIR_SUCCESS:
		if (!(wParam & GAIF_FORCE) || m_state != 1)
			return GAIR_SUCCESS;

	case GAIR_WAITFOR:
		pai->format = ProtoGetAvatarFormat(pai->filename);
		break;

	default:
		return GAIR_NOAVATAR;
	}

	if (m_state == 1) {
		ForkThread(&CAimProto::avatar_request_thread, (void*)pai->hContact);
		return GAIR_WAITFOR;
	}

	return GAIR_NOAVATAR;
}

INT_PTR CAimProto::GetAvatarCaps(WPARAM wParam, LPARAM lParam)
{
	int res = 0;

	switch (wParam) {
	case AF_MAXSIZE:
		((POINT*)lParam)->x = 100;
		((POINT*)lParam)->y = 100;
		break;

	case AF_MAXFILESIZE:
		res = 11264;
		break;

	case AF_PROPORTION:
		res = PIP_SQUARE;
		break;

	case AF_FORMATSUPPORTED:
		res = (lParam == PA_FORMAT_JPEG || lParam == PA_FORMAT_GIF || lParam == PA_FORMAT_BMP);
		break;

	case AF_ENABLED:
	case AF_DONTNEEDDELAYS:
	case AF_FETCHIFPROTONOTVISIBLE:
	case AF_FETCHIFCONTACTOFFLINE:
		res = 1;
		break;
	}

	return res;
}

INT_PTR CAimProto::GetAvatar(WPARAM wParam, LPARAM lParam)
{
	wchar_t* buf = (wchar_t*)wParam;
	size_t size = (size_t)lParam;
	if (buf == nullptr || size <= 0)
		return -1;

	PROTO_AVATAR_INFORMATION ai = { 0 };
	if (GetAvatarInfo(0, (LPARAM)&ai) == GAIR_SUCCESS) {
		wcsncpy_s(buf, size, ai.filename, _TRUNCATE);
		return 0;
	}

	return -1;
}

INT_PTR CAimProto::SetAvatar(WPARAM, LPARAM lParam)
{
	wchar_t *szFileName = (wchar_t*)lParam;

	if (m_state != 1)
		return 1;

	if (szFileName == nullptr) {
		aim_ssi_update(m_hServerConn, m_seqno, true);
		aim_delete_avatar_hash(m_hServerConn, m_seqno, 1, 1, m_avatar_id_sm);
		aim_delete_avatar_hash(m_hServerConn, m_seqno, 1, 12, m_avatar_id_lg);
		aim_ssi_update(m_hServerConn, m_seqno, false);

		avatar_request_handler(NULL, nullptr, 0);
	}
	else {
		char hash[16], hash1[16], *data, *data1 = nullptr;
		unsigned short size, size1 = 0;

		if (!get_avatar_hash(szFileName, hash, &data, size)) {
			mir_free(hash);
			return 1;
		}

		rescale_image(data, size, data1, size1);

		if (size1) {
			mir_md5_state_t state;
			mir_md5_init(&state);
			mir_md5_append(&state, (unsigned char*)data1, size1);
			mir_md5_finish(&state, (unsigned char*)hash1);

			mir_free(m_hash_lg); m_hash_lg = bytes_to_string(hash, sizeof(hash));
			mir_free(m_hash_sm); m_hash_sm = bytes_to_string(hash1, sizeof(hash1));

			aim_ssi_update(m_hServerConn, m_seqno, true);
			aim_set_avatar_hash(m_hServerConn, m_seqno, 1, 1, m_avatar_id_sm, 16, hash1);
			aim_set_avatar_hash(m_hServerConn, m_seqno, 1, 12, m_avatar_id_lg, 16, hash);
			aim_ssi_update(m_hServerConn, m_seqno, false);
		}
		else {
			mir_free(m_hash_lg); m_hash_lg = nullptr;
			mir_free(m_hash_sm); m_hash_sm = bytes_to_string(hash, sizeof(hash1));

			aim_ssi_update(m_hServerConn, m_seqno, true);
			aim_set_avatar_hash(m_hServerConn, m_seqno, 1, 1, m_avatar_id_sm, 16, hash);
			aim_delete_avatar_hash(m_hServerConn, m_seqno, 1, 12, m_avatar_id_lg);
			aim_ssi_update(m_hServerConn, m_seqno, false);
		}

		avatar_request_handler(NULL, nullptr, 0);

		avatar_up_req *req = new avatar_up_req(data, size, data1, size1);
		ForkThread(&CAimProto::avatar_upload_thread, req);

		wchar_t tFileName[MAX_PATH];
		wchar_t *ext = wcsrchr(szFileName, '.');
		get_avatar_filename(NULL, tFileName, _countof(tFileName), ext);
		int fileId = _wopen(tFileName, _O_CREAT | _O_TRUNC | _O_WRONLY | O_BINARY, _S_IREAD | _S_IWRITE);
		if (fileId < 0) {
			char errmsg[512];
			mir_snprintf(errmsg, "Cannot store avatar. File '%s' could not be created/overwritten", tFileName);
			ShowPopup(errmsg, ERROR_POPUP);
			return 1;
		}
		_write(fileId, data, size);
		_close(fileId);
	}
	return 0;
}
