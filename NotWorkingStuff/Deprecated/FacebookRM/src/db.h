/*

Facebook plugin for Miranda Instant Messenger
_____________________________________________

Copyright © 2009-11 Michal Zelinka, 2011-17 Robert Pösel, 2017-19 Miranda NG team

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

#pragma once

// Contact DB keys
#define FACEBOOK_KEY_LOGIN					"Email"
#define FACEBOOK_KEY_ID						"ID"
#define FACEBOOK_KEY_TID					"ThreadID"
#define FACEBOOK_KEY_FIRST_NAME				"FirstName"
#define FACEBOOK_KEY_SECOND_NAME			"SecondName"
#define FACEBOOK_KEY_LAST_NAME				"LastName"
#define FACEBOOK_KEY_NICK					"Nick"
#define FACEBOOK_KEY_USERNAME				"Username"
#define FACEBOOK_KEY_PASS					"Password"
#define FACEBOOK_KEY_DEVICE_ID				"DeviceID"
#define FACEBOOK_KEY_AVATAR					"Avatar"
#define FACEBOOK_KEY_DELETED				"DeletedTS"
#define FACEBOOK_KEY_CONTACT_TYPE			"ContactType"
#define FACEBOOK_KEY_MESSAGE_ID				"LastMessageId"
#define FACEBOOK_KEY_MESSAGE_READ			"LastMsgReadTime"
#define FACEBOOK_KEY_MESSAGE_READERS		"MessageReaders"

// Thread specific DB keys 
#define FACEBOOK_KEY_CHAT_CAN_REPLY			"CanReply"
#define FACEBOOK_KEY_CHAT_READ_ONLY			"ReadOnly"
#define FACEBOOK_KEY_CHAT_IS_ARCHIVED		"IsArchived"
#define FACEBOOK_KEY_CHAT_IS_SUBSCRIBED		"IsSubscribed"

// Contact and account DB keys
#define FACEBOOK_KEY_KEEP_UNREAD			"KeepUnread"	// (byte) 1 = don't mark messages as read on server (works globally or per contact)

// Account DB keys
#define FACEBOOK_KEY_DEF_GROUP				"DefaultGroup"
#define FACEBOOK_KEY_SET_MIRANDA_STATUS		"SetMirandaStatus"
#define FACEBOOK_KEY_SYSTRAY_NOTIFY			"UseSystrayNotify"
#define FACEBOOK_KEY_DISABLE_STATUS_NOTIFY	"DisableStatusNotify"
#define FACEBOOK_KEY_BIG_AVATARS			"UseBigAvatars"
#define FACEBOOK_KEY_DISCONNECT_CHAT		"DisconnectChatEnable"
#define FACEBOOK_KEY_MAP_STATUSES			"MapStatuses"
#define FACEBOOK_KEY_CUSTOM_SMILEYS			"CustomSmileys"
#define FACEBOOK_KEY_SERVER_TYPE			"ServerType"
#define FACEBOOK_KEY_PRIVACY_TYPE			"PrivacyType"
#define FACEBOOK_KEY_PLACE					"Place"
#define FACEBOOK_KEY_LAST_WALL				"LastWall"
#define FACEBOOK_KEY_LOAD_PAGES				"LoadPages"
#define FACEBOOK_KEY_FILTER_ADS				"FilterAds"
#define FACEBOOK_KEY_LOGON_TS				"LogonTS"
#define FACEBOOK_KEY_LAST_ACTION_TS			"LastActionTS"
#define FACEBOOK_KEY_MESSAGES_ON_OPEN		"MessagesOnOpen"
#define FACEBOOK_KEY_MESSAGES_ON_OPEN_COUNT	"MessagesOnOpenCount"
#define FACEBOOK_KEY_HIDE_CHATS				"HideChats"
#define FACEBOOK_KEY_ENABLE_CHATS			"EnableChat"
#define FACEBOOK_KEY_JOIN_EXISTING_CHATS	"JoinExistingChats"
#define FACEBOOK_KEY_NOTIFICATIONS_CHATROOM	"NotificationsChatroom"
#define FACEBOOK_KEY_NAME_AS_NICK			"NameAsNick"
#define FACEBOOK_KEY_LOAD_ALL_CONTACTS		"LoadAllContacts"
#define FACEBOOK_KEY_PAGES_ALWAYS_ONLINE	"PagesAlwaysOnline"
#define FACEBOOK_KEY_TYPING_WHEN_INVISIBLE	"TypingWhenInvisible"

// Account DB keys - notifications
#define FACEBOOK_KEY_EVENT_NOTIFICATIONS_ENABLE	"EventNotificationsEnable"
#define FACEBOOK_KEY_EVENT_FEEDS_ENABLE			"EventFeedsEnable"
#define FACEBOOK_KEY_EVENT_FRIENDSHIP_ENABLE	"EventFriendshipEnable"
#define FACEBOOK_KEY_EVENT_TICKER_ENABLE		"EventTickerEnable"
#define FACEBOOK_KEY_EVENT_ON_THIS_DAY_ENABLE	"EventMemoriesEnable"
#define FACEBOOK_KEY_FEED_TYPE					"EventFeedsType"

// Hidden account DB keys (can't be changed through GUI)
#define FACEBOOK_KEY_POLL_RATE				"PollRate"					// [HIDDEN] - (byte)
#define FACEBOOK_KEY_TIMEOUTS_LIMIT			"TimeoutsLimit"				// [HIDDEN] - (byte)
#define	FACEBOOK_KEY_LOCALE					"Locale"					// [HIDDEN] - (string) en_US, cs_CZ, etc. (requires restart to apply)
#define FACEBOOK_KEY_NASEEMS_SPAM_MODE		"NaseemsSpamMode"			// [HIDDEN] - (byte) 1 = don't load messages sent from other instances (e.g., browser) - known as "Naseem's spam mode"
#define FACEBOOK_KEY_OPEN_URL_BROWSER		"OpenUrlBrowser"			// [HIDDEN] - (unicode) = absolute path to browser to open url links with
#define FACEBOOK_KEY_SEND_MESSAGE_TRIES		"SendMessageTries"			// [HIDDEN] - (byte) = number of tries to send message, default=1, min=1, max=5
#define FACEBOOK_KEY_PAGE_PREFIX			"PagePrefix"				// [HIDDEN] - (unicode) = prefix for name of "page" contacts (requires restart to apply), default is emoji :page_facing_up: (written as unicode char)

// Temporary key for login
#define FACEBOOK_KEY_TRIED_DELETING_DEVICE_ID  "_TriedDeletingDeviceID"