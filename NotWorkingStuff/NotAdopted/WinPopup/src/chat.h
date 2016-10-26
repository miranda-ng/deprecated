/*

WinPopup Protocol plugin for Miranda IM.

Copyright (C) 2008-2009 Nikolay Raspopov

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

#ifdef CHAT_ENABLED

bool ChatRegister();
void ChatUnregister();
bool ChatNewSession(LPCTSTR szSession);
bool ChatAddGroup(LPCTSTR szSession, LPCTSTR szGroup);
bool ChatJoinMe(LPCTSTR szSession, LPCTSTR szGroup);
bool ChatJoinUser(LPCTSTR szSession, LPCTSTR szUser, LPCTSTR szGroup);
bool ChatInitDone(LPCTSTR szSession);
bool ChatOnline(LPCTSTR szSession);
bool ChatOffline(LPCTSTR szSession);
bool ChatMessage(LPCTSTR szSession, LPCTSTR szFrom, LPCTSTR szMessage);
bool ChatMessage(LPCTSTR szSession, LPCTSTR szMessage);

// ��������� �������������� ���� (������ ��� ������� ������)
CString GetChatSession(HANDLE hContact);

// �������� ��� ������� - ��� ���
bool IsChatRoom(HANDLE hContact);

#endif // CHAT_ENABLED
