/*

Omegle plugin for Miranda Instant Messenger
_____________________________________________

Copyright © 2011-17 Robert Pösel, 2017-23 Miranda NG team

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "stdafx.h"

// Known server messages, only to inform lpgen
const char *server_messages[] = {
	LPGEN("Stranger is using Omegle's mobile Web site (omegle.com on a phone or tablet)"),
	LPGEN("You both speak the same language.")
};

void OmegleProto::SignOn(void*)
{
	SYSTEMTIME t;
	GetLocalTime(&t);
	debugLogA("[%d.%d.%d] Using Omegle Protocol %s", t.wDay, t.wMonth, t.wYear, __VERSION_STRING_DOTS);

	ScopedLock s(signon_lock_);

	int old_status = m_iStatus;
	m_iStatus = m_iDesiredStatus;
	ProtoBroadcastAck(0, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)old_status, m_iStatus);

	setDword("LogonTS", (uint32_t)time(0));
	ClearChat();
	OnJoinChat(0, false);

	if (getByte(OMEGLE_KEY_AUTO_CONNECT, 0))
		NewChat();
}

void OmegleProto::SignOff(void*)
{
	ScopedLock s(signon_lock_);

	int old_status = m_iStatus;
	m_iStatus = ID_STATUS_OFFLINE;

	Netlib_Shutdown(facy.hEventsConnection);

	OnLeaveChat(NULL, NULL);

	delSetting("LogonTS");

	ProtoBroadcastAck(0, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)old_status, m_iStatus);
}

void OmegleProto::StopChat(bool disconnect)
{
	if (facy.state_ == STATE_WAITING)
		UpdateChat(nullptr, TranslateT("Connecting canceled."), false);

	else if (facy.state_ == STATE_ACTIVE || facy.state_ == STATE_SPY) {
		bool spy = facy.state_ == STATE_SPY;

		if (disconnect) {
			facy.state_ = STATE_DISCONNECTING;
			UpdateChat(nullptr, TranslateT("Disconnecting..."), true);

			if (facy.stop())
				debugLogA("***** Disconnected from stranger %s", facy.chat_id_.c_str());
			else
				debugLogA("***** Error in disconnecting from stranger %s", facy.chat_id_.c_str());
		}

		if (spy) {
			DeleteChatContact(TranslateT("Stranger 1"));
			DeleteChatContact(TranslateT("Stranger 2"));
		}
		else DeleteChatContact(TranslateT("Stranger"));

		SetTopic(); // reset topic content

		Srmm_SetStatusText(GetChatHandle(), nullptr);
	}
	else return; // disconnecting or inactive

	facy.state_ = STATE_INACTIVE;
	facy.chat_id_.clear();
}

void OmegleProto::NewChat()
{
	if (facy.state_ == STATE_WAITING) {
		UpdateChat(nullptr, TranslateT("We are already waiting for new stranger..."), false);
		return;
	}
	
	if (facy.state_ == STATE_ACTIVE || facy.state_ == STATE_SPY) {
		UpdateChat(nullptr, TranslateT("Disconnecting..."), true);

		if (facy.stop())
			debugLogA("***** Disconnected from stranger %s", facy.chat_id_.c_str());
		else
			debugLogA("***** Error in disconnecting from stranger %s", facy.chat_id_.c_str());

		if (facy.state_ == STATE_SPY) {
			DeleteChatContact(TranslateT("Stranger 1"));
			DeleteChatContact(TranslateT("Stranger 2"));
		}
		else {
			DeleteChatContact(TranslateT("Stranger"));
		}

		SetTopic(); // reset topic content

		ClearChat();

		UpdateChat(nullptr, TranslateT("Connecting..."), true);

		facy.state_ = STATE_WAITING;

		if (facy.start()) {
			UpdateChat(nullptr, TranslateT("Waiting for Stranger..."), true);
			debugLogA("***** Waiting for stranger %s", facy.chat_id_.c_str());
		}
	}
	else if (facy.state_ == STATE_DISCONNECTING) {
		//UpdateChat(NULL, TranslateT("We are disconnecting now, wait..."), false);
		return;
	}
	else {
		ClearChat();
		UpdateChat(nullptr, TranslateT("Connecting..."), true);

		facy.state_ = STATE_WAITING;

		if (facy.start()) {
			UpdateChat(nullptr, TranslateT("Waiting for Stranger..."), true);
			debugLogA("***** Waiting for stranger %s", facy.chat_id_.c_str());

			ForkThread(&OmegleProto::EventsLoop, this);
		}
	}
}

void OmegleProto::EventsLoop(void *)
{
	ScopedLock s(events_loop_lock_);

	time_t tim = ::time(0);
	debugLogA(">>>>> Entering Omegle::EventsLoop[%d]", tim);

	while (facy.events()) {
		if (facy.state_ == STATE_INACTIVE || facy.state_ == STATE_DISCONNECTING || !isOnline())
			break;
		debugLogA("***** OmegleProto::EventsLoop[%d] refreshing...", tim);
	}

	ResetEvent(events_loop_lock_);

	Netlib_CloseHandle(facy.hConnection);
	facy.hConnection = nullptr;

	Netlib_CloseHandle(facy.hEventsConnection);
	facy.hEventsConnection = nullptr;

	debugLogA("<<<<< Exiting OmegleProto::EventsLoop[%d]", tim);
}
