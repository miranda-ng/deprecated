/*

WinPopup Protocol plugin for Miranda IM.

Copyright (C) 2004-2010 Nikolay Raspopov

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

// ������ ������������ �������� � ���� ������ Miranda IM
//
// ����������� ������ �������:
// Nick						String	- ���� �������� ������ (���.�����������)
// User						String	- ���� �������� ������������ (���.�����������)
// Workgroup				String	- ���� �������� ������ ��� ������� ������ ������ (���.�����������)
// Auto-answer				BYTE	- ���� ����-������ (��-���������: 0)
// Filter-dups				BYTE	- ���� ���������� ���������� (��-���������: 1)
// SendMethod   			BYTE    - ������ ������� ���������: 0 - mailslot, 1 - NetBIOS, 2 - Messenger  (��-���������: 0)
// RegisterNick 			BYTE    - ���� ����������� NetBIOS ����� Nick<01> � Nick<03> (��-���������: 1)
// RegisterUser 			BYTE    - ���� ����������� NetBIOS ����� User<03> (��-���������: 1)
// RegisterStatus 			BYTE	- ���� ����������� NetBIOS ����� MNS_STATUS<ab> (��-���������: 1)
// Check00ForOnline			BYTE	- ���� �������������� �������� NetBIOS ��� Nick<00> �� ������ ������, ��� ���� ��������� (�� ���������: 0) (������������� �� AlwaysCheck00ForOnline)
// AvatarFile				String	- ���� � ����� �������
//
// ������ ������� ��� ���������:
// Nick						String	- �������� ������
// IP						DWORD	- ����� ������
// RealIP					DWORD	- ����� ������ (��������� � IP)
// IPTimeL					DWORD	- ����� ���������� ���������� ����� � �����
// IPTimeH					DWORD
// LastSeenL				DWORD	- ����� ��������� �������� ��������
// LastSeenH				DWORD
// PingCounter				WORD	- ������� ��������� ������ ��� ������ (������ �� ������������)
// Status					WORD	- ������ ��������
// About					String	- �����������
// AlwaysOnline				BYTE	- ���� ����������� �������� � online-��������� ������
// Check00ForOnline 		BYTE	- ���� �������������� �������� NetBIOS ����� Nick<00> �� ������ ������ (�� ���������: 0)
// AvatarFile				String	- ���� � ����� �������
// Group					BYTE	- 1/0 - ��� ��������� �������
//
// �������������� ���������:
// ChatRoom					BYTE	- 1/0 - ��� ���
// ChatRoomID				String	- ������������� ����, ������ ��� ������� ������
//
// ������ CList (�������� ������ ��������) ��� ���������:
// MyHandle					String	- �������� �������� (���. �������������)
// NotOnList				BYTE	- ���� ���������� ��������
// Hidden					BYTE	- ���� �������� ��������
//
// ������ Icons (������ �������) ��� �������:
// WinPopup Protocol40071	String	- ���� � ������ �������
// WinPopup Protocol40072	String	- ���� � ������ �������
// WinPopup Protocol40073	String	- ���� � ������ �������
// WinPopup Protocol40074	String	- ���� � ������ �������
// WinPopup Protocol40075	String	- ���� � ������ �������
// WinPopup Protocol40076	String	- ���� � ������ �������
// WinPopup Protocol40077	String	- ���� � ������ �������
// WinPopup Protocol40078	String	- ���� � ������ �������
// WinPopup Protocol40079	String	- ���� � ������ �������
// WinPopup Protocol40080	String	- ���� � ������ �������

// �������� �������
#define modname				"WinPopup Protocol"
#define modname_t			_T( modname )
// ������� �������� �������
#define modtitle			"WinPopup"
#define modtitle_t			_T( modtitle )
// "�� ���� ���������� ������ ���������"
#define T_STOP_ERROR		TranslateT("Cannot stop Messenger service")
// "�� ���� ��������� ������ ���������"
#define T_START_ERROR		TranslateT("Cannot start Messenger service")
// "�� ���� �������� ������ ���������"
#define T_ENABLE_ERROR		TranslateT("Cannot enable Messenger service")
// "�� ���� ������� ������� ��������"
#define T_CREATE_ERROR		TranslateT("Cannot create receiving mailslot")
// "������"
#define T_ERROR				TranslateT("Error")
// �������� ������� Messenger
#define MESSENGER			_T("Messenger")
// �������� ��������� Messenger
#define MESSENGER_MAIL		_T("messngr")
// ����� ������� IP ������ (3 ����)
#define MAX_TRUSTED_IP_TIME	3*60*60
// ����������� ����� ���������� �������� (��� � 30 �)
#define MIN_PING_INTERVAL	30

typedef struct _ContactData
{
	HANDLE			cookie;		// ������������� ������� ���� ������� ��� ������
	HANDLE			hContact;	// ������� ��������
} ContactData;

typedef struct _SendMsgData
{
	HANDLE			cookie;		// ������������� ������� ���� ������� ��� ������
	HANDLE			hContact;	// ������� ��������
	CString			text;
} SendMsgData;

typedef struct _ThreadEvent
{
	HANDLE			evt;		// ������� �������
	HANDLE			cookie;		// ������������� ������� ���� ������� ��� ������
} ThreadEvent;

typedef CAtlMap < WPARAM, CString > CIntStrMap;
typedef CAtlMap < HANDLE, ThreadEvent > CThreadContactMap;

// ����� ��� ����������� ANSI ������ � OEM � ������������ ��������
class COemString
{
public:
	COemString(LPCTSTR ansi) :
		len( (size_t)lstrlen( ansi ) + 1 ),
		pos( 0 )
	{
		oem = (LPSTR)mir_alloc( len );
		CharToOemBuff( ansi, oem, (DWORD)len );
	}

	~COemString()
	{
		mir_free( oem );
	}

	inline operator LPCSTR() const
	{
		return oem + pos;
	}

	// ����� ������ (��� null)
	inline int GetLength() const
	{
		return (int)( len - pos - 1 );
	}

	// "���������" �� ������ ������
	inline void CutFromStart(int n)
	{
		if ( GetLength() > n )
			pos += n;
		else
			pos = len - 1;
	}

protected:
	size_t	len;
	size_t	pos;
	LPSTR	oem;
};

// ����� ��� ����������� OEM ������ � ANSI � ������������ ��������
class CAnsiString
{
public:
	CAnsiString(LPCSTR oem) :
		len( lstrlenA( oem ) + 1 ),
		pos( 0 )
	{
		ansi = (LPTSTR)mir_alloc( len * sizeof( TCHAR ) );
		OemToCharBuff( oem, ansi, (DWORD)len );
	}
	~CAnsiString()
	{
		mir_free( ansi );
	}
	inline operator LPCTSTR() const
	{
		return ansi + pos;
	}
	// ����� ������ (��� null)
	inline int GetLength() const
	{
		return len - pos - 1;
	}
	// "���������" �� ������ ������
	inline void CutFromStart(int n)
	{
		if ( len - pos - 1 > n )
			pos += n;
		else
			pos = len - 1;
	}
protected:
	int		len;
	int		pos;
	LPTSTR	ansi;
};

extern volatile WPARAM		pluginRequestedStatus;	// ��������� ������
extern volatile WPARAM		pluginCurrentStatus;	// ������� ������
extern CIntStrMap			pluginStatusMessage;	// ����� ��������� ���������
extern CString				pluginMachineName;		// ��� ����������
extern CString				pluginUserName;			// ��� ������������
extern CString				pluginDomainName;		// ��� ������ ��� ������
extern HMODULE				pluginModule;			// ������� �� ������
extern volatile bool		pluginInstalled;		// ���� ���������� ������ �������,
													// ������������ � false ��� �������������
													// ��� ��� ����������
extern volatile bool		pluginInitialized;		// ���� ���������� ������������� �������
extern HANDLE				pluginNetLibUser;		// ������� NetLib
extern HANDLE				pluginInternalState;	// ������� ������� (���������� ��������� - ������ ����������)
extern bool					pluginChatEnabled;		// ���������� Chat-������?
extern OSVERSIONINFO		pluginOS;				// ������ �������

// ��������� ���� ��������
CString GetNick(HANDLE hContact);
// ��������� ���� ��������
void SetNick(HANDLE hContact, LPCTSTR szNick);
// ����� � ����������� ��� ������� ��� NetLib
int LOG(const char *fmt,...);
// ��������� ���� ��� �������� �������
void GetAvatarCache(LPTSTR szPath);
// ��������� "�����������" �����
HANDLE GenerateCookie();
// Win32 API ������ ������� time() (��� ������������ �� ����������� CRT)
DWORD time();
// ����������� �������������� �������� �������
bool IsMyContact(HANDLE hContact);
// ��������� ������� �������
void SetPluginStatus(WPARAM status);
// ��������� ���� ��������� �������
bool InternalStartup();
// ���������� ���� ��������� �������
void InternalShutdown();
// ������� ������� � Online
void GotoOnline();
// ������� ������� � Offline
void GotoOffline();
// ������� ���� ������������ �������� ������� � Online
void GotoOnlineTread(LPVOID status);
// ������� ���� ������������ �������� ������� � Offline
void GotoOfflineTread(LPVOID status);
// ������� ���� ������������ ��������� ����-��������� ��������
void GetAwayMsgThread(LPVOID param);
// ������� ���� ������������ ����������� ���������� � ��������
void GetInfoThread(LPVOID param);
// ������������ ���������� ���������
void Autoanswer(HANDLE hContact);
// ���� ���������, �������� �� ���������, ������� � �������� � ���� Miranda IM
void ReceiveContactMessage(LPCTSTR msg_from, LPCTSTR msg_to, LPCTSTR msg_text, int msg_len);
// ���������� �������� �� ����� (� ����������� ��� IP-������)
HANDLE AddToListByName (const CString& sName, WPARAM flags, LPCTSTR notes, bool bInteractive, bool bGroup);
// ��������� "nameL" | ("nameH" << 32) ��������
DWORD GetElapsed(HANDLE hContact, const char* name);
// ��������� "LastSeen" �������� ������ �������� �������
void SetElapsed(HANDLE hContact, const char* name);
// ����� �������� �� "IP"
HANDLE GetContact(ip addr);
// ����� �������� �� "Nick"
HANDLE GetContact(LPCTSTR name);
// ��������� ������� �������� (simple == true - ��������� ������ online/offline)
void SetContactStatus(HANDLE hContact, int status, bool simple);
// ��������� ����-������ ��������
void SetContactAway(HANDLE hContact, LPCSTR away);
// ��������� ������� ��������
void SetContactAvatar(HANDLE hContact, LPCVOID pBuffer, DWORD nLength);
// ��������� IP-������ �������� (� ��������� � �����������, ���� ���������)
ip GetContactIP(HANDLE hContact);
// ��������� "IP" ��������
void SetContactIP(HANDLE hContact, ip addr);
// ��� ��������� �������?
bool IsGroup(HANDLE hContact);
// ��������� ���������� ��������
void SetGroup(HANDLE hContact, bool bGroup);
// ������������ ������ ����� ������-��������?
bool IsLegacyOnline(HANDLE hContact);
// ��������� ������� ������ ������-��������
void SetLegacyOnline(HANDLE hContact, bool bOnline);
// ������� ���������
bool SendContactMessage(HANDLE hContact, LPCTSTR msg, DWORD& err);
// ������� ���� ����������� ������� ���������
void SendMsgThread(LPVOID param);
// ������� ���� ������������ ������� �������
void GetAvatarInfoThread(LPVOID param);
// �� ���?
bool IsItMe(LPCTSTR name);
// ������������ ��������� ��������� �� ������
void Normalize(CString& msg);
// ��������� ������ ������� ����� / �������
void EnumWorkgroups(CAtlList< CString >& lst, LPNETRESOURCE hRoot = NULL);

// ������ ����������� ����� �������� ����� ��������� � ������ ���������
#define FOR_EACH_CONTACT(h) \
	HANDLE h = (HANDLE)CallService( MS_DB_CONTACT_FINDFIRST, 0, 0 ); \
	for ( ; h != NULL; \
	h = (HANDLE)CallService( MS_DB_CONTACT_FINDNEXT, (WPARAM)h, 0 ) ) \
		if ( IsMyContact( h ) )

// ������ ��� ������ �������� ������� (����������)
#define STATUS2TEXT(s) \
	((((s) == ID_STATUS_OFFLINE) ? "Offline" : \
	(((s) == ID_STATUS_ONLINE) ? "Online" : \
	(((s) == ID_STATUS_AWAY) ? "Away" : \
	(((s) == ID_STATUS_DND) ? "DND" : \
	(((s) == ID_STATUS_NA) ? "NA" : \
	(((s) == ID_STATUS_OCCUPIED) ? "Occupied" : \
	(((s) == ID_STATUS_FREECHAT) ? "Free to chat" : \
	(((s) == ID_STATUS_INVISIBLE) ? "Invisible" : \
	(((s) == ID_STATUS_ONTHEPHONE) ? "On the phone" : \
	(((s) == ID_STATUS_OUTTOLUNCH) ? "Out to lunch" : \
	(((s) == ID_STATUS_IDLE) ? "Idle" : \
	(((s) == (ID_STATUS_CONNECTING + 0)) ? "Connecting 1" : \
	(((s) == (ID_STATUS_CONNECTING + 1)) ? "Connecting 2" : \
	(((s) <  (ID_STATUS_CONNECTING + MAX_CONNECT_RETRIES)) ? "Connecting > 2" : \
	"Unknown")))))))))))))))
