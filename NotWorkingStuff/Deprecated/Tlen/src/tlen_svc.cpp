/*

Jabber Protocol Plugin for Miranda IM
Tlen Protocol Plugin for Miranda NG
Copyright (C) 2002-2004  Santithorn Bunchua
Copyright (C) 2004-2007  Piotr Piastucki

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
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "resource.h"
#include "tlen_list.h"
#include "tlen_iq.h"
#include "tlen_p2p_old.h"
#include "tlen_avatar.h"
#include "tlen_file.h"

DWORD_PTR TlenProtocol::GetCaps(int type, MCONTACT)
{
	switch (type) {
	case PFLAGNUM_1:
		return PF1_IM | PF1_AUTHREQ | PF1_SERVERCLIST | PF1_MODEMSG | PF1_BASICSEARCH | PF1_SEARCHBYEMAIL | PF1_EXTSEARCH | PF1_EXTSEARCHUI | PF1_SEARCHBYNAME | PF1_FILE;//|PF1_VISLIST|PF1_INVISLIST;
	case PFLAGNUM_2:
		return PF2_ONLINE | PF2_INVISIBLE | PF2_SHORTAWAY | PF2_LONGAWAY | PF2_HEAVYDND | PF2_FREECHAT;
	case PFLAGNUM_3:
		return PF2_ONLINE | PF2_INVISIBLE | PF2_SHORTAWAY | PF2_LONGAWAY | PF2_HEAVYDND | PF2_FREECHAT;
	case PFLAGNUM_4:
		return PF4_FORCEAUTH | PF4_NOCUSTOMAUTH | PF4_SUPPORTTYPING | PF4_AVATARS | PF4_IMSENDOFFLINE | PF4_OFFLINEFILES;
	case PFLAG_UNIQUEIDTEXT:
		return (INT_PTR)Translate("Tlen login");
	case PFLAG_UNIQUEIDSETTING:
		return (INT_PTR) "jid";
	default:
		return 0;
	}
}

INT_PTR TlenProtocol::GetName(WPARAM wParam, LPARAM lParam)
{
	strncpy((char*)lParam, m_szModuleName, wParam);
	return 0;
}

int TlenRunSearch(TlenProtocol *proto) {
	int iqId = 0;
	if (!proto->isOnline) return 0;
	if (proto->searchQuery != nullptr && proto->searchIndex < 10) {
		iqId = proto->searchID;
		TlenIqAdd(proto, iqId, IQ_PROC_GETSEARCH, TlenIqResultSearch);
		if (proto->searchIndex == 0) {
			TlenSend(proto, "<iq type='get' id='" TLEN_IQID "%d' to='tuba'><query xmlns='jabber:iq:search'>%s</query></iq>", iqId, proto->searchQuery);
		}
		else {
			TlenSend(proto, "<iq type='get' id='" TLEN_IQID "%d' to='tuba'><query xmlns='jabber:iq:search'>%s<f>%d</f></query></iq>", iqId, proto->searchQuery, proto->searchIndex * TLEN_MAX_SEARCH_RESULTS_PER_PAGE);
		}
		proto->searchIndex++;
	}
	return iqId;
}

void TlenResetSearchQuery(TlenProtocol *proto) {
	if (proto->searchQuery != nullptr) {
		mir_free(proto->searchQuery);
		proto->searchQuery = nullptr;
	}
	proto->searchQueryLen = 0;
	proto->searchIndex = 0;
	proto->searchID = TlenSerialNext(proto);
}

HANDLE TlenProtocol::SearchBasic(const wchar_t* id)
{
	int iqId = 0;
	if (!isOnline) return nullptr;
	if (id == nullptr) return nullptr;
	char* id_A = mir_u2a(id);
	char *jid = TlenTextEncode(id_A);
	if (jid != nullptr) {
		searchJID = mir_strdup(id_A);
		TlenResetSearchQuery(this);
		TlenStringAppend(&searchQuery, &searchQueryLen, "<i>%s</i>", jid);
		iqId = TlenRunSearch(this);
		mir_free(jid);
	}
	mir_free(id_A);
	return (HANDLE)iqId;
}

HANDLE TlenProtocol::SearchByEmail(const wchar_t* email)
{
	int iqId = 0;

	if (!isOnline) return nullptr;
	if (email == nullptr) return nullptr;

	char* email_A = mir_u2a(email);
	char *emailEnc = TlenTextEncode(email_A);
	if (emailEnc != nullptr) {
		TlenResetSearchQuery(this);
		TlenStringAppend(&searchQuery, &searchQueryLen, "<email>%s</email>", emailEnc);
		iqId = TlenRunSearch(this);
		mir_free(emailEnc);
	}
	mir_free(email_A);
	return (HANDLE)iqId;
}

HANDLE TlenProtocol::SearchByName(const wchar_t* nickT, const wchar_t* firstNameT, const wchar_t* lastNameT)
{
	if (!isOnline) return nullptr;

	char *nick = mir_u2a(nickT);
	char *firstName = mir_u2a(firstNameT);
	char *lastName = mir_u2a(lastNameT);

	char *p;
	int iqId = 0;

	TlenResetSearchQuery(this);

	if (nick != nullptr && nick[0] != '\0') {
		if ((p = TlenTextEncode(nick)) != nullptr) {
			TlenStringAppend(&searchQuery, &searchQueryLen, "<nick>%s</nick>", p);
			mir_free(p);
		}
	}
	if (firstName != nullptr && firstName[0] != '\0') {
		if ((p = TlenTextEncode(firstName)) != nullptr) {
			TlenStringAppend(&searchQuery, &searchQueryLen, "<first>%s</first>", p);
			mir_free(p);
		}
	}
	if (lastName != nullptr && lastName[0] != '\0') {
		if ((p = TlenTextEncode(lastName)) != nullptr) {
			TlenStringAppend(&searchQuery, &searchQueryLen, "<last>%s</last>", p);
			mir_free(p);
		}
	}

	iqId = TlenRunSearch(this);
	return (HANDLE)iqId;
}

HWND TlenProtocol::CreateExtendedSearchUI(HWND owner)
{
	return (HWND)CreateDialog(hInst, MAKEINTRESOURCE(IDD_ADVSEARCH), owner, TlenAdvSearchDlgProc);
}

HWND TlenProtocol::SearchAdvanced(HWND owner)
{
	if (!isOnline) return nullptr;

	TlenResetSearchQuery(this);
	int iqId = TlenSerialNext(this);
	if ((searchQuery = TlenAdvSearchCreateQuery(owner, iqId)) != nullptr) {
		iqId = TlenRunSearch(this);
	}
	return (HWND)iqId;
}


static MCONTACT AddToListByJID(TlenProtocol *proto, const char *newJid, DWORD flags)
{
	MCONTACT hContact = TlenHContactFromJID(proto, newJid);
	if (hContact == NULL) {
		// not already there: add
		char *jid = mir_strdup(newJid); _strlwr(jid);
		hContact = db_add_contact();
		Proto_AddToContact(hContact, proto->m_szModuleName);
		db_set_s(hContact, proto->m_szModuleName, "jid", jid);
		char *nick = TlenNickFromJID(newJid);
		if (nick == nullptr)
			nick = mir_strdup(newJid);
		db_set_s(hContact, "CList", "MyHandle", nick);
		mir_free(nick);
		mir_free(jid);

		// Note that by removing or disable the "NotOnList" will trigger
		// the plugin to add a particular contact to the roster list.
		// See DBSettingChanged hook at the bottom part of this source file.
		// But the add module will delete "NotOnList". So we will not do it here.
		// Also because we need "MyHandle" and "Group" info, which are set after
		// PS_ADDTOLIST is called but before the add dialog issue deletion of
		// "NotOnList".
		// If temporary add, "NotOnList" won't be deleted, and that's expected.
		db_set_b(hContact, "CList", "NotOnList", 1);
		if (flags & PALF_TEMPORARY)
			db_set_b(hContact, "CList", "Hidden", 1);
	}
	else {
		// already exist
		// Set up a dummy "NotOnList" when adding permanently only
		if (!(flags&PALF_TEMPORARY))
			db_set_b(hContact, "CList", "NotOnList", 1);
	}

	return hContact;
}

MCONTACT TlenProtocol::AddToList(int flags, PROTOSEARCHRESULT *psr)
{
	TLEN_SEARCH_RESULT *jsr = (TLEN_SEARCH_RESULT*)psr;
	if (jsr->hdr.cbSize != sizeof(TLEN_SEARCH_RESULT))
		return NULL;
	return AddToListByJID(this, jsr->jid, flags);// wParam is flag e.g. PALF_TEMPORARY
}

MCONTACT TlenProtocol::AddToListByEvent(int flags, int, MEVENT hDbEvent)
{
	DBEVENTINFO dbei = {};
	if ((dbei.cbBlob = db_event_getBlobSize(hDbEvent)) == (DWORD)(-1))
		return NULL;
	if ((dbei.pBlob = (PBYTE)mir_alloc(dbei.cbBlob)) == nullptr)
		return NULL;
	if (db_event_get(hDbEvent, &dbei)) {
		mir_free(dbei.pBlob);
		return NULL;
	}
	if (mir_strcmp(dbei.szModule, m_szModuleName)) {
		mir_free(dbei.pBlob);
		return NULL;
	}

	/*
		// EVENTTYPE_CONTACTS is when adding from when we receive contact list (not used in Tlen)
		// EVENTTYPE_ADDED is when adding from when we receive "You are added" (also not used in Tlen)
		// Tlen will only handle the case of EVENTTYPE_AUTHREQUEST
		// EVENTTYPE_AUTHREQUEST is when adding from the authorization request dialog
	*/

	if (dbei.eventType != EVENTTYPE_AUTHREQUEST) {
		mir_free(dbei.pBlob);
		return NULL;
	}

	DB_AUTH_BLOB blob(dbei.pBlob);
	MCONTACT hContact = (MCONTACT)AddToListByJID(this, blob.get_email(), flags);
	mir_free(dbei.pBlob);
	return hContact;
}

int TlenProtocol::Authorize(MEVENT hDbEvent)
{
	if (!isOnline)
		return 1;

	DBEVENTINFO dbei = {};
	if ((dbei.cbBlob = db_event_getBlobSize(hDbEvent)) == (DWORD)-1)
		return 1;
	if ((dbei.pBlob = (PBYTE)mir_alloc(dbei.cbBlob)) == nullptr)
		return 1;
	if (db_event_get(hDbEvent, &dbei)) {
		mir_free(dbei.pBlob);
		return 1;
	}
	if (dbei.eventType != EVENTTYPE_AUTHREQUEST) {
		mir_free(dbei.pBlob);
		return 1;
	}
	if (mir_strcmp(dbei.szModule, m_szModuleName)) {
		mir_free(dbei.pBlob);
		return 1;
	}

	DB_AUTH_BLOB blob(dbei.pBlob);
	TlenSend(this, "<presence to='%s' type='subscribed'/>", blob.get_email());

	// Automatically add this user to my roster if option is enabled
	if (db_get_b(NULL, m_szModuleName, "AutoAdd", TRUE) == TRUE) {
		MCONTACT hContact;
		TLEN_LIST_ITEM *item = TlenListGetItemPtr(this, LIST_ROSTER, blob.get_email());

		if (item == nullptr || (item->subscription != SUB_BOTH && item->subscription != SUB_TO)) {
			debugLogA("Try adding contact automatically jid=%s", blob.get_email());
			if ((hContact = AddToListByJID(this, blob.get_email(), 0)) != NULL) {
				// Trigger actual add by removing the "NotOnList" added by AddToListByJID()
				// See AddToListByJID() and TlenDbSettingChanged().
				db_unset(hContact, "CList", "NotOnList");
			}
		}
	}

	mir_free(dbei.pBlob);
	return 0;
}

int TlenProtocol::AuthDeny(MEVENT hDbEvent, const wchar_t*)
{
	if (!isOnline)
		return 1;

	DBEVENTINFO dbei = {};
	if ((dbei.cbBlob = db_event_getBlobSize(hDbEvent)) == (DWORD)(-1))
		return 1;
	if ((dbei.pBlob = (PBYTE)mir_alloc(dbei.cbBlob)) == nullptr)
		return 1;
	if (db_event_get(hDbEvent, &dbei)) {
		mir_free(dbei.pBlob);
		return 1;
	}
	if (dbei.eventType != EVENTTYPE_AUTHREQUEST) {
		mir_free(dbei.pBlob);
		return 1;
	}
	if (mir_strcmp(dbei.szModule, m_szModuleName)) {
		mir_free(dbei.pBlob);
		return 1;
	}

	DB_AUTH_BLOB blob(dbei.pBlob);
	TlenSend(this, "<presence to='%s' type='unsubscribed'/>", blob.get_email());
	TlenSend(this, "<iq type='set'><query xmlns='jabber:iq:roster'><item jid='%s' subscription='remove'/></query></iq>", blob.get_email());
	mir_free(dbei.pBlob);
	return 0;
}

static void TlenConnect(TlenProtocol *proto, int initialStatus)
{
	if (!proto->isConnected) {
		ThreadData *thread = (ThreadData *)mir_alloc(sizeof(ThreadData));
		memset(thread, 0, sizeof(ThreadData));
		thread->proto = proto;
		proto->m_iDesiredStatus = initialStatus;

		int oldStatus = proto->m_iStatus;
		proto->m_iStatus = ID_STATUS_CONNECTING;
		ProtoBroadcastAck(proto->m_szModuleName, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)oldStatus, proto->m_iStatus);
		thread->hThread = mir_forkthread((pThreadFunc)TlenServerThread, thread);
	}
}

int TlenProtocol::SetStatus(int iNewStatus)
{
	int oldStatus;
	HANDLE s;

	m_iDesiredStatus = iNewStatus;

	if (iNewStatus == ID_STATUS_OFFLINE) {
		if (threadData) {
			if (isConnected) {
				TlenSendPresence(this, ID_STATUS_OFFLINE);
			}

			// TODO bug? s = proto;
			s = threadData->s;

			threadData = nullptr;
			if (isConnected) {
				Sleep(200);
				//				TlenSend(s, "</s>");
								// Force closing connection
				isConnected = FALSE;
				isOnline = FALSE;
				Netlib_CloseHandle(s);
			}
		}
		else {
			if (m_iStatus != ID_STATUS_OFFLINE) {
				oldStatus = m_iStatus;
				m_iStatus = ID_STATUS_OFFLINE;
				ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)oldStatus, m_iStatus);
			}
		}
	}
	else if (iNewStatus != m_iStatus) {
		if (!isConnected) {
			TlenConnect(this, iNewStatus);
		}
		else {
			// change status
			oldStatus = m_iStatus;
			// send presence update
			TlenSendPresence(this, iNewStatus);
			ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)oldStatus, m_iStatus);
		}
	}
	return 0;
}

INT_PTR TlenProtocol::GetStatus(WPARAM, LPARAM)
{
	return m_iStatus;
}

int TlenProtocol::SetAwayMsg(int iStatus, const wchar_t* msg)
{
	char **szMsg;
	char *newModeMsg;

	newModeMsg = mir_u2a(msg);

	debugLogA("SetAwayMsg called, wParam=%d lParam=%s", iStatus, newModeMsg);

	mir_cslock lck(modeMsgMutex);

	switch (iStatus) {
	case ID_STATUS_ONLINE:
		szMsg = &modeMsgs.szOnline;
		break;
	case ID_STATUS_AWAY:
	case ID_STATUS_ONTHEPHONE:
	case ID_STATUS_OUTTOLUNCH:
		szMsg = &modeMsgs.szAway;
		break;
	case ID_STATUS_NA:
		szMsg = &modeMsgs.szNa;
		break;
	case ID_STATUS_DND:
	case ID_STATUS_OCCUPIED:
		szMsg = &modeMsgs.szDnd;
		break;
	case ID_STATUS_FREECHAT:
		szMsg = &modeMsgs.szFreechat;
		break;
	case ID_STATUS_INVISIBLE:
		szMsg = &modeMsgs.szInvisible;
		break;
	default:
		return 1;
	}

	if ((*szMsg == nullptr && newModeMsg == nullptr) ||
		(*szMsg != nullptr && newModeMsg != nullptr && !mir_strcmp(*szMsg, newModeMsg))) {
		// Message is the same, no update needed
		if (newModeMsg != nullptr) mir_free(newModeMsg);
	}
	else {
		// Update with the new mode message
		if (*szMsg != nullptr) mir_free(*szMsg);
		*szMsg = newModeMsg;
		// Send a presence update if needed
		if (iStatus == m_iStatus) {
			TlenSendPresence(this, m_iStatus);
		}
	}

	return 0;
}

int TlenProtocol::GetInfo(MCONTACT hContact, int)
{
	DBVARIANT dbv;
	int iqId;
	char *nick, *pNick;

	if (!isOnline) return 1;
	if (hContact == NULL) {
		iqId = TlenSerialNext(this);
		TlenIqAdd(this, iqId, IQ_PROC_NONE, TlenIqResultVcard);
		TlenSend(this, "<iq type='get' id='" TLEN_IQID "%d' to='tuba'><query xmlns='jabber:iq:register'></query></iq>", iqId);
	}
	else {
		if (db_get(hContact, m_szModuleName, "jid", &dbv)) return 1;
		if ((nick = TlenNickFromJID(dbv.pszVal)) != nullptr) {
			if ((pNick = TlenTextEncode(nick)) != nullptr) {
				iqId = TlenSerialNext(this);
				TlenIqAdd(this, iqId, IQ_PROC_NONE, TlenIqResultVcard);
				TlenSend(this, "<iq type='get' id='" TLEN_IQID "%d' to='tuba'><query xmlns='jabber:iq:search'><i>%s</i></query></iq>", iqId, pNick);
				mir_free(pNick);
			}
			mir_free(nick);
		}
		db_free(&dbv);
	}
	return 0;
}

int TlenProtocol::SetApparentMode(MCONTACT hContact, int mode)
{
	DBVARIANT dbv;
	int oldMode;
	char *jid;

	if (!isOnline) return 0;
	if (!db_get_b(NULL, m_szModuleName, "VisibilitySupport", FALSE)) return 0;
	if (mode != 0 && mode != ID_STATUS_ONLINE && mode != ID_STATUS_OFFLINE) return 1;
	oldMode = db_get_w(hContact, m_szModuleName, "ApparentMode", 0);
	if ((int)mode == oldMode) return 1;
	db_set_w(hContact, m_szModuleName, "ApparentMode", (WORD)mode);
	if (!db_get(hContact, m_szModuleName, "jid", &dbv)) {
		jid = dbv.pszVal;
		switch (mode) {
		case ID_STATUS_ONLINE:
			if (m_iStatus == ID_STATUS_INVISIBLE || oldMode == ID_STATUS_OFFLINE)
				TlenSend(this, "<presence to='%s'><show>available</show></presence>", jid);
			break;
		case ID_STATUS_OFFLINE:
			if (m_iStatus != ID_STATUS_INVISIBLE || oldMode == ID_STATUS_ONLINE)
				TlenSend(this, "<presence to='%s' type='invisible'/>", jid);
			break;
		case 0:
			if (oldMode == ID_STATUS_ONLINE && m_iStatus == ID_STATUS_INVISIBLE)
				TlenSend(this, "<presence to='%s' type='invisible'/>", jid);
			else if (oldMode == ID_STATUS_OFFLINE && m_iStatus != ID_STATUS_INVISIBLE)
				TlenSend(this, "<presence to='%s'><show>available</show></presence>", jid);
			break;
		}
		db_free(&dbv);
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

struct SENDACKTHREADDATA
{
	__inline SENDACKTHREADDATA(TlenProtocol *_ppro, MCONTACT _hContact, int _msgid = 0) :
		proto(_ppro), hContact(_hContact), msgid(_msgid)
	{}

	TlenProtocol *proto;
	MCONTACT hContact;
	int msgid;
};

static void __cdecl TlenSendMessageAckThread(void *ptr)
{
	SENDACKTHREADDATA *data = (SENDACKTHREADDATA *)ptr;
	SleepEx(10, TRUE);
	ProtoBroadcastAck(data->proto->m_szModuleName, data->hContact, ACKTYPE_MESSAGE, ACKRESULT_SUCCESS, (HANDLE)data->msgid, 0);
	delete data;
}

static void __cdecl TlenSendMessageFailedThread(void *ptr)
{
	SENDACKTHREADDATA *data = (SENDACKTHREADDATA *)ptr;
	SleepEx(10, TRUE);
	ProtoBroadcastAck(data->proto->m_szModuleName, data->hContact, ACKTYPE_MESSAGE, ACKRESULT_FAILED, (HANDLE)data->msgid, 0);
	delete data;
}

static void __cdecl TlenGetAwayMsgThread(void *ptr)
{
	DBVARIANT dbv;
	SENDACKTHREADDATA *data = (SENDACKTHREADDATA *)ptr;

	Sleep(50);

	if (!db_get(data->hContact, data->proto->m_szModuleName, "jid", &dbv)) {
		TLEN_LIST_ITEM *item = TlenListGetItemPtr(data->proto, LIST_ROSTER, dbv.pszVal);
		if (item != nullptr) {
			db_free(&dbv);
			ProtoBroadcastAck(data->proto->m_szModuleName, data->hContact, ACKTYPE_AWAYMSG, ACKRESULT_SUCCESS, (HANDLE)1,
				item->statusMessage == NULL ? NULL : _A2T(item->statusMessage));
		}
		else {
			ptrA ownJid(db_get_sa(NULL, data->proto->m_szModuleName, "jid"));
			if (!mir_strcmp(ownJid, dbv.pszVal)) {
				DBVARIANT dbv2;
				if (!db_get_s(data->hContact, "CList", "StatusMsg", &dbv2, DBVT_WCHAR)) {
					data->proto->ProtoBroadcastAck(data->hContact, ACKTYPE_AWAYMSG, ACKRESULT_SUCCESS, (HANDLE)1, (LPARAM)dbv2.ptszVal);
					db_free(&dbv2);
				}
				else {
					data->proto->ProtoBroadcastAck(data->hContact, ACKTYPE_AWAYMSG, ACKRESULT_SUCCESS, (HANDLE)1, NULL);
				}
			}
			db_free(&dbv);
		}
	}
	else {
		data->proto->ProtoBroadcastAck(data->hContact, ACKTYPE_AWAYMSG, ACKRESULT_SUCCESS, (HANDLE)1, NULL);
	}

	delete data;
}

INT_PTR TlenProtocol::SendAlert(WPARAM hContact, LPARAM)
{
	DBVARIANT dbv;
	if (isOnline && !db_get(hContact, m_szModuleName, "jid", &dbv)) {
		TlenSend(this, "<m tp='a' to='%s'/>", dbv.pszVal);

		db_free(&dbv);
	}
	return 0;
}

int TlenProtocol::SendMsg(MCONTACT hContact, int, const char* msgRAW)
{
	DBVARIANT dbv;
	if (!isOnline || db_get(hContact, m_szModuleName, "jid", &dbv)) {
		mir_forkthread(TlenSendMessageFailedThread, new SENDACKTHREADDATA(this, hContact, 2));
		return 2;
	}

	TLEN_LIST_ITEM *item;
	char msgType[16];

	char* msg = mir_utf8decodeA(msgRAW);
	int id = TlenSerialNext(this);

	if (!mir_strcmp(msg, "<alert>")) {
		TlenSend(this, "<m tp='a' to='%s'/>", dbv.pszVal);
		mir_forkthread(TlenSendMessageAckThread, new SENDACKTHREADDATA(this, hContact, id));
	}
	else if (!mir_strcmp(msg, "<image>")) {
		TlenSend(this, "<message to='%s' type='%s' crc='%x' idt='%d'/>", dbv.pszVal, "pic", 0x757f044, id);
		mir_forkthread(TlenSendMessageAckThread, new SENDACKTHREADDATA(this, hContact, id));
	}
	else {
		char *msgEnc = TlenTextEncode(msg);
		if (msgEnc != nullptr) {
			if (TlenListExist(this, LIST_CHATROOM, dbv.pszVal) && strchr(dbv.pszVal, '/') == nullptr)
				mir_strcpy(msgType, "groupchat");
			else if (db_get_b(hContact, m_szModuleName, "bChat", FALSE))
				mir_strcpy(msgType, "privchat");
			else
				mir_strcpy(msgType, "chat");

			if (!mir_strcmp(msgType, "groupchat") || db_get_b(NULL, m_szModuleName, "MsgAck", FALSE) == FALSE) {
				if (!mir_strcmp(msgType, "groupchat"))
					TlenSend(this, "<message to='%s' type='%s'><body>%s</body></message>", dbv.pszVal, msgType, msgEnc);
				else if (!mir_strcmp(msgType, "privchat"))
					TlenSend(this, "<m to='%s'><b n='6' s='10' f='0' c='000000'>%s</b></m>", dbv.pszVal, msgEnc);
				else
					TlenSend(this, "<message to='%s' type='%s' id='" TLEN_IQID "%d'><body>%s</body><x xmlns='jabber:x:event'><composing/></x></message>", dbv.pszVal, msgType, id, msgEnc);

				mir_forkthread(TlenSendMessageAckThread, new SENDACKTHREADDATA(this, hContact, id));
			}
			else {
				if ((item = TlenListGetItemPtr(this, LIST_ROSTER, dbv.pszVal)) != nullptr)
					item->idMsgAckPending = id;
				TlenSend(this, "<message to='%s' type='%s' id='" TLEN_IQID "%d'><body>%s</body><x xmlns='jabber:x:event'><offline/><delivered/><composing/></x></message>", dbv.pszVal, msgType, id, msgEnc);
			}
		}
		mir_free(msgEnc);
	}

	mir_free(msg);
	db_free(&dbv);
	return id;
}

/////////////////////////////////////////////////////////////////////////////////////////
// TlenGetAvatarInfo - retrieves the avatar info

INT_PTR TlenProtocol::GetAvatarInfo(WPARAM wParam, LPARAM lParam)
{
	if (!tlenOptions.enableAvatars) return GAIR_NOAVATAR;
	BOOL downloadingAvatar = FALSE;
	char *avatarHash = nullptr;
	TLEN_LIST_ITEM *item = nullptr;
	DBVARIANT dbv;
	PROTO_AVATAR_INFORMATION *pai = (PROTO_AVATAR_INFORMATION*)lParam;

	if (pai->hContact != NULL) {
		if (!db_get(pai->hContact, m_szModuleName, "jid", &dbv)) {
			item = TlenListGetItemPtr(this, LIST_ROSTER, dbv.pszVal);
			db_free(&dbv);
			if (item != nullptr) {
				downloadingAvatar = item->newAvatarDownloading;
				avatarHash = item->avatarHash;
			}
		}
	}
	else if (threadData != nullptr)
		avatarHash = threadData->avatarHash;

	if ((avatarHash == nullptr || avatarHash[0] == '\0') && !downloadingAvatar)
		return GAIR_NOAVATAR;

	if (avatarHash != nullptr && !downloadingAvatar) {
		TlenGetAvatarFileName(this, item, pai->filename, _countof(pai->filename) - 1);
		pai->format = (pai->hContact == NULL) ? threadData->avatarFormat : item->avatarFormat;
		return GAIR_SUCCESS;
	}

	/* get avatar */
	if ((wParam & GAIF_FORCE) != 0 && pai->hContact != NULL && isOnline)
		return GAIR_WAITFOR;

	return GAIR_NOAVATAR;
}

HANDLE TlenProtocol::GetAwayMsg(MCONTACT hContact)
{
	SENDACKTHREADDATA *tdata = new SENDACKTHREADDATA(this, hContact, 0);
	mir_forkthread((pThreadFunc)TlenGetAwayMsgThread, tdata);
	return (HANDLE)1;
}

int TlenProtocol::RecvAwayMsg(MCONTACT, int, PROTORECVEVENT*)
{
	return 0;
}

HANDLE TlenProtocol::FileAllow(MCONTACT, HANDLE hTransfer, const wchar_t* szPath)
{
	if (!isOnline) return nullptr;

	TLEN_FILE_TRANSFER *ft = (TLEN_FILE_TRANSFER *)hTransfer;
	ft->szSavePath = mir_strdup(mir_u2a(szPath));	//TODO convert to wchar_t*
	TLEN_LIST_ITEM *item = TlenListAdd(this, LIST_FILE, ft->iqId);
	if (item != nullptr) {
		item->ft = ft;
	}
	char *nick = TlenNickFromJID(ft->jid);
	if (ft->newP2P) {
		TlenSend(this, "<iq to='%s'><query xmlns='p2p'><fs t='%s' e='5' i='%s' v='1'/></query></iq>", ft->jid, ft->jid, ft->iqId);
	}
	else {
		TlenSend(this, "<f t='%s' i='%s' e='5' v='1'/>", nick, ft->iqId);
	}
	mir_free(nick);
	return (HANDLE)hTransfer;
}

int TlenProtocol::FileDeny(MCONTACT, HANDLE hTransfer, const wchar_t*)
{
	if (!isOnline) return 1;

	TLEN_FILE_TRANSFER *ft = (TLEN_FILE_TRANSFER *)hTransfer;
	char *nick = TlenNickFromJID(ft->jid);
	TlenSend(this, "<f i='%s' e='4' t='%s'/>", ft->iqId, nick); \
	mir_free(nick);
	TlenP2PFreeFileTransfer(ft);
	return 0;
}

int TlenProtocol::FileResume(HANDLE, int*, const wchar_t**)
{
	return 0;
}

int TlenProtocol::FileCancel(MCONTACT, HANDLE hTransfer)
{
	TLEN_FILE_TRANSFER *ft = (TLEN_FILE_TRANSFER *)hTransfer;
	debugLogA("Invoking FileCancel()");
	if (ft->s != nullptr) {
		ft->state = FT_ERROR;
		Netlib_CloseHandle(ft->s);
		ft->s = nullptr;
		if (ft->hFileEvent != nullptr) {
			HANDLE hEvent = ft->hFileEvent;
			ft->hFileEvent = nullptr;
			SetEvent(hEvent);
		}
	}
	else {
		TlenP2PFreeFileTransfer(ft);
	}
	return 0;
}

HANDLE TlenProtocol::SendFile(MCONTACT hContact, const wchar_t* szDescription, wchar_t** ppszFiles)
{
	int i, j;
	struct _stat statbuf;
	DBVARIANT dbv;
	char *nick, *p, idStr[10];

	if (!isOnline) return nullptr;
	//	if (db_get_w(ccs->hContact, m_szModuleName, "Status", ID_STATUS_OFFLINE) == ID_STATUS_OFFLINE) return 0;
	if (db_get(hContact, m_szModuleName, "jid", &dbv)) return nullptr;
	TLEN_FILE_TRANSFER *ft = TlenFileCreateFT(this, dbv.pszVal);
	for (ft->fileCount = 0; ppszFiles[ft->fileCount]; ft->fileCount++);
	ft->files = (char **)mir_alloc(sizeof(char *) * ft->fileCount);
	ft->filesSize = (long *)mir_alloc(sizeof(long) * ft->fileCount);
	ft->allFileTotalSize = 0;
	for (i = j = 0; i < ft->fileCount; i++) {
		char* ppszFiles_i_A = mir_u2a(ppszFiles[i]);
		if (_stat(ppszFiles_i_A, &statbuf))
			debugLogA("'%s' is an invalid filename", ppszFiles[i]);
		else {
			ft->filesSize[j] = statbuf.st_size;
			ft->files[j++] = mir_strdup(ppszFiles_i_A);
			ft->allFileTotalSize += statbuf.st_size;
		}
		mir_free(ppszFiles_i_A);
	}
	ft->fileCount = j;
	ft->szDescription = mir_u2a(szDescription);
	ft->hContact = hContact;
	ft->currentFile = 0;
	db_free(&dbv);

	int id = TlenSerialNext(this);
	mir_snprintf(idStr, "%d", id);
	TLEN_LIST_ITEM *item = TlenListAdd(this, LIST_FILE, idStr);
	if (item != nullptr) {
		ft->iqId = mir_strdup(idStr);
		nick = TlenNickFromJID(ft->jid);
		item->ft = ft;
		if (tlenOptions.useNewP2P) {
			TlenSend(this, "<iq to='%s'><query xmlns='p2p'><fs t='%s' e='1' i='%s' c='%d' s='%d' v='%d'/></query></iq>",
				ft->jid, ft->jid, idStr, ft->fileCount, ft->allFileTotalSize, ft->fileCount);

			ft->newP2P = TRUE;
		}
		else {
			if (ft->fileCount == 1) {
				char* ppszFiles_0_A = mir_u2a(ppszFiles[0]);
				if ((p = strrchr(ppszFiles_0_A, '\\')) != nullptr) {
					p++;
				}
				else {
					p = ppszFiles_0_A;
				}
				p = TlenTextEncode(p);
				TlenSend(this, "<f t='%s' n='%s' e='1' i='%s' c='1' s='%d' v='1'/>", nick, p, idStr, ft->allFileTotalSize);
				mir_free(ppszFiles[0]);
				mir_free(p);
			}
			else {
				TlenSend(this, "<f t='%s' e='1' i='%s' c='%d' s='%d' v='1'/>", nick, idStr, ft->fileCount, ft->allFileTotalSize);
			}
		}
		mir_free(nick);
	}

	return (HANDLE)ft;
}

static char* settingToChar(DBCONTACTWRITESETTING* cws)
{
	switch (cws->value.type) {
	case DBVT_ASCIIZ:
		return mir_strdup(cws->value.pszVal);
	case DBVT_UTF8:
		return mir_utf8decode(mir_strdup(cws->value.pszVal), nullptr);
	}
	return nullptr;
}

int TlenProtocol::TlenDbSettingChanged(WPARAM wParam, LPARAM lParam)
{
	MCONTACT hContact = (MCONTACT)wParam;
	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING *)lParam;
	// no action for hContact == NULL or when offline
	if (hContact == NULL) return 0;
	if (!isConnected) return 0;

	if (!strcmp(cws->szModule, "CList")) {
		DBVARIANT dbv;
		TLEN_LIST_ITEM *item;
		char *nick, *jid, *group;

		char *szProto = GetContactProto(hContact);
		if (szProto == nullptr || strcmp(szProto, m_szModuleName)) return 0;
		// A contact's group is changed
		if (!strcmp(cws->szSetting, "Group")) {
			if (!db_get(hContact, m_szModuleName, "jid", &dbv)) {
				if ((item = TlenListGetItemPtr(this, LIST_ROSTER, dbv.pszVal)) != nullptr) {
					db_free(&dbv);
					if (!db_get(hContact, "CList", "MyHandle", &dbv)) {
						nick = TlenTextEncode(dbv.pszVal);
						db_free(&dbv);
					}
					else if (!db_get(hContact, this->m_szModuleName, "Nick", &dbv)) {
						nick = TlenTextEncode(dbv.pszVal);
						db_free(&dbv);
					}
					else nick = TlenNickFromJID(item->jid);

					if (nick != nullptr) {
						// Note: we need to compare with item->group to prevent infinite loop
						if (cws->value.type == DBVT_DELETED && item->group != nullptr) {
							debugLogA("Group set to nothing");
							TlenSend(this, "<iq type='set'><query xmlns='jabber:iq:roster'><item name='%s' jid='%s'></item></query></iq>", nick, item->jid);
						}
						else if (cws->value.pszVal != nullptr) {
							char *newGroup = settingToChar(cws);
							if (item->group == nullptr || strcmp(newGroup, item->group)) {
								debugLogA("Group set to %s", newGroup);
								if ((group = TlenGroupEncode(newGroup)) != nullptr) {
									TlenSend(this, "<iq type='set'><query xmlns='jabber:iq:roster'><item name='%s' jid='%s'><group>%s</group></item></query></iq>", nick, item->jid, group);
									mir_free(group);
								}
							}
							mir_free(newGroup);
						}
						mir_free(nick);
					}
				}
				else {
					db_free(&dbv);
				}
			}
		}
		// A contact is renamed
		else if (!strcmp(cws->szSetting, "MyHandle")) {
			char *newNick;

			//			hContact = (MCONTACT) wParam;
			//			szProto = GetContactProto(hContact);
			//			if (szProto == NULL || strcmp(szProto, proto->m_szModuleName)) return 0;

			if (!db_get(hContact, m_szModuleName, "jid", &dbv)) {
				jid = dbv.pszVal;
				if ((item = TlenListGetItemPtr(this, LIST_ROSTER, dbv.pszVal)) != nullptr) {
					if (cws->value.type == DBVT_DELETED) {
						newNick = mir_u2a(pcli->pfnGetContactDisplayName(hContact, GCDNF_NOMYHANDLE));
					}
					else if (cws->value.pszVal != nullptr) {
						newNick = settingToChar(cws);
					}
					else {
						newNick = nullptr;
					}
					// Note: we need to compare with item->nick to prevent infinite loop
					if (newNick != nullptr && (item->nick == nullptr || (item->nick != nullptr && strcmp(item->nick, newNick)))) {
						if ((nick = TlenTextEncode(newNick)) != nullptr) {
							debugLogA("Nick set to %s", newNick);
							if (item->group != nullptr && (group = TlenGroupEncode(item->group)) != nullptr) {
								TlenSend(this, "<iq type='set'><query xmlns='jabber:iq:roster'><item name='%s' jid='%s'><group>%s</group></item></query></iq>", nick, jid, group);
								mir_free(group);
							}
							else {
								TlenSend(this, "<iq type='set'><query xmlns='jabber:iq:roster'><item name='%s' jid='%s'></item></query></iq>", nick, jid);
							}
							mir_free(nick);
						}
					}
					if (newNick != nullptr) mir_free(newNick);
				}
				db_free(&dbv);
			}
		}
		// A temporary contact has been added permanently
		else if (!strcmp(cws->szSetting, "NotOnList")) {
			if (cws->value.type == DBVT_DELETED || (cws->value.type == DBVT_BYTE && cws->value.bVal == 0)) {
				ptrA szJid(db_get_sa(hContact, m_szModuleName, "jid"));
				if (szJid != NULL) {
					char *szNick, *pGroup;
					debugLogA("Add %s permanently to list", szJid);
					if (!db_get(hContact, "CList", "MyHandle", &dbv)) {
						szNick = TlenTextEncode(dbv.pszVal); //Utf8Encode
						db_free(&dbv);
					}
					else szNick = TlenNickFromJID(szJid);

					if (szNick != nullptr) {
						debugLogA("jid=%s szNick=%s", szJid, szNick);
						if (!db_get(hContact, "CList", "Group", &dbv)) {
							if ((pGroup = TlenGroupEncode(dbv.pszVal)) != nullptr) {
								TlenSend(this, "<iq type='set'><query xmlns='jabber:iq:roster'><item name='%s' szJid='%s'><group>%s</group></item></query></iq>", szNick, szJid, pGroup);
								TlenSend(this, "<presence to='%s' type='subscribe'/>", szJid);
								mir_free(pGroup);
							}
							db_free(&dbv);
						}
						else {
							TlenSend(this, "<iq type='set'><query xmlns='jabber:iq:roster'><item name='%s' szJid='%s'/></query></iq>", szNick, szJid);
							TlenSend(this, "<presence to='%s' type='subscribe'/>", szJid);
						}
						mir_free(szNick);
						db_unset(hContact, "CList", "Hidden");
					}
				}
			}
		}
	}

	return 0;
}

int TlenProtocol::TlenContactDeleted(WPARAM wParam, LPARAM)
{
	if (!isOnline)	// should never happen
		return 0;

	char *szProto = GetContactProto(wParam);
	if (szProto == nullptr || mir_strcmp(szProto, m_szModuleName))
		return 0;

	DBVARIANT dbv;
	if (!db_get(wParam, m_szModuleName, "jid", &dbv)) {
		char *p, *q;

		char *jid = dbv.pszVal;
		if ((p = strchr(jid, '@')) != nullptr) {
			if ((q = strchr(p, '/')) != nullptr)
				*q = '\0';
		}

		// Remove from roster, server also handles the presence unsubscription process.
		if (TlenListExist(this, LIST_ROSTER, jid))
			TlenSend(this, "<iq type='set'><query xmlns='jabber:iq:roster'><item jid='%s' subscription='remove'/></query></iq>", jid);

		db_free(&dbv);
	}
	return 0;
}

int TlenProtocol::UserIsTyping(MCONTACT hContact, int type)
{
	DBVARIANT dbv;

	if (!isOnline) return 0;
	if (!db_get(hContact, m_szModuleName, "jid", &dbv)) {
		TLEN_LIST_ITEM *item = TlenListGetItemPtr(this, LIST_ROSTER, dbv.pszVal);
		if (item != nullptr /*&& item->wantComposingEvent == TRUE*/) {
			switch (type) {
			case PROTOTYPE_SELFTYPING_OFF:
				TlenSend(this, "<m tp='u' to='%s'/>", dbv.pszVal);
				break;
			case PROTOTYPE_SELFTYPING_ON:
				TlenSend(this, "<m tp='t' to='%s'/>", dbv.pszVal);
				break;
			}
		}
		db_free(&dbv);
	}
	return 0;
}

INT_PTR TlenProtocol::GetMyAvatar(WPARAM wParam, LPARAM lParam)
{
	wchar_t* buf = (wchar_t*)wParam;
	int  size = (int)lParam;
	if (buf == nullptr || size <= 0)
		return -1;

	TlenGetAvatarFileName(this, nullptr, buf, size);
	return 0;
}

static INT_PTR CALLBACK TlenChangeAvatarDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM)
{
	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		Window_SetIcon_IcoLib(hwndDlg, GetIconHandle(IDI_TLEN));
		CheckDlgButton(hwndDlg, IDC_PUBLICAVATAR, BST_CHECKED);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
		{
			int result = LOWORD(wParam);
			if (IsDlgButtonChecked(hwndDlg, IDC_PUBLICAVATAR)) {
				result |= 0x10000;
			}
			EndDialog(hwndDlg, result);
		}
		return TRUE;
		}
		break;
	}
	return 0;
}

INT_PTR TlenProtocol::SetMyAvatar(WPARAM, LPARAM lParam)
{
	if (!isOnline) {
		PUShowMessageT(TranslateT("You need to be connected to Tlen account to set avatar."), SM_WARNING);
		return 1;
	}
	wchar_t* szFileName = (wchar_t*)lParam;
	wchar_t tFileName[MAX_PATH];
	if (szFileName != nullptr) {
		int result = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_USER_CHANGEAVATAR), nullptr, TlenChangeAvatarDlgProc, (LPARAM)NULL);
		TlenGetAvatarFileName(this, nullptr, tFileName, MAX_PATH);
		if (CopyFile(szFileName, tFileName, FALSE) == FALSE)
			return 1;

		char* tFileNameA = mir_u2a(tFileName); //TODO - drop io.h
		int fileIn = open(tFileNameA, O_RDWR | O_BINARY, S_IREAD | S_IWRITE);
		if (fileIn != -1) {
			long  dwPngSize = filelength(fileIn);
			BYTE* pResult = (BYTE *)mir_alloc(dwPngSize);
			if (pResult != nullptr) {
				read(fileIn, pResult, dwPngSize);
				close(fileIn);
				TlenUploadAvatar(this, pResult, dwPngSize, (result & 0x10000) != 0);
				mir_free(pResult);
			}
		}
		else debugLogA("SetMyAvatar open error");
		mir_free(tFileName);
		mir_free(tFileNameA);
	}
	else TlenRemoveAvatar(this);

	return 0;
}

INT_PTR TlenProtocol::GetAvatarCaps(WPARAM wParam, LPARAM lParam)
{
	switch (wParam) {
	case AF_MAXSIZE:
	{
		POINT* size = (POINT*)lParam;
		if (size)
			size->x = size->y = 64;
	}
	return 0;
	case AF_PROPORTION:
		return PIP_SQUARE;
	case AF_FORMATSUPPORTED:
		return (lParam == PA_FORMAT_PNG) ? 1 : 0;
	case AF_ENABLED:
		return tlenOptions.enableAvatars;
	case AF_DONTNEEDDELAYS:
		return 1;
	case AF_MAXFILESIZE:
		return 10 * 1024;
	case AF_FETCHIFCONTACTOFFLINE:
		return 1;
	default:
		return 0;
	}
}

int TlenProtocol::OnEvent(PROTOEVENTTYPE iEventType, WPARAM wParam, LPARAM lParam)
{
	//TlenProtocol *proto = (TlenProtocol *)ptr;
	switch (iEventType) {
	case EV_PROTO_ONLOAD:    return OnModulesLoaded(0, 0);
	case EV_PROTO_ONOPTIONS: return OptionsInit(wParam, lParam);
	case EV_PROTO_ONEXIT:    return PreShutdown(0, 0);

	case EV_PROTO_ONRENAME:
		Menu_ModifyItem(hMenuRoot, m_tszUserName);
	}
	return 1;
}

extern INT_PTR CALLBACK TlenAccMgrUIDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

INT_PTR TlenProtocol::AccMgrUI(WPARAM, LPARAM lParam)
{
	return (INT_PTR)CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_ACCMGRUI), (HWND)lParam, TlenAccMgrUIDlgProc, (LPARAM)this);
}

void TlenInitServicesVTbl(TlenProtocol *proto)
{
	proto->CreateProtoService(PS_GETNAME, &TlenProtocol::GetName);
	proto->CreateProtoService(PS_GETAVATARINFO, &TlenProtocol::GetAvatarInfo);
	proto->CreateProtoService(PS_SEND_NUDGE, &TlenProtocol::SendAlert);
	proto->CreateProtoService(PS_GETAVATARCAPS, &TlenProtocol::GetAvatarCaps);
	proto->CreateProtoService(PS_SETMYAVATAR, &TlenProtocol::SetMyAvatar);
	proto->CreateProtoService(PS_GETMYAVATAR, &TlenProtocol::GetMyAvatar);
	proto->CreateProtoService(PS_GETSTATUS, &TlenProtocol::GetStatus);
	proto->CreateProtoService(PS_CREATEACCMGRUI, &TlenProtocol::AccMgrUI);
}

TlenProtocol::TlenProtocol(const char *aProtoName, const wchar_t *aUserName) :
	PROTO<TlenProtocol>(aProtoName, aUserName)
{
	TlenInitServicesVTbl(this);

	hTlenNudge = CreateProtoEvent("/Nudge");

	HookProtoEvent(ME_OPT_INITIALISE, &TlenProtocol::OptionsInit);
	HookProtoEvent(ME_DB_CONTACT_SETTINGCHANGED, &TlenProtocol::TlenDbSettingChanged);
	HookProtoEvent(ME_DB_CONTACT_DELETED, &TlenProtocol::TlenContactDeleted);
	HookProtoEvent(ME_CLIST_PREBUILDCONTACTMENU, &TlenProtocol::PrebuildContactMenu);

	DBVARIANT dbv;
	if (!db_get(NULL, m_szModuleName, "LoginServer", &dbv))
		db_free(&dbv);
	else
		db_set_s(NULL, m_szModuleName, "LoginServer", "tlen.pl");

	if (!db_get(NULL, m_szModuleName, "ManualHost", &dbv))
		db_free(&dbv);
	else
		db_set_s(NULL, m_szModuleName, "ManualHost", "s1.tlen.pl");

	TlenLoadOptions(this);

	TlenWsInit(this);
	TlenSerialInit(this);
	TlenIqInit(this);
	TlenListInit(this);

	initMenuItems();
}

TlenProtocol::~TlenProtocol()
{
	TlenVoiceCancelAll(this);
	TlenFileCancelAll(this);
	if (hTlenNudge)
		DestroyHookableEvent(hTlenNudge);
	TlenListUninit(this);
	TlenIqUninit(this);
	TlenWsUninit(this);

	mir_free(modeMsgs.szOnline);
	mir_free(modeMsgs.szAway);
	mir_free(modeMsgs.szNa);
	mir_free(modeMsgs.szDnd);
	mir_free(modeMsgs.szFreechat);
	mir_free(modeMsgs.szInvisible);
}
