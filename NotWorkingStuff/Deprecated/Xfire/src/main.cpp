#include "stdafx.h"

/*
 *  Plugin of miranda IM(ICQ) for Communicating with users of the XFire Network.
 *
 *  Copyright (C) 2010 by
 *          dufte <dufte@justmail.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *  Based on J. Lawler              - BaseProtocol
 *			 Herbert Poul/Beat Wolf - xfirelib
 *
 *  Miranda ICQ: the free icq client for MS Windows
 *  Copyright (C) 2000-2008  Richard Hughes, Roland Rabien & Tristan Van de Vreede
 *
 */

//xfire stuff
#include "client.h"
#include "xfirepacket.h"
#include "loginfailedpacket.h"
#include "otherloginpacket.h"
#include "loginsuccesspacket.h"
#include "messagepacket.h"
#include "sendstatusmessagepacket.h"
#include "sendmessagepacket.h"
#include "invitebuddypacket.h"
#include "sendacceptinvitationpacket.h"
#include "senddenyinvitationpacket.h"
#include "sendremovebuddypacket.h"
#include "sendnickchangepacket.h"
#include "sendgamestatuspacket.h"
#include "sendgamestatus2packet.h"
#include "dummyxfiregameresolver.h"
#include "sendgameserverpacket.h"
#include "recvstatusmessagepacket.h"
#include "recvoldversionpacket.h"
#include "packetlistener.h"
#include "inviterequestpacket.h"
#include "buddylistgames2packet.h"
#include "dummyxfiregameresolver.h"
#include "sendtypingpacket.h"
#include "xfireclanpacket.h"
#include "recvremovebuddypacket.h"
#include "gameinfopacket.h"
#include "claninvitationpacket.h"
#include "xfireprefpacket.h"
#include "searchbuddy.h"
#include "xfirefoundbuddys.h"
#include "getbuddyinfo.h"
#include "buddyinfo.h"
#include "variables.h"
#include "passworddialog.h"
#include "setnickname.h"
#include "all_statusmsg.h"
#include "processbuddyinfo.h"
#include "recvprefspacket.h"
#include "sendsidpacket.h"
#include "friendsoffriendlist.h"
#include "recvbuddychangednick.h"

//miranda stuff
#include "baseProtocol.h"
#include "Xfire_gamelist.h"
#include "Xfire_proxy.h"
#include "Xfire_avatar_loader.h"
#include "Xfire_voicechat.h"

#include "variables.h"
#include "version.h"

#include <stdexcept>
#include <sstream>

Xfire_gamelist xgamelist;
Xfire_voicechat voicechat;

HANDLE hLogEvent;
int bpStatus = ID_STATUS_OFFLINE;
int previousMode;
int OptInit(WPARAM wParam, LPARAM lParam);
int OnDetailsInit(WPARAM wParam, LPARAM lParam);
HANDLE hFillListEvent = 0;
CONTACT user;
HINSTANCE hinstance = NULL;
int hLangpack;
HANDLE hExtraIcon1, hExtraIcon2;
HANDLE heventXStatusIconChanged;
HGENMENU copyipport, gotoclansite, vipport, joingame, startthisgame, removefriend, blockfriend;
int foundgames = 0;
Gdiplus::GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR           gdiplusToken;

//xfire preferences, wichtige variablen
xfire_prefitem xfireconfig[XFIRE_RECVPREFSPACKET_MAXCONFIGS];
extern xfireconfigitem xfireconfigitems[XFIRE_RECVPREFSPACKET_SUPPORTEDONFIGS];

mir_cs modeMsgsMutex;
mir_cs avatarMutex;
mir_cs connectingMutex;

DWORD pid = NULL; //processid des gefunden spiels
DWORD ts2pid = NULL; // processid vom teamspeak/ventrilo

HANDLE	 XFireAvatarFolder = NULL;
HANDLE	 XFireWorkingFolder = NULL;
HANDLE	 XFireIconFolder = NULL;
HANDLE	 hookgamestart = NULL;
char statusmessage[2][1024];
BOOL sendonrecieve = FALSE;
HANDLE hNetlib = NULL;
extern LPtsrGetServerInfo tsrGetServerInfo;

//eventhandles
HANDLE hGameDetection = CreateEvent(NULL, FALSE, FALSE, NULL);
HANDLE hConnectionClose = CreateEvent(NULL, TRUE, FALSE, NULL);

PLUGININFOEX pluginInfo = {
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__AUTHOREMAIL,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// {9B8E1735-970D-4ce0-930C-A561956BDCA2}
	{ 0x9b8e1735, 0x970d, 0x4ce0, { 0x93, 0xc, 0xa5, 0x61, 0x95, 0x6b, 0xdc, 0xa2 } }
};

static IconItem icon = { LPGEN("Protocol icon"), "XFIRE_main", IDI_TM };

INT_PTR RecvMessage(WPARAM wParam, LPARAM lParam);
INT_PTR SendMessage(WPARAM wParam, LPARAM lParam);

int FillList(WPARAM wParam, LPARAM lParam);
MCONTACT CList_AddContact(XFireContact xfc, bool InList, bool SetOnline, int clan);
MCONTACT CList_FindContact(int uid);
void CList_MakeAllOffline();
static INT_PTR UserIsTyping(WPARAM wParam, LPARAM lParam);
HANDLE LoadGameIcon(char* g, int id, HICON* ico, BOOL onyico = FALSE, char * gamename = NULL, int*uu = NULL);
BOOL GetAvatar(char* username, XFireAvatar* av);
//void SetAvatar(MCONTACT hContact, char* username);
static void SetAvatar(LPVOID lparam);
static INT_PTR GetIPPort(WPARAM /*wParam*/, LPARAM lParam);
static INT_PTR GetVIPPort(WPARAM /*wParam*/, LPARAM lParam);
int RebuildContactMenu(WPARAM wParam, LPARAM lParam);
int doneQuery(WPARAM wParam, LPARAM lParam);

static INT_PTR GotoProfile(WPARAM wParam, LPARAM lParam);
static INT_PTR GotoProfileAct(WPARAM wParam, LPARAM lParam);
static INT_PTR GotoXFireClanSite(WPARAM wParam, LPARAM lParam);
static INT_PTR ReScanMyGames(WPARAM wParam, LPARAM lParam);
static INT_PTR SetNickDlg(WPARAM wParam, LPARAM lParam);
static INT_PTR CustomGameSetup(WPARAM wParam, LPARAM lParam);

#ifndef NO_PTHREAD
void *gamedetectiont(void *ptr);
void *inigamedetectiont(void *ptr);
pthread_t gamedetection;
#else
void inigamedetectiont(LPVOID lParam);
void gamedetectiont(LPVOID lparam);
#endif

INT_PTR AddtoList(WPARAM wParam, LPARAM lParam);
INT_PTR BasicSearch(WPARAM wParam, LPARAM lParam);
INT_PTR GetAvatarInfo(WPARAM wParam, LPARAM lParam); //GAIR_NOAVATAR
INT_PTR SearchAddtoList(WPARAM wParam, LPARAM lParam);
INT_PTR SendPrefs(WPARAM wparam, LPARAM lparam);
INT_PTR SetAwayMsg(WPARAM wParam, LPARAM lParam);
//INT_PTR GetAwayMsg(WPARAM wParam, LPARAM lParam);
INT_PTR GetXStatusIcon(WPARAM wParam, LPARAM lParam);

static INT_PTR GotoProfile2(WPARAM wParam, LPARAM lParam);
MCONTACT handlingBuddys(BuddyListEntry *entry, int clan = 0, char* group = NULL, BOOL dontscan = FALSE);
int StatusIcon(WPARAM wParam, LPARAM lParam);

void CreateGroup(char*grpn, char*field); //void CreateGroup(char*grp);
int ContactDeleted(WPARAM wParam, LPARAM /*lParam*/);
INT_PTR JoinGame(WPARAM wParam, LPARAM lParam);
extern void Scan4Games(LPVOID lparam);
INT_PTR RemoveFriend(WPARAM wParam, LPARAM lParam);
INT_PTR BlockFriend(WPARAM wParam, LPARAM lParam);
INT_PTR StartThisGame(WPARAM wParam, LPARAM lParam);
void SetAvatar2(LPVOID lparam);
int ExtraListRebuild(WPARAM wparam, LPARAM lparam);

//XFire Stuff
using namespace xfirelib;

class XFireClient : public PacketListener
{

public:
	Client *m_client;
	Xfire_avatar_loader *m_avatarloader;

	XFireClient(string username, string password, char protover, int useproxy = 0, string proxyip = "", int proxyport = 0);
	~XFireClient();
	void run();

	void Status(string s);

	void receivedPacket(XFirePacket *packet);

	void getBuddyList();
	void sendmsg(char*usr, char*msg);
	void setNick(char*nnick);
	void handlingBuddy(MCONTACT handle);
	void CheckAvatar(BuddyListEntry* entry);

private:
	vector<string> explodeString(string s, string e);
	string joinString(vector<string> s, int startindex, int endindex = -1, string delimiter = " ");
	void BuddyList();

	string *m_lastInviteRequest;

	string m_username;
	string m_password;
	string m_proxyip;
	int m_useproxy;
	int m_proxyport;
	BOOL m_connected;
	unsigned int m_myuid;
};

XFireClient* myClient = NULL;

void XFireClient::CheckAvatar(BuddyListEntry* entry)
{
	//kein entry, zur�ck
	if (!entry)
		return;

	//keine avatars?
	if (db_get_b(NULL, protocolname, "noavatars", -1) == 0) {
		//avatar gelocked?
		if (db_get_b(entry->m_hcontact, "ContactPhoto", "Locked", -1) != 1) {
			//avatar lade auftrag �bergeben
			m_avatarloader->loadAvatar(entry->m_hcontact, (char*)entry->m_username.c_str(), entry->m_userid);
		}
	}
}

void XFireClient::handlingBuddy(MCONTACT handle)
{
	vector<BuddyListEntry*> *entries = m_client->getBuddyList()->getEntries();
	for (uint i = 0; i < entries->size(); i++) {
		BuddyListEntry *entry = entries->at(i);
		if (entry->m_hcontact == handle) {
			handlingBuddys(entry, 0, NULL);
			break;
		}
	}
	//mir_forkthread(
}

void XFireClient::setNick(char*nnick)
{
	/*if (mir_strlen(nnick)==0)
		return;*/
	SendNickChangePacket nick;
	nick.nick = nnick;
	m_client->send(&nick);
}


void XFireClient::sendmsg(char*usr, char*cmsg)
{
	SendMessagePacket msg;
	//	if (mir_strlen(cmsg)>255)
	//		*(cmsg+255)=0;
	msg.init(m_client, usr, cmsg);
	m_client->send(&msg);
}


XFireClient::XFireClient(string username, string password, char protover, int useproxy, string proxyip, int proxyport) :
	m_username(username), m_password(password)
{
	m_client = new Client();
	m_client->setGameResolver(new DummyXFireGameResolver());
	m_client->m_protocolVersion = protover;
	m_useproxy = useproxy;
	m_proxyip = proxyip;
	m_proxyport = proxyport;

	m_avatarloader = new Xfire_avatar_loader(m_client);

	m_lastInviteRequest = NULL;
	m_connected = FALSE;
}

XFireClient::~XFireClient()
{
	if (m_client != NULL) {
		m_client->disconnect();
		delete m_client;
	}
	if (m_avatarloader) {
		delete m_avatarloader;
		m_avatarloader = NULL;
	}
	if (m_lastInviteRequest != NULL)
		delete m_lastInviteRequest;
}

void XFireClient::run()
{
	m_client->connect(m_username, m_password, m_useproxy, m_proxyip, m_proxyport);
	m_client->addPacketListener(this);
}

void XFireClient::Status(string s)
{
	//da bei xfire statusmsg nur 100bytes l�nge unterst�tzt werden, wird gecutted
	if (!m_client->m_gotBudduyList)
		return;

	s = s.substr(0, 100);

	SendStatusMessagePacket *packet = new SendStatusMessagePacket();

	packet->awaymsg = ptrA(mir_utf8encode(s.c_str()));
	m_client->send(packet);
	delete packet;
}

void XFireClient::receivedPacket(XFirePacket *packet)
{
	XFirePacketContent *content = packet->getContent();

	switch (content->getPacketId()) {
		/*case XFIRE_RECVBUDDYCHANGEDNICK:
		{
		RecvBuddyChangedNick *changednick = (RecvBuddyChangedNick*)content;
		if (changednick) {
		handlingBuddys((BuddyListEntry*)changednick->entry,0,NULL);
		}
		break;
		}*/
		//Konfigpacket empfangen
	case XFIRE_RECVPREFSPACKET:
		{
			//Konfigarray leeren
			memset(&xfireconfig, 0, sizeof(xfire_prefitem)*XFIRE_RECVPREFSPACKET_MAXCONFIGS);
			RecvPrefsPacket *config = (RecvPrefsPacket*)content;
			//konfigs in array speichern
			if (config != NULL) {
				//ins preferenes array sichern
				for (int i = 0; i < XFIRE_RECVPREFSPACKET_MAXCONFIGS; i++) {
					xfireconfig[i] = config->config[i];
				}
				//datenbank eintr�ge durchf�hren
				for (int i = 0; i < XFIRE_RECVPREFSPACKET_SUPPORTEDONFIGS; i++) {
					char temp = 1;
					if (xfireconfig[xfireconfigitems[i].xfireconfigid].wasset == 1) {
						temp = 0;
					}
					db_set_b(NULL, protocolname, xfireconfigitems[i].dbentry, temp);
				}
			}
			break;
		}
	case XFIRE_FOUNDBUDDYS_ID:
		{
			PROTOSEARCHRESULT psr;
			memset(&psr, 0, sizeof(psr));
			psr.cbSize = sizeof(psr);
			psr.flags = PSR_TCHAR;

			XFireFoundBuddys *fb = (XFireFoundBuddys*)content;
			for (uint i = 0; i < fb->usernames->size(); i++) {
				if ((char*)fb->usernames->at(i).c_str() != NULL)
					psr.nick.t = _A2T((char*)fb->usernames->at(i).c_str());
				if ((char*)fb->fname->at(i).c_str() != NULL)
					psr.firstName.t = _A2T((char*)fb->fname->at(i).c_str());
				if ((char*)fb->lname->at(i).c_str() != NULL)
					psr.lastName.t = _A2T((char*)fb->lname->at(i).c_str());
				ProtoBroadcastAck(protocolname, NULL, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE)1, (LPARAM)& psr);
			}

			ProtoBroadcastAck(protocolname, NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)1, 0);
			break;
		}
	case XFIRE_BUDDYINFO:
		{
			BuddyInfoPacket *buddyinfo = (BuddyInfoPacket*)content;
			BuddyListEntry *entry = m_client->getBuddyList()->getBuddyById(buddyinfo->userid);

			//wenn die uid die gleiche wie die eigene ist, dann avatar auch selbst zuweisen
			if (buddyinfo->userid == m_myuid) {
				ProcessBuddyInfo(buddyinfo, NULL, "myxfireavatar");
			}

			if (entry)
				ProcessBuddyInfo(buddyinfo, entry->m_hcontact, (char*)entry->m_username.c_str());

			break;
		}
	case XFIRE_CLANINVITATION_ID:
		{
			ClanInvitationPacket *claninv = (ClanInvitationPacket*)content;
			for (int i = 0; i < claninv->numberOfInv; i++) {
				char msg[XFIRE_MAX_STATIC_STRING_LEN];
				mir_snprintf(msg, Translate("%s (Nickname: %s) has invited you to join the %s clan. Message: %s%sPlease go to the XFire clan site to accept the Invitation."), claninv->invitefromusername[i].c_str(),
					claninv->invitefrom[i].c_str(),
					claninv->clanname[i].c_str(),
					claninv->invitemsg[i].c_str(), "\n");
				MSGBOX(msg);
			}
			break;
		}
	case XFIRE_GAMEINFO_ID:
		{
			GameInfoPacket *gameinfo = (GameInfoPacket*)content;
			for (uint i = 0; i < gameinfo->sids->size(); i++) {
				BuddyListEntry *entry = m_client->getBuddyList()->getBuddyBySid(gameinfo->sids->at(i));
				if (entry) {
					entry->m_gameinfo = gameinfo->gameinfo->at(i);
					handlingBuddys(entry, 0, NULL);
				}
			}
			break;
		}
	case XFIRE_RECVREMOVEBUDDYPACKET:
		{
			RecvRemoveBuddyPacket *remove = (RecvRemoveBuddyPacket*)content;
			CallService(MS_DB_CONTACT_DELETE, (WPARAM)remove->handle, 1);
			break;
		}
	case XFIRE_BUDDYS_NAMES_ID:
		{
			//status nachricht nach der buddylist senden
			m_client->m_gotBudduyList = TRUE;
			if (sendonrecieve) {
				if (myClient != NULL) {
					if (myClient->m_client->m_connected) {
						//
						if (bpStatus == ID_STATUS_AWAY)
							myClient->Status(statusmessage[1]);
						else
							myClient->Status(statusmessage[0]);
					}
				}
				sendonrecieve = FALSE;
			}
			sendonrecieve = FALSE;

			/*			  	GetBuddyInfo buddyinfo;

							vector<BuddyListEntry*> *entries = client->getBuddyList()->getEntries();
							for(uint i = 0 ; i < entries->size() ; i ++) {
							BuddyListEntry *entry = entries->at(i);
							handlingBuddys(entry,0,NULL);
							}*/
			break;
		}
		/* case XFIRE_CLAN_BUDDYS_NAMES_ID:
		 {
		 vector<BuddyListEntry*> *entries = client->getBuddyList()->getEntriesClan();

		 char temp[255];
		 char * dummy;
		 ClanBuddyListNamesPacket *clan = (ClanBuddyListNamesPacket*)content;
		 mir_snprintf(temp, "Clan_%d", clan->clanid);

		 DBVARIANT dbv;
		 if (!db_get(NULL,protocolname,temp,&dbv))
		 {
		 dummy=dbv.pszVal;
		 }
		 else
		 dummy=NULL;

		 for(uint i = 0 ; i < entries->size() ; i ++) {
		 BuddyListEntry *entry = entries->at(i);
		 if (entry->clanid==clan->clanid) {
		 handlingBuddys(entry,clan->clanid,dummy);
		 }
		 }
		 break;
		 }*/
	case XFIRE_FRIENDS_BUDDYS_NAMES_ID:
		{
			for (uint i = 0; i < ((FriendsBuddyListNamesPacket*)content)->userids->size(); i++) {
				BuddyListEntry *entry = m_client->getBuddyList()->getBuddyById(((FriendsBuddyListNamesPacket*)content)->userids->at(i));
				if (entry) {
					char fofname[128] = LPGEN("Friends of Friends Playing");
					DBVARIANT dbv;
					//gruppennamen �berladen
					if (!db_get(NULL, protocolname, "overload_fofgroupname", &dbv)) {
						strcpy_s(fofname, 128, dbv.pszVal);
						db_free(&dbv);
					}
					CreateGroup(Translate(fofname), "fofgroup");
					MCONTACT hc = handlingBuddys(entry, -1, Translate(fofname));
					if (hc) {
						CheckAvatar(entry);
						db_set_b(hc, protocolname, "friendoffriend", 1);
					}
				}
			}
			break;
		}
		/*case XFIRE_BUDDYS_ONLINE_ID:
		{
		for(uint i = 0 ; i < ((BuddyListOnlinePacket*)content)->userids->size() ; i++) {
		BuddyListEntry *entry = client->getBuddyList()->getBuddyById( ((BuddyListOnlinePacket*)content)->userids->at(i) );
		if (entry){
		handlingBuddys(entry,0,NULL);
		}
		}
		break;
		}*/
		/*case XFIRE_RECV_STATUSMESSAGE_PACKET_ID:
		{
		for(uint i=0;i<((RecvStatusMessagePacket*)content)->sids->size();i++)
		{
		BuddyListEntry *entry = m_client->getBuddyList()->getBuddyBySid( ((RecvStatusMessagePacket*)content)->sids->at(i) );
		if (entry) //crashbug entfernt
		setBuddyStatusMsg(entry); //auf eine funktion reduziert, verringert cpuauslastung und beseitigt das
		//das problem der fehlenden statusmsg
		//handlingBuddys(entry,0,NULL);
		}
		break;
		}*/
	case XFIRE_BUDDYS_GAMES_ID:
		{
			vector<char *> *sids = NULL; //dieses array dient zu zwischensicherung von unbekannten sids
			for (uint i = 0; i < ((BuddyListGamesPacket*)content)->sids->size(); i++) {
				BuddyListEntry *entry = m_client->getBuddyList()->getBuddyBySid(((BuddyListGamesPacket*)content)->sids->at(i));
				if (entry != NULL) {
					//wir haben einen unbekannten user
					if (entry->m_username.length() == 0) {
						//sid array ist noch nicht init
						if (sids == NULL) {
							sids = new vector < char * >;
						}
						//kopie der sid anlegen
						char *sid = new char[16];
						memcpy(sid, ((BuddyListGamesPacket*)content)->sids->at(i), 16);
						//ab ins array damit
						sids->push_back(sid);
					}
					else {
						if (entry->m_game == 0 && entry->m_hcontact != 0 && db_get_b(entry->m_hcontact, protocolname, "friendoffriend", 0) == 1)
							db_set_w(entry->m_hcontact, protocolname, "Status", ID_STATUS_OFFLINE);
						else
							handlingBuddys(entry, 0, NULL);
					}
				}
			}
			//sid anfragen nur senden, wenn das sids array init wurde
			if (sids) {
				SendSidPacket sp;
				sp.sids = sids;
				m_client->send(&sp);
				delete sids;
			}
			break;
		}
	case XFIRE_BUDDYS_GAMES2_ID:
		{
			for (uint i = 0; i < ((BuddyListGames2Packet*)content)->sids->size(); i++) {
				BuddyListEntry *entry = m_client->getBuddyList()->getBuddyBySid(((BuddyListGames2Packet*)content)->sids->at(i));
				if (entry != NULL) handlingBuddys(entry, 0, NULL);
			}
			break;
		}
	case XFIRE_PACKET_INVITE_REQUEST_PACKET: //friend request
		{
			InviteRequestPacket *invite = (InviteRequestPacket*)content;

			//nur nich blockierte buddy's durchlassen
			if (!db_get_b(NULL, "XFireBlock", (char*)invite->name.c_str(), 0)) {
				XFireContact xfire_newc;
				xfire_newc.username = (char*)invite->name.c_str();
				xfire_newc.nick = (char*)invite->nick.c_str();
				xfire_newc.id = 0;

				MCONTACT handle = CList_AddContact(xfire_newc, TRUE, TRUE, 0);
				if (handle) {  // invite nachricht mitsenden
					string str = (char*)invite->msg.c_str();

					PROTORECVEVENT pre;
					pre.flags = 0;
					pre.timestamp = time(NULL);
					pre.szMessage = (char*)mir_utf8decode((char*)str.c_str(), NULL);
					//invite nachricht konnte nicht zugewiesen werden?!?!?!
					if (!pre.szMessage)
						pre.szMessage = (char*)str.c_str();
					pre.lParam = 0;
					ProtoChainRecvMsg(handle, &pre);
				}
			}
			else {
				SendDenyInvitationPacket deny;
				deny.name = invite->name;
				m_client->send(&deny);
			}
			break;
		}
	case XFIRE_CLAN_PACKET:
		{
			char temp[100];
			XFireClanPacket *clan = (XFireClanPacket*)content;

			for (int i = 0; i < clan->count; i++) {
				mir_snprintf(temp, "Clan_%d", clan->clanid[i]);
				db_set_s(NULL, protocolname, temp, (char*)clan->name[i].c_str());

				mir_snprintf(temp, "ClanUrl_%d", clan->clanid[i]);
				db_set_s(NULL, protocolname, temp, (char*)clan->url[i].c_str());

				if (!db_get_b(NULL, protocolname, "noclangroups", 0)) {
					CreateGroup((char*)clan->name[i].c_str(), "mainclangroup");
				}
			}
			break;
		}
	case XFIRE_LOGIN_FAILED_ID:
		MSGBOXE(Translate("Login failed."));
		SetStatus(ID_STATUS_OFFLINE, NULL);
		break;
	case XFIRE_LOGIN_SUCCESS_ID: //login war erfolgreich
		{
			LoginSuccessPacket *login = (LoginSuccessPacket*)content;
			char * temp = mir_utf8decode((char*)login->nick.c_str(), NULL);
			//nick speichern
			db_set_s(NULL, protocolname, "Nick", temp);
			//uid speichern
			db_set_dw(NULL, protocolname, "myuid", login->myuid);
			m_myuid = login->myuid;
			//avatar auslesen
			GetBuddyInfo* buddyinfo = new GetBuddyInfo();
			buddyinfo->userid = login->myuid;
			mir_forkthread(SetAvatar2, (LPVOID)buddyinfo);
			break;
		}

	case XFIRE_RECV_OLDVERSION_PACKET_ID:
		{
			RecvOldVersionPacket *version = (RecvOldVersionPacket*)content;
			char temp[255];

			if ((unsigned int)m_client->m_protocolVersion < (unsigned int)version->newversion) {
				db_set_b(NULL, protocolname, "protover", version->newversion);
				//recprotoverchg
				if (db_get_w(NULL, protocolname, "recprotoverchg", 0) == 0) {
					mir_snprintf(temp, Translate("The protocol version is too old. Changed current version from %d to %d. You can reconnect now."), 
						m_client->m_protocolVersion, version->newversion);
					MSGBOXE(temp);
				}
				else {
					SetStatus(ID_STATUS_RECONNECT, NULL);
					return;
				}
			}
			else {
				mir_snprintf(temp, Translate("The protocol version is too old. Cannot detect a new version number."));
				MSGBOXE(temp);
				SetStatus(ID_STATUS_OFFLINE, NULL);
			}
			break;
		}

	case XFIRE_OTHER_LOGIN:
		MSGBOXE(Translate("Someone logged in with your account. Disconnect."));
		SetStatus(ID_STATUS_OFFLINE, NULL);
		break;

		//ne nachricht f�r mich, juhu
	case XFIRE_MESSAGE_ID: 
		string str;

		if (((MessagePacket*)content)->getMessageType() == 0) {
			BuddyListEntry *entry = m_client->getBuddyList()->getBuddyBySid(((MessagePacket*)content)->getSid());
			if (entry != NULL) {
				str = ((MessagePacket*)content)->getMessage();

				CallService(MS_PROTO_CONTACTISTYPING, (WPARAM)entry->m_hcontact, PROTOTYPE_CONTACTTYPING_OFF);

				PROTORECVEVENT pre = { 0 };
				pre.timestamp = time(NULL);
				pre.szMessage = (char*)str.c_str();
				ProtoChainRecvMsg(entry->m_hcontact, &pre);
			}
		}
		else if (((MessagePacket*)content)->getMessageType() == 3) {
			BuddyListEntry *entry = m_client->getBuddyList()->getBuddyBySid(((MessagePacket*)content)->getSid());
			if (entry != NULL)
				CallService(MS_PROTO_CONTACTISTYPING, (WPARAM)entry->m_hcontact, 5);
		}

		break;
	}
}

//=====================================================

extern "C" __declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD)
{
	return &pluginInfo;
}

//=====================================================
// Unloads plugin
//=====================================================

extern "C" __declspec(dllexport) int  Unload(void)
{
	//urlprefix raushaun
	if (ServiceExists(MS_ASSOCMGR_ADDNEWURLTYPE))
		CallService(MS_ASSOCMGR_REMOVEURLTYPE, 0, (LPARAM)"xfire:");

	//gamedetetion das dead signal geben
	SetEvent(hGameDetection);

#ifndef NO_PTHREAD
	pthread_cancel (gamedetection);
	pthread_win32_process_detach_np ();
#endif

	Gdiplus::GdiplusShutdown(gdiplusToken);

	return 0;
}

void __stdcall XFireLog(const char* fmt, ...)
{
	va_list vararg;
	va_start(vararg, fmt);
	char* str = (char*)alloca(32000);
	mir_vsnprintf(str, 32000, fmt, vararg);
	va_end(vararg);

	CallService(MS_NETLIB_LOG, (WPARAM)hNetlib, (LPARAM)str);
}

//=====================================================
// WINAPI DllMain
//=====================================================

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD, LPVOID)
{
	hinstance = hinst;
	//AtlAxWinInit();
	return TRUE;
}

//suche nach ini und danach starte gamedetection thread
void StartIniUpdateAndDetection(LPVOID)
{
	mir_cslock lck(connectingMutex);

	//ini/ico updater, nur wenn aktiv
	if (db_get_b(NULL, protocolname, "autoiniupdate", 0))
		UpdateMyXFireIni(NULL);
	if (db_get_b(NULL, protocolname, "autoicodllupdate", 0))
		UpdateMyIcons(NULL);

#ifndef NO_PTHREAD
	void* (*func)(void*) = &inigamedetectiont;
	pthread_create( &gamedetection, NULL, func , NULL);
#else
	mir_forkthread(inigamedetectiont, NULL);
#endif
}

INT_PTR UrlCall(WPARAM, LPARAM lparam)
{
	//lparam!=0?
	if (lparam) {
		//nach dem doppelpunkt suchen
		char*type = strchr((char*)lparam, ':');
		//gefunden, dann anch fragezeichen suchen
		if (type) {
			type++;
			char*q = strchr(type, '?');
			//gefunden? dann urltype ausschneiden
			if (q) {
				//abschneiden
				*q = 0;
				//ein addfriend url request?
				if (mir_strcmp("add_friend", type) == 0) {
					q++;
					//nach = suchen
					char*g = strchr(q, '=');
					//gefunden? dann abschneiden
					if (g) {
						*g = 0;
						g++;
						//user parameter?
						if (mir_strcmp("user", q) == 0) {
							//tempbuffer f�r die frage and en user
							char temp[100];

							if (mir_strlen(g) > 25) //zugro�e abschneiden
								*(g + 25) = 0;

							mir_snprintf(temp, Translate("Do you really want to add %s to your friend list?"), g);
							//Nutzer vorher fragen, ob er wirklich user xyz adden m�chte
							if (MessageBoxA(NULL, temp, Translate(PLUGIN_TITLE), MB_YESNO | MB_ICONQUESTION) == IDYES) {
								if (myClient != NULL) {
									if (myClient->m_client->m_connected) {
										InviteBuddyPacket invite;
										invite.addInviteName(g, Translate("Add me to your friend list."));
										myClient->m_client->send(&invite);
									}
									else
										MSGBOXE(Translate("XFire is not connected."));
								}
								else
									MSGBOXE(Translate("XFire is not connected."));
							}
						}
					}
				}
			}
		}

	}
	return 0;
}

//wenn alle module geladen sind
static int OnSystemModulesLoaded(WPARAM, LPARAM)
{
	/*NETLIB***********************************/
	NETLIBUSER nlu;
	memset(&nlu, 0, sizeof(nlu));
	nlu.cbSize = sizeof(nlu);
	nlu.flags = NUF_OUTGOING | NUF_HTTPCONNS;
	nlu.szSettingsModule = protocolname;
	nlu.szDescriptiveName = "XFire server connection";
	hNetlib = (HANDLE)CallService(MS_NETLIB_REGISTERUSER, 0, (LPARAM)& nlu);
	/*NETLIB***********************************/

	HookEvent(ME_USERINFO_INITIALISE, OnDetailsInit);
	HookEvent(ME_DB_CONTACT_DELETED, ContactDeleted);

	//hook das queryplugin
	HookEvent("GameServerQuery/doneQuery", doneQuery);

	CreateProtoServiceFunction(protocolname, PS_SETAWAYMSG, SetAwayMsg);

	// Variables support
	if (ServiceExists(MS_VARS_REGISTERTOKEN)) {
		TOKENREGISTER tr = { 0 };
		tr.cbSize = sizeof(TOKENREGISTER);
		tr.memType = TR_MEM_MIRANDA;
		tr.flags = TRF_FREEMEM | TRF_PARSEFUNC | TRF_FIELD;

		tr.szTokenString = "xfiregame";
		tr.parseFunction = Varxfiregame;
		tr.szHelpText = LPGEN("XFire") "\t" LPGEN("Current Game");
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM)&tr);

		tr.szTokenString = "myxfiregame";
		tr.parseFunction = Varmyxfiregame;
		tr.szHelpText = LPGEN("XFire") "\t" LPGEN("My Current Game");
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM)&tr);

		tr.szTokenString = "xfireserverip";
		tr.parseFunction = Varxfireserverip;
		tr.szHelpText = LPGEN("XFire") "\t" LPGEN("ServerIP");
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM)&tr);

		tr.szTokenString = "myxfireserverip";
		tr.parseFunction = Varmyxfireserverip;
		tr.szHelpText = LPGEN("XFire") "\t" LPGEN("My Current ServerIP");
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM)&tr);

		tr.szTokenString = "xfirevoice";
		tr.parseFunction = Varxfirevoice;
		tr.szHelpText = LPGEN("XFire") "\t" LPGEN("Voice");
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM)&tr);

		tr.szTokenString = "myxfirevoice";
		tr.parseFunction = Varmyxfirevoice;
		tr.szHelpText = LPGEN("XFire") "\t" LPGEN("My Current Voice");
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM)&tr);

		tr.szTokenString = "xfirevoiceip";
		tr.parseFunction = Varxfirevoiceip;
		tr.szHelpText = LPGEN("XFire") "\t" LPGEN("Voice ServerIP");
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM)&tr);

		tr.szTokenString = "myxfirevoiceip";
		tr.parseFunction = Varmyxfirevoiceip;
		tr.szHelpText = LPGEN("XFire") "\t" LPGEN("My Voice ServerIP");
		CallService(MS_VARS_REGISTERTOKEN, 0, (LPARAM)&tr);
	}

	//File Association Manager support
	if (ServiceExists(MS_ASSOCMGR_ADDNEWURLTYPE)) {
		AssocMgr_AddNewUrlType("xfire:", Translate("Xfire Link Protocol"), hinstance, IDI_TM, XFIRE_URLCALL, 0);
	}

	//sound einf�gen
	SkinAddNewSoundEx("xfirebstartgame", protocolname, LPGEN("Buddy start a game"));

	//hook f�r mbot einf�gen, nur wenn mbot option aktiv
	if (db_get_b(NULL, protocolname, "mbotsupport", 0))
		HookEvent(XFIRE_INGAMESTATUSHOOK, mBotNotify);

	//initialisiere teamspeak und co detection
	voicechat.initVoicechat();

	mir_forkthread(StartIniUpdateAndDetection, NULL);

	return 0;
}

//=====================================================
// Called when plugin is loaded into Miranda
//=====================================================

int ExtraListRebuild(WPARAM, LPARAM)
{
	//f�r alle gameicons ein neues handle setzen
	return xgamelist.iconmngr.resetIconHandles();
}

int ExtraImageApply1(WPARAM hContact, LPARAM)
{
	char *szProto = GetContactProto(hContact);
	if (szProto != NULL && !mir_strcmpi(szProto, protocolname) && db_get_w(hContact, protocolname, "Status", ID_STATUS_OFFLINE) != ID_STATUS_OFFLINE) {
		int gameid = db_get_w(hContact, protocolname, "GameId", 0);
		if (gameid != 0)
			ExtraIcon_SetIcon(hExtraIcon1, hContact, xgamelist.iconmngr.getGameIconHandle(gameid));
	}
	return 0;
}

int ExtraImageApply2(WPARAM hContact, LPARAM)
{
	// TODO: maybe need to fix extra icons
	char *szProto = GetContactProto(hContact);
	if (szProto != NULL && !mir_strcmpi(szProto, protocolname) && db_get_w(hContact, protocolname, "Status", ID_STATUS_OFFLINE) != ID_STATUS_OFFLINE) {
		int gameid = db_get_w(hContact, protocolname, "VoiceId", 0);
		if (gameid != 0)
			ExtraIcon_SetIcon(hExtraIcon2, hContact, xgamelist.iconmngr.getGameIconHandle(gameid));
	}
	return 0;
}



extern "C" __declspec(dllexport) int  Load(void)
{
	mir_getLP(&pluginInfo);

	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	//keine protoversion in der db, dann wohl der erste start von xfire
	if (db_get_b(NULL, protocolname, "protover", 0) == 0) {
		db_set_b(NULL, protocolname, "protover", 0x84);
		db_set_w(NULL, protocolname, "avatarloadlatency", 1000);
		db_set_b(NULL, protocolname, "gameico", 0);
		db_set_b(NULL, protocolname, "voiceico", 1);
		db_set_b(NULL, protocolname, "specialavatarload", 1);
		db_set_b(NULL, protocolname, "xfiresitegameico", 1);
		db_set_b(NULL, protocolname, "recprotoverchg", 1);

		if (MessageBox(NULL, TranslateT("It seems that is the first time you use this plugin. Do you want to automatically download the latest available xfire_games.ini and icons.dll?\r\nWithout the xfire_games.ini Xfire can't detect any games on your computer."), TranslateT(PLUGIN_TITLE), MB_YESNO | MB_ICONQUESTION) == IDYES) {
			db_set_b(NULL, protocolname, "autoiniupdate", 1);
			db_set_b(NULL, protocolname, "autoicodllupdate", 1);
		}
	}


	XDEBUGS("-----------------------------------------------------\n");

	//statusmessages setzen
	mir_strcpy(statusmessage[0], "");
	mir_snprintf(statusmessage[1], "(AFK) %s", Translate("Away from Keyboard"));

	HookEvent(ME_OPT_INITIALISE, OptInit);
	HookEvent(ME_SYSTEM_MODULESLOADED, OnSystemModulesLoaded);

	PROTOCOLDESCRIPTOR pd = { PROTOCOLDESCRIPTOR_V3_SIZE };
	pd.szName = protocolname;
	pd.type = PROTOTYPE_PROTOCOL;
	Proto_RegisterModule(&pd);

	hLogEvent = CreateHookableEvent("XFireProtocol/Log");

	CList_MakeAllOffline();

	CreateProtoServiceFunction(protocolname, PS_GETCAPS, GetCaps);
	CreateProtoServiceFunction(protocolname, PS_GETNAME, GetName);
	CreateProtoServiceFunction(protocolname, PS_LOADICON, TMLoadIcon);
	CreateProtoServiceFunction(protocolname, PS_SETSTATUS, SetStatus);
	CreateProtoServiceFunction(protocolname, PS_GETSTATUS, GetStatus);
	CreateProtoServiceFunction(protocolname, PSS_ADDED, AddtoList);
	CreateProtoServiceFunction(protocolname, PS_ADDTOLIST, SearchAddtoList);
	CreateProtoServiceFunction(protocolname, PS_GETAVATARINFO, GetAvatarInfo);
	CreateProtoServiceFunction(protocolname, PS_GETMYAVATAR, GetMyAvatar);

	HookEvent(ME_CLIST_EXTRA_LIST_REBUILD, ExtraListRebuild);

	//erstell eine hook f�r andere plugins damit diese nachpr�fen k�nnen, ab wann jemand ingame ist oer nicht
	hookgamestart = CreateHookableEvent(XFIRE_INGAMESTATUSHOOK);

	CreateProtoServiceFunction(protocolname, PS_BASICSEARCH, BasicSearch);
	CreateProtoServiceFunction(protocolname, PSS_MESSAGE, SendMessage);
	CreateProtoServiceFunction(protocolname, PSS_USERISTYPING, UserIsTyping);
	CreateProtoServiceFunction(protocolname, PSR_MESSAGE, RecvMessage);
	CreateProtoServiceFunction(protocolname, XFIRE_URLCALL, UrlCall);
	///CreateProtoServiceFunction( protocolname, PSS_GETAWAYMSG, GetAwayMsg );
	CreateProtoServiceFunction(protocolname, XFIRE_SET_NICK, SetNickName);
	CreateProtoServiceFunction(protocolname, XFIRE_SEND_PREFS, SendPrefs);

	//f�r mtipper, damit man das statusico �bertragen kann
	CreateProtoServiceFunction(protocolname, PS_GETCUSTOMSTATUSICON, GetXStatusIcon);

	char AvatarsFolder[MAX_PATH] = "";
	char CurProfileF[MAX_PATH] = "";
	char CurProfile[MAX_PATH] = "";
	CallService(MS_DB_GETPROFILEPATH, (WPARAM)MAX_PATH, (LPARAM)AvatarsFolder);
	mir_strcat(AvatarsFolder, "\\");
	CallService(MS_DB_GETPROFILENAME, (WPARAM)MAX_PATH, (LPARAM)CurProfileF);

	int i;
	for (i = MAX_PATH - 1; i > 5; i--) {
		if (CurProfileF[i] == 't' && CurProfileF[i - 3] == '.') {
			i -= 3;
			break;
		}
	}
	memcpy(CurProfile, CurProfileF, i);
	mir_strcat(AvatarsFolder, CurProfile);
	mir_strcat(AvatarsFolder, "\\");
	mir_strcat(AvatarsFolder, "XFire");

	XFireWorkingFolder = FoldersRegisterCustomPath(protocolname, "Working Folder", AvatarsFolder);
	if (!(XFireIconFolder = FoldersRegisterCustomPath(protocolname, "Game Icon Folder", AvatarsFolder)))
		CreateDirectoryA(AvatarsFolder, NULL);

	mir_strcat(AvatarsFolder, "\\Avatars");
	if (!(XFireAvatarFolder = FoldersRegisterCustomPath(protocolname, "Avatars", AvatarsFolder)))
		CreateDirectoryA(AvatarsFolder, NULL);

	// erweiterte Kontextmen�punkte
	CMenuItem mi;
	mi.position = 500090000;
	mi.name.a = protocolname;
	mi.root = Menu_AddContactMenuItem(&mi, protocolname);
	mi.flags = CMIF_TCHAR;

	char servicefunction[100];

	// gotoprofilemen�punkt
	CreateProtoServiceFunction(protocolname, "/GotoProfile", GotoProfile);
	mi.pszService = "/GotoProfile";
	mi.hIcolibItem = LoadIcon(hinstance, MAKEINTRESOURCE(ID_OP));
	mi.name.t = LPGENT("&XFire Online Profile");
	Menu_AddContactMenuItem(&mi, protocolname);

	// gotoxfireclansitemen�punkt
	CreateProtoServiceFunction(protocolname, "/GotoXFireClanSite", GotoXFireClanSite);
	mi.pszService = "/GotoXFireClanSite";
	mi.hIcolibItem = LoadIcon(hinstance, MAKEINTRESOURCE(ID_OP));
	mi.name.t = LPGENT("XFire &Clan Site");
	gotoclansite = Menu_AddContactMenuItem(&mi, protocolname);

	// kopiermen�punkt
	CreateProtoServiceFunction(protocolname, "/GetIPPort", GetIPPort);
	mi.pszService = "/GetIPPort";
	mi.hIcolibItem = LoadIcon(hinstance, MAKEINTRESOURCE(ID_OP));
	mi.name.t = LPGENT("C&opy Server Address and Port");
	copyipport = Menu_AddContactMenuItem(&mi, protocolname);

	// kopiermen�punkt
	CreateProtoServiceFunction(protocolname, "/VoiceIPPort", GetVIPPort);
	mi.pszService = "/VoiceIPPort";
	mi.hIcolibItem = LoadIcon(hinstance, MAKEINTRESOURCE(ID_OP));
	mi.name.t = LPGENT("Cop&y Voice Server Address and Port");
	vipport = Menu_AddContactMenuItem(&mi, protocolname);

	// joinmen�punkt
	CreateProtoServiceFunction(protocolname, "/JoinGame", JoinGame);
	mi.pszService = "/JoinGame";
	mi.hIcolibItem = LoadIcon(hinstance, MAKEINTRESOURCE(ID_OP));
	mi.name.t = LPGENT("Join &Game...");
	joingame = Menu_AddContactMenuItem(&mi, protocolname);

	// playmen�punkt
	CreateProtoServiceFunction(protocolname, "/StartThisGame", StartThisGame);
	mi.pszService = "/StartThisGame";
	mi.hIcolibItem = LoadIcon(hinstance, MAKEINTRESOURCE(ID_OP));
	mi.name.t = LPGENT("Play this game...");
	startthisgame = Menu_AddContactMenuItem(&mi, protocolname);

	// remove friend
	CreateProtoServiceFunction(protocolname, "/RemoveFriend", RemoveFriend);
	mi.pszService = "/RemoveFriend";
	mi.position = 2000070000;
	mi.hIcolibItem = LoadIcon(hinstance, MAKEINTRESOURCE(ID_OP));
	mi.name.t = LPGENT("Remove F&riend...");
	removefriend = Menu_AddContactMenuItem(&mi, protocolname);

	// block user
	CreateProtoServiceFunction(protocolname, "/BlockFriend", BlockFriend);
	mi.pszService = "/BlockFriend";
	mi.hIcolibItem = LoadIcon(hinstance, MAKEINTRESOURCE(ID_OP));
	mi.name.t = LPGENT("Block U&ser...");
	blockfriend = Menu_AddContactMenuItem(&mi, protocolname);

	// main menu items
	// my fire profile
	mi.root = Menu_CreateRoot(MO_MAIN, _T(protocolname), 500090000);
	strncpy_s(servicefunction, protocolname, _TRUNCATE);
	strncat_s(servicefunction, "GotoProfile2", _TRUNCATE);
	CreateServiceFunction(servicefunction, GotoProfile2);
	mi.position = 500090000;
	mi.hIcolibItem = LoadIcon(hinstance, MAKEINTRESOURCE(ID_OP));
	mi.name.t = LPGENT("&My XFire Online Profile");
	Menu_AddMainMenuItem(&mi);

	// my activity protocol
	strncpy_s(servicefunction, protocolname, _TRUNCATE);
	strncat_s(servicefunction, "GotoProfileAct", _TRUNCATE);
	CreateServiceFunction(servicefunction, GotoProfileAct);
	mi.hIcolibItem = LoadIcon(hinstance, MAKEINTRESOURCE(ID_OP));
	mi.name.t = LPGENT("&Activity Report");
	Menu_AddMainMenuItem(&mi);

	//rescan my games
	strncpy_s(servicefunction, protocolname, _TRUNCATE);
	strncat_s(servicefunction, "ReScanMyGames", _TRUNCATE);
	CreateServiceFunction(servicefunction, ReScanMyGames);
	mi.hIcolibItem = LoadIcon(hinstance, MAKEINTRESOURCE(ID_OP));
	mi.name.t = LPGENT("&Rescan my games...");
	Menu_AddMainMenuItem(&mi);

	strncpy_s(servicefunction, protocolname, _TRUNCATE);
	strncat_s(servicefunction, "SetNick", _TRUNCATE);
	CreateServiceFunction(servicefunction, SetNickDlg);
	mi.hIcolibItem = LoadIcon(hinstance, MAKEINTRESOURCE(ID_OP));
	mi.name.t = LPGENT("Set &Nickname");
	Menu_AddMainMenuItem(&mi);

	HookEvent(ME_CLIST_PREBUILDCONTACTMENU, RebuildContactMenu);

	if (db_get_b(NULL, protocolname, "ipportdetec", 0)) {
		//MessageBoxA(0,"GetExtendedUdpTable not found. ServerIP/Port detection feature will be disabled.",PLUGIN_TITLE,MB_OK|MB_ICONINFORMATION);
		db_set_b(NULL, protocolname, "ipportdetec", 0);
		XFireLog("Wasn't able to get GetExtendedUdpTable function");
	}

	Icon_Register(hinstance, LPGEN("Protocols") "/" LPGEN("XFire"), &icon, 1);

	hExtraIcon1 = ExtraIcon_RegisterCallback("xfire_game", LPGEN("XFire game icon"), "", NULL, ExtraImageApply1);
	hExtraIcon2 = ExtraIcon_RegisterCallback("xfire_voice", LPGEN("XFire voice icon"), "", NULL, ExtraImageApply2);
	return 0;
}

//funktion liefert f�r xstatusid den passenden ico zur�ck, f�r tipper zb notwendig
INT_PTR GetXStatusIcon(WPARAM wParam, LPARAM lParam)
{
	if (lParam == LR_SHARED) {
		if (wParam > 1)
			return (INT_PTR)xgamelist.iconmngr.getGameIconFromId(wParam - 2); //icocache[(int)wParam-2].hicon;
	}
	else {
		if (wParam > 1)
			return (INT_PTR)CopyIcon((HICON)xgamelist.iconmngr.getGameIconFromId(wParam - 2)/*icocache[(int)wParam-2].hicon*/);
	}

	return 0;
}

INT_PTR RecvMessage(WPARAM wParam, LPARAM lParam)
{
	CCSDATA *ccs = (CCSDATA*)lParam;
	db_unset(ccs->hContact, "CList", "Hidden");

	char *szProto = GetContactProto(ccs->hContact);
	if (szProto != NULL && !mir_strcmpi(szProto, protocolname))
		return CallService(MS_PROTO_RECVMSG, wParam, lParam);

	return 1;
}

static void SetMeAFK(LPVOID param)
{
	if (bpStatus == ID_STATUS_ONLINE) {
		SetStatus(ID_STATUS_AWAY, (LPARAM)param);
	}
}

static void SetStatusLate(LPVOID param)
{
	Sleep(1000);
	if (bpStatus == ID_STATUS_OFFLINE) {
		SetStatus((WPARAM)param, 0);
	}
}

static void SendAck(LPVOID param)
{
	ProtoBroadcastAck(protocolname, (DWORD_PTR)param, ACKTYPE_MESSAGE, ACKRESULT_SUCCESS, (HANDLE)1, 0);
}

static void SendBadAck(LPVOID param)
{
	ProtoBroadcastAck(protocolname, (DWORD_PTR)param, ACKTYPE_MESSAGE, ACKRESULT_FAILED, (HANDLE)0, LPARAM(Translate("XFire does not support offline messaging!")));
}

static INT_PTR UserIsTyping(WPARAM hContact, LPARAM lParam)
{
	DBVARIANT dbv;

	if (lParam == PROTOTYPE_SELFTYPING_ON) {
		if (db_get_b(NULL, protocolname, "sendtyping", 1) == 1) {
			if (myClient != NULL)
				if (myClient->m_client->m_connected)
					if (!db_get_s(hContact, protocolname, "Username", &dbv)) {
						SendTypingPacket typing;
						typing.init(myClient->m_client, dbv.pszVal);
						myClient->m_client->send(&typing);
						db_free(&dbv);
					}
		}
	}
	else if (lParam == PROTOTYPE_SELFTYPING_OFF) {
	}

	return 0;
}

INT_PTR SendMessage(WPARAM, LPARAM lParam)
{
	CCSDATA *ccs = (CCSDATA *)lParam;
	DBVARIANT dbv;
	int sended = 0;

	if (db_get_s(ccs->hContact, protocolname, "Username", &dbv))
		return 0;

	if (myClient != NULL)
		if (myClient->m_client->m_connected && db_get_w(ccs->hContact, protocolname, "Status", -1) != ID_STATUS_OFFLINE) {
			myClient->sendmsg(dbv.pszVal, ptrA(mir_utf8encode((char*)ccs->lParam)));
			mir_forkthread(SendAck, (void*)ccs->hContact);
			sended = 1;
		}
		else mir_forkthread(SendBadAck, (void*)ccs->hContact);

		db_free(&dbv);
		return sended;
}

//=======================================================
// GetCaps
//=======================================================

INT_PTR GetCaps(WPARAM wParam, LPARAM)
{
	if (wParam == PFLAGNUM_1)
		return PF1_BASICSEARCH | PF1_MODEMSG | PF1_IM/*|PF1_SERVERCLIST*/;
	else if (wParam == PFLAGNUM_2)
		return PF2_ONLINE | PF2_SHORTAWAY; // add the possible statuses here.
	else if (wParam == PFLAGNUM_3)
		return PF2_ONLINE | (db_get_b(NULL, protocolname, "nocustomaway", 0) == 1 ? 0 : PF2_SHORTAWAY);
	else if (wParam == PFLAGNUM_4)
		return PF4_SUPPORTTYPING | PF4_AVATARS;
	else if (wParam == PFLAG_UNIQUEIDTEXT)
		return (INT_PTR)Translate("Username");
	else if (wParam == PFLAG_UNIQUEIDSETTING)
		return (INT_PTR)"Username";
	else if (wParam == PFLAG_MAXLENOFMESSAGE)
		return 3996; //255;
	return 0;
}

//=======================================================
// GetName (tray icon)
//=======================================================

INT_PTR GetName(WPARAM wParam, LPARAM lParam)
{
	mir_strncpy((char*)lParam, "XFire", wParam);
	return 0;
}

//=======================================================
// TMLoadIcon
//=======================================================

INT_PTR TMLoadIcon(WPARAM wParam, LPARAM)
{
	if (LOWORD(wParam) == PLI_PROTOCOL) {
		if (wParam & PLIF_ICOLIB)
			return (INT_PTR)IcoLib_GetIcon("XFIRE_main");
		return (INT_PTR)CopyIcon(IcoLib_GetIcon("XFIRE_main"));
	}
	return NULL;
}


static void ConnectingThread(LPVOID params)
{
	WPARAM wParam = (WPARAM)params;

	mir_cslock lck(connectingMutex);

	if (myClient != NULL&&myClient->m_client != NULL)
		myClient->run();
	else
		return;

	if (myClient->m_client->m_connected)
		sendonrecieve = TRUE;
	else {
		if (db_get_w(NULL, protocolname, "noconnectfailedbox", 0) == 0) MSGBOXE(Translate("Unable to connect to XFire."));
		wParam = ID_STATUS_OFFLINE;
	}

	int oldStatus;
	oldStatus = bpStatus;
	bpStatus = wParam;

	ProtoBroadcastAck(protocolname, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)oldStatus, wParam);
}

//=======================================================
// SetStatus
//=======================================================

INT_PTR SetStatus(WPARAM wParam, LPARAM)
{
	int oldStatus = bpStatus;

	if (bpStatus == ID_STATUS_CONNECTING)
		return 0;

	if (wParam != ID_STATUS_ONLINE&&wParam != ID_STATUS_OFFLINE&&wParam != ID_STATUS_AWAY&&wParam != ID_STATUS_RECONNECT)
		if (db_get_b(NULL, protocolname, "oninsteadafk", 0) == 0)
			wParam = ID_STATUS_AWAY; //protokoll auf away schalten
		else
			wParam = ID_STATUS_ONLINE; //protokoll auf online schalten

	if (
		(wParam == ID_STATUS_ONLINE && bpStatus != ID_STATUS_ONLINE) || // offline --> online
		(wParam == ID_STATUS_AWAY && bpStatus == ID_STATUS_OFFLINE) // offline --> away
		) {
		if (bpStatus == ID_STATUS_AWAY) // away --> online
		{
			myClient->Status(statusmessage[0]);
		}
		else {
			// the status has been changed to online (maybe run some more code)
			DBVARIANT dbv;
			DBVARIANT dbv2;

			if (db_get(NULL, protocolname, "login", &dbv)) {
				MSGBOXE(Translate("No Login name is set!"));
				wParam = ID_STATUS_OFFLINE;
			}
			else if (db_get(NULL, protocolname, "password", &dbv2)) {
				MSGBOXE(Translate("No Password is set!"));
				wParam = ID_STATUS_OFFLINE;
			}
			else {
				if (myClient != NULL)
					delete myClient;

				myClient = new XFireClient(dbv.pszVal, dbv2.pszVal, db_get_b(NULL, protocolname, "protover", 0));

				//verbindung als thread
				bpStatus = ID_STATUS_CONNECTING;
				ProtoBroadcastAck(protocolname, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)oldStatus, ID_STATUS_CONNECTING);

				mir_forkthread(ConnectingThread, (LPVOID)wParam);

				//f�r die vars
				db_unset(NULL, protocolname, "currentgamename");
				db_unset(NULL, protocolname, "currentvoicename");
				db_unset(NULL, protocolname, "VServerIP");
				db_unset(NULL, protocolname, "ServerIP");

				db_free(&dbv);
				db_free(&dbv2);
				return 0;
			}
		}
	}
	else if (wParam == ID_STATUS_AWAY && bpStatus != ID_STATUS_AWAY) {
		if (bpStatus == ID_STATUS_OFFLINE) // nix
		{
		}
		else if (myClient != NULL&&myClient->m_client->m_connected) // online --> afk
		{
			//setze bei aktivem nocustomaway die alte awaystatusmsg zur�ck, bugfix
			if (db_get_b(NULL, protocolname, "nocustomaway", 0))
				mir_snprintf(statusmessage[1], "(AFK) %s", Translate("Away from Keyboard"));

			myClient->Status(statusmessage[1]);
		}
	}
	else if ((wParam == ID_STATUS_OFFLINE || wParam == ID_STATUS_RECONNECT) && bpStatus != ID_STATUS_OFFLINE) // * --> offline
	{
		SetEvent(hConnectionClose);

		// the status has been changed to offline (maybe run some more code)
		if (myClient != NULL)
			if (myClient->m_client->m_connected)
				myClient->m_client->disconnect();
		CList_MakeAllOffline();

		//teamspeak/ventrilo pid sowie gamepid auf NULL setzen, damit bei einem reconnect die neuerkannt werden
		pid = NULL;
		ts2pid = NULL;
		db_set_w(NULL, protocolname, "currentgame", 0);
		db_set_w(NULL, protocolname, "currentvoice", 0);
		db_unset(NULL, protocolname, "VServerIP");
		db_unset(NULL, protocolname, "ServerIP");

		if (wParam == ID_STATUS_RECONNECT) {
			mir_forkthread(SetStatusLate, (LPVOID)oldStatus);
			wParam = ID_STATUS_OFFLINE;
		}
	}
	else {
		// the status has been changed to unknown  (maybe run some more code)
	}
	//broadcast the message
	bpStatus = wParam;
	ProtoBroadcastAck(protocolname, NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)oldStatus, wParam);


	return 0;
}

//=======================================================
// GetStatus
//=======================================================

INT_PTR GetStatus(WPARAM, LPARAM)
{
	if (bpStatus == ID_STATUS_ONLINE)
		return ID_STATUS_ONLINE;
	else if (bpStatus == ID_STATUS_AWAY)
		return ID_STATUS_AWAY;
	else if (bpStatus == ID_STATUS_CONNECTING)
		return ID_STATUS_CONNECTING;
	else
		return ID_STATUS_OFFLINE;
}

MCONTACT CList_AddContact(XFireContact xfc, bool InList, bool SetOnline, int clan)
{
	MCONTACT hContact;

	if (xfc.username == NULL)
		return 0;

	// here we create a new one since no one is to be found
	hContact = (MCONTACT)CallService(MS_DB_CONTACT_ADD, 0, 0);
	if (hContact) {
		Proto_AddToContact(hContact, protocolname);

		if (InList)
			db_unset(hContact, "CList", "NotOnList");
		else
			db_set_b(hContact, "CList", "NotOnList", 1);
		db_unset(hContact, "CList", "Hidden");

		if (mir_strlen(xfc.nick) > 0) {
			db_set_utf(hContact, protocolname, "Nick", xfc.nick);
		}
		else if (mir_strlen(xfc.username) > 0) {
			db_set_s(hContact, protocolname, "Nick", xfc.username);
		}

		db_set_s(hContact, protocolname, "Username", xfc.username);

		//db_set_s(hContact, protocolname, "Screenname", xfc.nick);
		db_set_dw(hContact, protocolname, "UserId", xfc.id);

		if (clan > 0)
			db_set_dw(hContact, protocolname, "Clan", clan);

		db_set_w(hContact, protocolname, "Status", SetOnline ? ID_STATUS_ONLINE : ID_STATUS_OFFLINE);

		if (db_get_b(NULL, protocolname, "noavatars", -1) == 0) {
			if (!db_get_b(NULL, protocolname, "specialavatarload", 0)) {
				XFire_SetAvatar* xsa = new XFire_SetAvatar;
				xsa->hContact = hContact;
				xsa->username = new char[mir_strlen(xfc.username) + 1];
				mir_strcpy(xsa->username, xfc.username);
				mir_forkthread(SetAvatar, (LPVOID)xsa);
			}
			else {
				/*
					scheinbar unterpricht xfire bei zu agressiven nachfragen der buddyinfos die verbindung , deshalb erstmal auskommentiert
					getestet mit clanbuddy's >270 members

					mit hilfe der buddyinfos kann man den avatar laden und screenshot infos etc bekommt man auch
					*/
				GetBuddyInfo* buddyinfo = new GetBuddyInfo();
				buddyinfo->userid = xfc.id;
				mir_forkthread(SetAvatar2, (LPVOID)buddyinfo);
			}


		}

		if (xfc.id == 0) {
			db_set_b(hContact, "CList", "NotOnList", 1);
			db_set_b(hContact, "CList", "Hidden", 1);
		}

		return hContact;
	}
	return false;
}

BOOL IsXFireContact(MCONTACT hContact)
{
	char *szProto = GetContactProto(hContact);
	if (szProto != NULL && !mir_strcmpi(szProto, protocolname))
		return TRUE;

	return FALSE;
}

MCONTACT CList_FindContact(int uid)
{
	for (MCONTACT hContact = db_find_first(protocolname); hContact; hContact = db_find_next(hContact, protocolname))
		if (db_get_dw(hContact, protocolname, "UserId", -1) == DWORD(uid))
			return hContact;

	return 0;
}

void CList_MakeAllOffline()
{
	vector<MCONTACT> fhandles;
	for (MCONTACT hContact = db_find_first(protocolname); hContact; hContact = db_find_next(hContact, protocolname)) {
		//freunde von freunden in eine seperate liste setzen
		//nur wenn das nicht abgestellt wurde
		if (db_get_b(hContact, protocolname, "friendoffriend", 0) == 1 && db_get_b(NULL, protocolname, "fofdbremove", 0) == 1)
			fhandles.push_back(hContact);

		db_unset(hContact, "CList", "StatusMsg");
		db_unset(hContact, protocolname, "ServerIP");
		db_unset(hContact, protocolname, "Port");
		db_unset(hContact, protocolname, "ServerName");
		db_unset(hContact, protocolname, "GameType");
		db_unset(hContact, protocolname, "Map");
		db_unset(hContact, protocolname, "Players");
		db_unset(hContact, protocolname, "Passworded");

		db_unset(hContact, protocolname, "XStatusMsg");
		db_unset(hContact, protocolname, "XStatusId");
		db_unset(hContact, protocolname, "XStatusName");

		if (db_get_b(NULL, protocolname, "noavatars", -1) == 1) {
			db_unset(hContact, "ContactPhoto", "File");
			db_unset(hContact, "ContactPhoto", "RFile");
			db_unset(hContact, "ContactPhoto", "Backup");
			db_unset(hContact, "ContactPhoto", "Format");
			db_unset(hContact, "ContactPhoto", "ImageHash");
			db_unset(hContact, "ContactPhoto", "XFireAvatarId");
			db_unset(hContact, "ContactPhoto", "XFireAvatarMode");
		}
		else {
			//pr�f ob der avatar noch existiert
			DBVARIANT dbv;
			if (!db_get_s(hContact, "ContactPhoto", "File", &dbv)) {
				FILE*f = fopen(dbv.pszVal, "r");
				if (f == NULL) {
					db_unset(hContact, "ContactPhoto", "File");
					db_unset(hContact, "ContactPhoto", "RFile");
					db_unset(hContact, "ContactPhoto", "Backup");
					db_unset(hContact, "ContactPhoto", "Format");
					db_unset(hContact, "ContactPhoto", "ImageHash");
					db_unset(hContact, "ContactPhoto", "XFireAvatarId");
					db_unset(hContact, "ContactPhoto", "XFireAvatarMode");
				}
				else fclose(f);

				db_free(&dbv);
			}
		}
		db_set_w(hContact, protocolname, "Status", ID_STATUS_OFFLINE);
	}

	//alle gefundenen handles ls�chen
	for (uint i = 0; i < fhandles.size(); i++)
		CallService(MS_DB_CONTACT_DELETE, (WPARAM)fhandles.at(i), 0);
}

void SetAvatar2(void *arg)
{
	static int lasttime = 0;
	int sleep = db_get_w(NULL, protocolname, "avatarloadlatency", 1000);
	lasttime += sleep;
	GetBuddyInfo *buddyinfo = (GetBuddyInfo*)arg;

	if (mySleep(lasttime, hConnectionClose)) {
		delete buddyinfo;
		lasttime -= sleep;
		return;
	}

	if (myClient != NULL)
		if (myClient->m_client->m_connected)
			myClient->m_client->send(buddyinfo);

	delete buddyinfo;
	lasttime -= sleep;
}

void SetAvatar(void *arg)
{
	static int lasttime = 0;
	int sleep = db_get_w(NULL, protocolname, "avatarloadlatency", 250);

	if (bpStatus == ID_STATUS_OFFLINE)
		return;


	XFire_SetAvatar* xsa = (XFire_SetAvatar*)arg;
	lasttime += sleep;
	//Sleep(lasttime);
	if (mySleep(lasttime, hConnectionClose)) {
		delete xsa;
		lasttime -= sleep;
		return;
	}

	if (bpStatus == ID_STATUS_OFFLINE)
		return;

	XFireAvatar av;

	if (xsa->hContact == NULL)
		return;

	if (GetAvatar(xsa->username, &av)) {
		PROTO_AVATAR_INFORMATION ai;
		ai.format = av.type;
		ai.hContact = xsa->hContact;
		_tcsncpy_s(ai.filename, _A2T(av.file), _TRUNCATE);
		ProtoBroadcastAck(protocolname, xsa->hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, (HANDLE)&ai, 0);
	}

	delete(xsa);

	lasttime -= sleep;
}

BOOL GetAvatar(char* username, XFireAvatar* av)
{
	BOOL status = FALSE;

	if (av == NULL || username == NULL)
		return FALSE;

	char address[256] = "http://www.xfire.com/profile/";
	strcat_s(address, 256, username);
	strcat_s(address, 256, "/");

	//netlib request
	NETLIBHTTPREQUEST nlhr = { 0 }, *nlhrReply;
	nlhr.cbSize = sizeof(nlhr);
	nlhr.requestType = REQUEST_GET;
	nlhr.flags = NLHRF_NODUMP | NLHRF_GENERATEHOST | NLHRF_SMARTAUTHHEADER;
	nlhr.szUrl = address;

	nlhrReply = (NETLIBHTTPREQUEST*)CallService(MS_NETLIB_HTTPTRANSACTION, (WPARAM)hNetlib, (LPARAM)&nlhr);

	if (nlhrReply) {
		//nicht auf dem server
		if (nlhrReply->resultCode != 200) {
			CallService(MS_NETLIB_FREEHTTPREQUESTSTRUCT, 0, (LPARAM)nlhrReply);
			return FALSE;
		}
		//keine daten f�r mich
		else if (nlhrReply->dataLength < 1 || nlhrReply->pData == NULL) {
			CallService(MS_NETLIB_FREEHTTPREQUESTSTRUCT, 0, (LPARAM)nlhrReply);
			return FALSE;
		}
		else {
			//fwrite(nlhrReply->pData,nlhrReply->dataLength,1,f);

			//id wo angefangen wird, die adresse "rauszuschneiden"
			char avatarid[] = "m_user_avatar_img_wrapper";
			char* pointer_av = avatarid;
			//ende des datenbuffers
			char* deathend = nlhrReply->pData + nlhrReply->dataLength;
			char* pointer = nlhrReply->pData;
			//status ob gefunden oder nich
			BOOL found = FALSE;

			while (pointer < deathend&&*pointer_av != 0) {
				if (*pointer_av == *pointer) {
					pointer_av++;
					if (pointer_av - avatarid > 4)
						found = TRUE;
				}
				else
					pointer_av = avatarid;

				pointer++;
			}
			//was gefunden, nun das bild raustrennen
			if (*pointer_av == 0) {
				char * pos = NULL;
				pos = strchr(pointer, '/');
				pos -= 5;
				pointer = pos;

				pos = strchr(pointer, ' ');
				if (pos) {
					pos--;
					*pos = 0;

					//analysieren, welchent typ das bild hat
					pos = strrchr(pointer, '.');
					if (pos) {
						char filename[512];
						mir_strcpy(filename, XFireGetFoldersPath("Avatar"));
						mir_strcat(filename, username);

						pos++;
						//gif?!?!
						if (*pos == 'g'&&
							*(pos + 1) == 'i'&&
							*(pos + 2) == 'f') {
							av->type = PA_FORMAT_GIF;
							mir_strcat(filename, ".gif");
						}
						else//dann kanns nur jpg sein
						{
							av->type = PA_FORMAT_JPEG;
							mir_strcat(filename, ".jpg");
						}

						//verusch das bild runterladen
						if (GetWWWContent2(pointer, filename, FALSE)) {
							strcpy_s(av->file, 256, filename); //setzte dateinamen
							status = TRUE; //avatarladen hat geklappt, cool :)
						}
					}
				}
			}
		}
		CallService(MS_NETLIB_FREEHTTPREQUESTSTRUCT, 0, (LPARAM)nlhrReply);
	}

	return status;
}

static INT_PTR GetIPPort(WPARAM hContact, LPARAM)
{
	if (db_get_w(hContact, protocolname, "Port", -1) == 0)
		return 0;

	DBVARIANT dbv;
	if (db_get_s(hContact, protocolname, "ServerIP", &dbv))
		return 0;

	char temp[XFIRE_MAX_STATIC_STRING_LEN];
	mir_snprintf(temp, "%s:%d", dbv.pszVal, db_get_w(hContact, protocolname, "Port", -1));
	db_free(&dbv);

	if (OpenClipboard(NULL)) {
		EmptyClipboard();

		HGLOBAL clipbuffer = GlobalAlloc(GMEM_DDESHARE, mir_strlen(temp) + 1);
		char *buffer = (char*)GlobalLock(clipbuffer);
		mir_strcpy(buffer, LPCSTR(temp));
		GlobalUnlock(clipbuffer);

		SetClipboardData(CF_TEXT, clipbuffer);
		CloseClipboard();
	}

	return 0;
}

static INT_PTR GetVIPPort(WPARAM hContact, LPARAM)
{
	if (db_get_w(hContact, protocolname, "VPort", -1) == 0)
		return 0;

	DBVARIANT dbv;
	if (db_get_s(hContact, protocolname, "VServerIP", &dbv))
		return 0;

	char temp[XFIRE_MAX_STATIC_STRING_LEN];
	mir_snprintf(temp, "%s:%d", dbv.pszVal, db_get_w(hContact, protocolname, "VPort", -1));
	db_free(&dbv);

	if (OpenClipboard(NULL)) {
		EmptyClipboard();

		HGLOBAL clipbuffer = GlobalAlloc(GMEM_DDESHARE, mir_strlen(temp) + 1);
		char *buffer = (char*)GlobalLock(clipbuffer);
		mir_strcpy(buffer, LPCSTR(temp));
		GlobalUnlock(clipbuffer);

		SetClipboardData(CF_TEXT, clipbuffer);
		CloseClipboard();
	}

	return 0;
}

static INT_PTR GotoProfile(WPARAM hContact, LPARAM)
{
	DBVARIANT dbv;
	if (db_get_s(hContact, protocolname, "Username", &dbv))
		return 0;

	char temp[64];
	mir_strcpy(temp, "http://xfire.com/profile/");
	strcat_s(temp, 64, dbv.pszVal);
	db_free(&dbv);

	Utils_OpenUrl(temp);
	return 0;
}

static INT_PTR GotoXFireClanSite(WPARAM hContact, LPARAM)
{
	DBVARIANT dbv;
	char temp[64] = "";

	int clanid = db_get_dw(hContact, protocolname, "Clan", -1);
	mir_snprintf(temp, "ClanUrl_%d", clanid);

	if (db_get_s(NULL, protocolname, temp, &dbv))
		return 0;

	mir_strcpy(temp, "http://xfire.com/clans/");
	strcat_s(temp, 64, dbv.pszVal);
	db_free(&dbv);

	Utils_OpenUrl(temp);
	return 0;
}

static INT_PTR GotoProfile2(WPARAM, LPARAM)
{
	DBVARIANT dbv;
	if (db_get_s(NULL, protocolname, "login", &dbv))
		return 0;

	char temp[64];
	mir_strcpy(temp, "http://xfire.com/profile/");
	strcat_s(temp, 64, dbv.pszVal);
	db_free(&dbv);

	Utils_OpenUrl(temp);
	return 0;
}

static INT_PTR GotoProfileAct(WPARAM, LPARAM)
{
	DBVARIANT dbv;
	char temp[64] = "";

	if (db_get_s(NULL, protocolname, "login", &dbv))
		return 0;

	mir_strcpy(temp, "http://www.xfire.com/?username=");
	strcat_s(temp, 64, dbv.pszVal);
	db_free(&dbv);

	Utils_OpenUrl(temp);
	return 0;
}

int RebuildContactMenu(WPARAM hContact, LPARAM)
{
	bool bEnabled = true, bEnabled2 = true;

	DBVARIANT dbv;
	if (db_get_s(hContact, protocolname, "ServerIP", &dbv))
		bEnabled = false;
	else
		db_free(&dbv);
	Menu_ShowItem(copyipport, bEnabled);

	//kopieren von voice port und ip nur erlauben, wenn verf�gbar
	bEnabled = true;
	if (db_get_s(hContact, protocolname, "VServerIP", &dbv))
		bEnabled = false;
	else
		db_free(&dbv);
	Menu_ShowItem(vipport, bEnabled);

	//clansite nur bei clanmembern anbieten
	Menu_ShowItem(gotoclansite, db_get_dw(hContact, protocolname, "Clan", 0) != 0);

	//NotOnList
	Menu_ShowItem(blockfriend, db_get_dw(hContact, "CList", "NotOnList", 0) != 0);

	//speichere gameid ab
	int gameid = db_get_w(hContact, protocolname, "GameId", 0);
	//spiel in xfirespieliste?
	bEnabled = bEnabled2 = true;
	if (!xgamelist.Gameinlist(gameid)) {
		//nein, dann start und join auf unsichbar schalten
		bEnabled = bEnabled2 = false;
	}
	else {
		//gameobject holen
		Xfire_game* game = xgamelist.getGamebyGameid(gameid);
		//hat das spiel netzwerkparameter?
		if (game) {
			if (game->m_networkparams) {
				//is beim buddy ein port hinterlegt, also spielt er im internet?
				if (!db_get_dw(hContact, protocolname, "Port", 0)) {
					//nein, dann join button auch ausblenden
					bEnabled = false;
				}
			}
			else bEnabled = false;
		}
		else bEnabled = false;
	}

	Menu_ShowItem(joingame, bEnabled);
	Menu_ShowItem(startthisgame, bEnabled2);

	//remove freind nur bei noramlen buddies
	Menu_ShowItem(removefriend, db_get_b(hContact, protocolname, "friendoffriend", 0) != 1);
	return 0;
}

//wird beim miranda start ausgef�hrt, l�dt spiele und startet gamedetection
#ifndef NO_PTHREAD
void *inigamedetectiont(void*)
#else
void inigamedetectiont(void*)
#endif
{
	Scan4Games(NULL);
#ifndef NO_PTHREAD
	return gamedetectiont(ptr);
#else
	gamedetectiont(NULL);
#endif

}

void SetXFireGameStatusMsg(Xfire_game* game)
{
	static char statusmsg[100] = "";

	//kein gameobject, dann abbrechen
	if (!game) return;

	if (!game->m_statusmsg)
		xgamelist.getIniValue(game->m_id, "XUSERStatusMsg", statusmsg, 100);
	else
		strncpy_s(statusmsg, game->m_statusmsg, _TRUNCATE);

	if (statusmsg[0] != 0)
		if (myClient != NULL)
			if (myClient->m_client->m_connected)
				myClient->Status(statusmsg);
}

#ifndef NO_PTHREAD
void *gamedetectiont(void*)
#else
void gamedetectiont(void*)
#endif
{
	DWORD ec; //exitcode der processid
	char temp[200];
	Xfire_game* currentgame = NULL;
	BOOL disabledsound = FALSE;
	BOOL disabledpopups = FALSE;

	//vaiable zum spielzeit messen
	time_t t1 = time(NULL);

	if (db_get_b(NULL, protocolname, "nogamedetect", 0))
#ifndef NO_PTHREAD
		return ptr;
#else
		return;
#endif

	DWORD lowpids = db_get_b(NULL, protocolname, "skiplowpid", 100);

	//XFireLog("XFire Gamedetectionthread started...","");

	while (1) {
		//Sleep(12000);
		//XFireLog("12 Sek warten...","");
		if (mySleep(12000, hGameDetection)) {
#ifndef NO_PTHREAD
			return ptr;
#else
			return;
#endif
		}

#ifndef NO_PTHREAD
		pthread_testcancel();
#else
		if (Miranda_Terminated())
			return;
#endif

		if (myClient != NULL)
			if (!myClient->m_client->m_connected) {
				//XFireLog("PID und TSPID resett...","");
				ts2pid = pid = 0;
				//voicechat internen status zur�cksetzen
				voicechat.resetCurrentvoicestatus();
			}
		/*
		else*/
			{
				//erstmal nach TS2 suchen
				//XFireLog("Teamspeak detection...","");
				if (db_get_b(NULL, protocolname, "ts2detection", 0)) {
					SendGameStatus2Packet *packet = new SendGameStatus2Packet();
					if (voicechat.checkVoicechat(packet)) {
						if (myClient != NULL) {
							XFireLog("Send voicechat infos...");
							myClient->m_client->send(packet);
						}
					}
					delete packet;
				}

				if (currentgame != NULL) {
					ec = 0;

					if (!xgamelist.isValidPid(pid)) {
						SendGameStatusPacket *packet = new SendGameStatusPacket();
						packet->gameid = 0;
						if (db_get_b(NULL, protocolname, "sendgamestatus", 1))
							if (myClient != NULL)
								myClient->m_client->send(packet);

						//spielzeit messen
						time_t t2 = time(NULL);
						time_t t3 = t2 - t1;
						tm * mytm = gmtime(&t3);

						//statusmsg von xfire zur�cksetzen
						if (currentgame->m_setstatusmsg) {
							if (myClient != NULL)
								if (myClient->m_client->m_connected)
									if (bpStatus == ID_STATUS_ONLINE)
										myClient->Status(statusmessage[0]);
									else if (bpStatus == ID_STATUS_AWAY)
										myClient->Status(statusmessage[1]);
						}

						mir_snprintf(temp, Translate("Last game: %s playtime: %.2d:%.2d:%.2d"), currentgame->m_name, mytm->tm_hour, mytm->tm_min, mytm->tm_sec);
						db_set_s(NULL, protocolname, "LastGame", temp);

						if (currentgame->m_noicqstatus != TRUE&&db_get_b(NULL, protocolname, "autosetstatusmsg", 0))
							SetOldStatusMsg();

						db_set_w(NULL, protocolname, "currentgame", 0);
						db_unset(NULL, protocolname, "currentgamename");

						//popup wieder aktivieren, menuservice funk aufrufen, nur wenn popups vorher abgestellt wurden
						if (disabledpopups)
							if (db_get_b(NULL, protocolname, "nopopups", 0)) {
								if (ServiceExists("Popup/EnableDisableMenuCommand"))
									CallService("Popup/EnableDisableMenuCommand", NULL, NULL);

								disabledpopups = FALSE;
							}
						//sound wieder aktivieren, nur wenn es vorher abgestellt wurde
						if (disabledsound)
							if (db_get_b(NULL, protocolname, "nosoundev", 0)) {
								db_set_b(NULL, "Skin", "UseSound", 1);
								disabledsound = FALSE;
							}

						//bug beseitigt, wenn spiel beendet, alte ip entfernen
						db_unset(NULL, protocolname, "ServerIP");

						pid = NULL;
						currentgame = NULL;
						xgamelist.SetGameStatus(FALSE);

						NotifyEventHooks(hookgamestart, 0, 0);

						delete packet;
					}
					else { //noch offen
						//XFireLog("Spiel noch offen...","");
						//nur nwspiele nach ip/port scannen
						if (db_get_b(NULL, protocolname, "ipportdetec", 0)) {
							if (currentgame->m_networkparams != NULL && currentgame->m_send_gameid > 0) {
								SendGameStatusPacket *packet = new SendGameStatusPacket();
								//verscueh serverip und port zu scannen

								XFireLog("IPPort detection...", "");
								if (GetServerIPPort(pid, myClient->m_client->m_localaddr, myClient->m_client->m_llocaladdr, &packet->ip[3], &packet->ip[2], &packet->ip[1], &packet->ip[0], &packet->port)) {

									if (packet->ip[3] != 0) {
										mir_snprintf(temp, "%d.%d.%d.%d:%d", (unsigned char)packet->ip[3], (unsigned char)packet->ip[2], (unsigned char)packet->ip[1], (unsigned char)packet->ip[0], packet->port);
										db_set_s(NULL, protocolname, "ServerIP", temp);
										XFireLog("Got IPPort: %s", temp);
									}
									else {
										db_unset(NULL, protocolname, "ServerIP");
										XFireLog("NO IPPort", "");
									}

									packet->gameid = currentgame->m_send_gameid;
									if (db_get_b(NULL, protocolname, "sendgamestatus", 1))
										if (myClient != NULL)
											myClient->m_client->send(packet);

									if (currentgame->m_noicqstatus != TRUE && db_get_b(NULL, protocolname, "autosetstatusmsg", 0))
										SetGameStatusMsg();
								}
								else XFireLog("GetServerIPPort failed", "");

								delete packet;
							}
						}
					}
				}
				else {
					//XFireLog("nach spiel suchen...","");
					//hardcoded game detection
					HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
					PROCESSENTRY32* processInfo = new PROCESSENTRY32;
					processInfo->dwSize = sizeof(PROCESSENTRY32);

					XFireLog("XFire Gamedetection - Suche laufende Spiele...");

					//gamelist blocken
					xgamelist.Block(TRUE);


					while (Process32Next(hSnapShot, processInfo) != FALSE && currentgame == NULL) {
						//�berspringe niedrige pids
						if (processInfo->th32ProcessID < lowpids)
							continue;

						Xfire_game* nextgame;
						while (xgamelist.getnextGame(&nextgame)) {
							if (nextgame->checkpath(processInfo)) {
								SendGameStatusPacket *packet = new SendGameStatusPacket();

								XFireLog("XFire Gamedetection - Spiel gefunden: %i", nextgame->m_id);

								if (myClient != NULL)
									if (myClient->m_client->m_connected) {
										currentgame = nextgame;
										pid = processInfo->th32ProcessID;
										db_set_w(NULL, protocolname, "currentgame", currentgame->m_id);
										db_set_s(NULL, protocolname, "currentgamename", currentgame->m_name);
										packet->gameid = currentgame->m_send_gameid;
										t1 = time(NULL);

										if (db_get_b(NULL, protocolname, "sendgamestatus", 1)) {
											XFireLog("XFire Gamedetection - Sendgame-ID: %i", currentgame->m_send_gameid);
											if (currentgame->m_send_gameid > 0) {
												XFireLog("XFire Gamedetection - Setzte Status f�r XFire");
												myClient->m_client->send(packet);
											}
										}

										xgamelist.SetGameStatus(TRUE);

										//eventhook triggern
										NotifyEventHooks(hookgamestart, 1, 0);

										//statusmsg f�r xfire setzen
										if (currentgame->m_setstatusmsg)
											SetXFireGameStatusMsg(currentgame);

										if (currentgame->m_noicqstatus != TRUE && db_get_b(NULL, protocolname, "autosetstatusmsg", 0)) {
											BackupStatusMsg();
											SetGameStatusMsg();
										}
										//popup abschalten, menuservice funk aufrufen
										if (db_get_b(NULL, protocolname, "nopopups", 0)) {
											if (ServiceExists("Popup/EnableDisableMenuCommand") && db_get_b(NULL, "Popup", "ModuleIsEnabled", 0) == 1) {
												disabledpopups = TRUE;
												CallService("Popup/EnableDisableMenuCommand", NULL, NULL);
											}
										}
										//sound abschalten
										if (db_get_b(NULL, protocolname, "nosoundev", 0) && db_get_b(NULL, "Skin", "UseSound", 0) == 1) {
											db_set_b(NULL, "Skin", "UseSound", 0);
											disabledsound = TRUE;
										}
									}

								delete packet;

								break;
							}
						}
					}
					CloseHandle(hSnapShot);

					//gamelist unblocken
					xgamelist.Block(FALSE);
				}
			}
	}
}

static INT_PTR ReScanMyGames(WPARAM, LPARAM)
{
	db_unset(NULL, protocolname, "foundgames");

	mir_forkthread(Scan4Games, NULL);
	return 0;
}

static INT_PTR CustomGameSetup(WPARAM, LPARAM)
{
	//DialogBox(hinstance,MAKEINTRESOURCE(IDD_GAMELIST),NULL,DlgAddGameProc);
	return 0;
}

void setBuddyStatusMsg(BuddyListEntry *entry)
{
	if (entry == NULL)
		return;

	if (IsContactMySelf(entry->m_username))
		return;

	if (entry->m_game) {
		ostringstream xstatus;
		DBVARIANT dbv;
		if (!db_get_s(entry->m_hcontact, protocolname, "RGame", &dbv)) {
			xstatus << dbv.pszVal << " ";
			db_free(&dbv);
		}

		if (!db_get_b(NULL, protocolname, "noipportinstatus", 0)) {
			if (!db_get_s(entry->m_hcontact, protocolname, "ServerName", &dbv)) {
				xstatus << dbv.pszVal;
				db_free(&dbv);
			}
			else if (!db_get_s(entry->m_hcontact, protocolname, "ServerIP", &dbv)) {
				xstatus << "(" << dbv.pszVal << ":" << db_get_w(entry->m_hcontact, protocolname, "Port", 0) << ")";
				db_free(&dbv);
			}
		}
		db_set_utf(entry->m_hcontact, protocolname, "XStatusMsg", xstatus.str().c_str());
	}
	else {
		//db_set_b(entry->m_hcontact, protocolname, "XStatusId", 1);
		db_unset(entry->m_hcontact, protocolname, "XStatusId");
		db_unset(entry->m_hcontact, protocolname, "XStatusName");
		db_unset(entry->m_hcontact, protocolname, "XStatusMsg");
	}

	string afk = entry->m_statusmsg.substr(0, 5);
	int status_id = (afk == "(AFK)" || afk == "(ABS)") ? ID_STATUS_AWAY : ID_STATUS_ONLINE;

	db_set_w(entry->m_hcontact, protocolname, "Status", status_id);

	if (!entry->m_statusmsg.empty())
		db_set_utf(entry->m_hcontact, "CList", "StatusMsg", entry->m_statusmsg.c_str());
	else
		db_unset(entry->m_hcontact, "CList", "StatusMsg");
}

MCONTACT handlingBuddys(BuddyListEntry *entry, int clan, char*group, BOOL dontscan)
{
	MCONTACT hContact;
	string game;

	if (entry == NULL)
		return NULL;

	//wenn der buddy ich selbst ist, dann ignorieren
	if (IsContactMySelf(entry->m_username))
		return NULL;

	if (entry->m_hcontact == NULL) {
		entry->m_hcontact = CList_FindContact(entry->m_userid);
		if (entry->m_hcontact && clan == -1) {
			db_set_w(entry->m_hcontact, protocolname, "Status", ID_STATUS_ONLINE);
			db_set_s(entry->m_hcontact, protocolname, "MirVer", "xfire");
		}
	}

	if (entry->m_hcontact == NULL) {
		XFireContact xfire_newc;
		xfire_newc.username = (char*)entry->m_username.c_str();
		xfire_newc.nick = (char*)entry->m_nick.c_str();
		xfire_newc.id = entry->m_userid;

		entry->m_hcontact = CList_AddContact(xfire_newc, TRUE, entry->isOnline() ? TRUE : FALSE, clan);
	}

	hContact = entry->m_hcontact;

	if (hContact != 0) {
		if (!entry->m_nick.empty() && db_get_b(NULL, protocolname, "shownicks", 1)) {
			db_set_utf(hContact, protocolname, "Nick", entry->m_nick.c_str());
		}
		else {
			db_set_s(hContact, protocolname, "Nick", entry->m_username.c_str());
		}

		if (!entry->isOnline()) {
			db_set_w(hContact, protocolname, "Status", ID_STATUS_OFFLINE);
			db_unset(hContact, protocolname, "XStatusMsg");
			db_unset(hContact, protocolname, "XStatusId");
			db_unset(hContact, protocolname, "XStatusName");
			db_unset(hContact, "CList", "StatusMsg");
			//db_set_utf(hContact, protocolname, "XStatusName", "");
			db_unset(hContact, protocolname, "ServerIP");
			db_unset(hContact, protocolname, "Port");
			db_unset(hContact, protocolname, "VServerIP");
			db_unset(hContact, protocolname, "VPort");
			db_unset(hContact, protocolname, "RVoice");
			db_unset(hContact, protocolname, "RGame");
			db_unset(hContact, protocolname, "GameId");
			db_unset(hContact, protocolname, "VoiceId");
			db_unset(hContact, protocolname, "GameInfo");
		}
		else if (entry->m_game > 0 || entry->m_game2 > 0) {
			char temp[XFIRE_MAX_STATIC_STRING_LEN] = "";
			char gname[255] = "";

			DummyXFireGame *gameob;

			if (mir_strlen(entry->m_gameinfo.c_str()) > 0)
				db_set_s(hContact, protocolname, "GameInfo", entry->m_gameinfo.c_str());

			//beim voicechat foglendes machn
			if (entry->m_game2 > 0) {
				gameob = (DummyXFireGame*)entry->m_game2Obj; //obj wo ip und port sind auslesen

				xgamelist.getGamename(entry->m_game2, gname, 255);

				db_set_s(hContact, protocolname, "RVoice", gname);

				if (gameob) {
					if ((unsigned char)gameob->m_ip[3] != 0) { // wenn ip, dann speichern
						mir_snprintf(temp, "%d.%d.%d.%d", (unsigned char)gameob->m_ip[3], (unsigned char)gameob->m_ip[2], (unsigned char)gameob->m_ip[1], (unsigned char)gameob->m_ip[0]);
						db_set_s(hContact, protocolname, "VServerIP", temp);
						db_set_w(hContact, protocolname, "VPort", gameob->m_port);
					}
					else {
						db_unset(hContact, protocolname, "VServerIP");
						db_unset(hContact, protocolname, "VPort");
					}
				}

				db_set_w(hContact, protocolname, "VoiceId", entry->m_game2);

				ExtraIcon_SetIcon(hExtraIcon2, hContact, xgamelist.iconmngr.getGameIconHandle(entry->m_game2));
			}
			else {
				db_unset(hContact, protocolname, "VServerIP");
				db_unset(hContact, protocolname, "VPort");
				db_unset(hContact, protocolname, "RVoice");
				db_unset(hContact, protocolname, "VoiceId");
				ExtraIcon_SetIcon(hExtraIcon2, hContact, INVALID_HANDLE_VALUE);
			}

			//beim game folgendes machen
			if (entry->m_game > 0) {
				HICON hicongame = xgamelist.iconmngr.getGameIcon(entry->m_game);

				xgamelist.getGamename(entry->m_game, gname, 255);

				db_set_s(hContact, protocolname, "RGame", gname);

				//beinhaltet ip und port
				gameob = (DummyXFireGame*)entry->m_gameObj;

				//popup, wenn jemand was spielt
				if (db_get_b(NULL, protocolname, "gamepopup", 0) == 1) {
					char szMsg[256] = "";
					mir_snprintf(szMsg, Translate("%s is playing %s."),
						//ist ein nick gesetzt?
						(entry->m_nick.length() == 0 ?
						//nein dann username
						entry->m_username.c_str() :
						//klar, dann nick nehmen
						entry->m_nick.c_str())
						, gname);

					if (gameob) {
						if ((unsigned char)gameob->m_ip[3] != 0) {
							mir_snprintf(szMsg, Translate("%s is playing %s on server %d.%d.%d.%d:%d."),
								//ist ein nick gesetzt?
								(entry->m_nick.length() == 0 ?
								//nein dann username
								entry->m_username.c_str() :
								//klar, dann nick nehmen
								entry->m_nick.c_str()),
								gname, (unsigned char)gameob->m_ip[3], (unsigned char)gameob->m_ip[2], (unsigned char)gameob->m_ip[1], (unsigned char)gameob->m_ip[0], (unsigned long)gameob->m_port);
						}
					}

					/*
						POPUP-Filter
						Nur Popups anzeigen die noch nicht angezeigt wurden
						*/
					if (entry->m_lastpopup == NULL) {
						//gr��e des popupstrings
						int size = mir_strlen(szMsg) + 1;
						//popup darstellen
						displayPopup(NULL, szMsg, PLUGIN_TITLE, 0, hicongame);
						//letzten popup definieren
						entry->m_lastpopup = new char[size];
						//string kopieren
						strcpy_s(entry->m_lastpopup, size, szMsg);
					}
					else {
						if (mir_strcmp(entry->m_lastpopup, szMsg) != 0) {
							delete[] entry->m_lastpopup;
							entry->m_lastpopup = NULL;

							//gr��e des popupstrings
							int size = mir_strlen(szMsg) + 1;
							//popup darstellen
							displayPopup(NULL, szMsg, PLUGIN_TITLE, 0, hicongame);
							//letzten popup definieren
							entry->m_lastpopup = new char[size];
							//string kopieren
							strcpy_s(entry->m_lastpopup, size, szMsg);
						}
					}
				}

				if (gameob) {
					if ((unsigned char)gameob->m_ip[3] != 0) {
						//ip und port in kontakt speichern
						mir_snprintf(temp, "%d.%d.%d.%d", (unsigned char)gameob->m_ip[3], (unsigned char)gameob->m_ip[2], (unsigned char)gameob->m_ip[1], (unsigned char)gameob->m_ip[0]);
						db_set_s(hContact, protocolname, "ServerIP", temp);
						db_set_w(hContact, protocolname, "Port", gameob->m_port);

						//lass das query arbeiten
						if (dontscan == FALSE)
							if (ServiceExists("GameServerQuery/Query") && db_get_b(NULL, protocolname, "gsqsupport", 0)) {
								GameServerQuery_query gsqq = { 0 };
								gsqq.port = gameob->m_port;
								gsqq.xfiregameid = entry->m_game;
								strncpy(gsqq.ip, temp, _countof(gsqq.ip) - 1);
								CallService("GameServerQuery/Query", (WPARAM)entry, (LPARAM)&gsqq);
							}
					}
					else {
						db_unset(hContact, protocolname, "ServerName");
						db_unset(hContact, protocolname, "ServerIP");
						db_unset(hContact, protocolname, "Port");
					}
				}

				ExtraIcon_SetIcon(hExtraIcon1, hContact, xgamelist.iconmngr.getGameIconHandle(entry->m_game));

				//db_unset(hContact, "CList", "StatusMsg");
				db_set_w(hContact, protocolname, "Status", ID_STATUS_ONLINE);
				db_set_utf(hContact, protocolname, "XStatusName", Translate("Playing"));
				setBuddyStatusMsg(entry);
				db_set_b(hContact, protocolname, "XStatusId", xgamelist.iconmngr.getGameIconId(entry->m_game) + 2);

				//buddy vorher ein spielgestartet, wenn nicht sound spielen?
				if (!db_get_w(hContact, protocolname, "GameId", 0))
					SkinPlaySound("xfirebstartgame");

				db_set_w(hContact, protocolname, "GameId", entry->m_game);
			}
			else {
				ExtraIcon_SetIcon(hExtraIcon1, hContact, INVALID_HANDLE_VALUE);
				db_unset(hContact, protocolname, "ServerIP");
				db_unset(hContact, protocolname, "Port");
				db_unset(hContact, protocolname, "XStatusMsg");
				db_unset(hContact, protocolname, "XStatusId");
				db_unset(hContact, protocolname, "XStatusName");
				db_unset(hContact, protocolname, "RGame");
				db_unset(hContact, protocolname, "GameId");
				setBuddyStatusMsg(entry);
			}
		}
		else if (!entry->m_statusmsg.empty()) {
			setBuddyStatusMsg(entry);

			ExtraIcon_SetIcon(hExtraIcon1, hContact, INVALID_HANDLE_VALUE);
			ExtraIcon_SetIcon(hExtraIcon2, hContact, INVALID_HANDLE_VALUE);

			// RM: test fix to remove xstatus when finished playing...
			db_unset(hContact, protocolname, "XStatusMsg");
			db_unset(hContact, protocolname, "XStatusId");
			db_unset(hContact, protocolname, "XStatusName");
			// ---

			db_unset(hContact, protocolname, "ServerIP");
			db_unset(hContact, protocolname, "Port");
			db_unset(hContact, protocolname, "VServerIP");
			db_unset(hContact, protocolname, "VPort");
			db_unset(hContact, protocolname, "RVoice");
			db_unset(hContact, protocolname, "RGame");
			db_unset(hContact, protocolname, "GameId");
			db_unset(hContact, protocolname, "VoiceId");
		}
		else {
			if (db_get_w(entry->m_hcontact, protocolname, "Status", -1) == ID_STATUS_OFFLINE) {
				if (db_get_b(NULL, protocolname, "noclanavatars", 0) == 1 && clan > 0)
					;
				else
					if (myClient) myClient->CheckAvatar(entry);
			}

			ExtraIcon_SetIcon(hExtraIcon1, hContact, INVALID_HANDLE_VALUE);
			ExtraIcon_SetIcon(hExtraIcon2, hContact, INVALID_HANDLE_VALUE);

			db_set_w(hContact, protocolname, "Status", ID_STATUS_ONLINE);
			db_set_s(entry->m_hcontact, protocolname, "MirVer", "xfire");
			if (clan > 0) db_set_dw(hContact, protocolname, "Clan", clan);
			//db_set_utf(hContact, "CList", "StatusMsg", "");
			db_unset(hContact, protocolname, "XStatusMsg");
			db_unset(hContact, protocolname, "XStatusId");
			db_unset(hContact, protocolname, "XStatusName");
			db_unset(hContact, "CList", "StatusMsg");
			db_unset(hContact, protocolname, "ServerIP");
			db_unset(hContact, protocolname, "Port");
			db_unset(hContact, protocolname, "VServerIP");
			db_unset(hContact, protocolname, "VPort");
			db_unset(hContact, protocolname, "RVoice");
			db_unset(hContact, protocolname, "RGame");
			db_unset(hContact, protocolname, "GameId");
			db_unset(hContact, protocolname, "VoiceId");
		}
	}
	if (group != NULL) {
		if (!db_get_b(NULL, protocolname, "noclangroups", 0)) {
			if (clan > 0) {
				int val = db_get_b(NULL, protocolname, "mainclangroup", 0);

				if (db_get_b(NULL, protocolname, "skipfriendsgroups", 0) == 0 ||
					(db_get_b(NULL, protocolname, "skipfriendsgroups", 0) == 1 &&
					db_get_b(entry->m_hcontact, protocolname, "isfriend", 0) == 0)
					) {
					if (val == 0) {
						db_set_s(entry->m_hcontact, "CList", "Group", group);
					}
					else {
						char temp[256];
						DBVARIANT dbv;
						mir_snprintf(temp, "%d", val - 1);
						db_get_s(NULL, "CListGroups", temp, &dbv);
						if (dbv.pszVal != NULL) {
							mir_snprintf(temp, "%s\\%s", &dbv.pszVal[1], group);
							db_set_s(entry->m_hcontact, "CList", "Group", temp);
							db_free(&dbv);
						}
					}
				}
			}
			else if (clan == -1)//hauptgruppe f�r fof
			{
				int val = db_get_b(NULL, protocolname, "fofgroup", 0);

				if (val == 0) {
					db_set_s(entry->m_hcontact, "CList", "Group", group);
				}
				else {
					char temp[256];
					DBVARIANT dbv;
					mir_snprintf(temp, "%d", val - 1);
					db_get_s(NULL, "CListGroups", temp, &dbv);
					if (dbv.pszVal != NULL) {
						mir_snprintf(temp, "%s\\%s", &dbv.pszVal[1], group);
						db_set_s(entry->m_hcontact, "CList", "Group", temp);
						db_free(&dbv);
					}
				}
			}
		}
	}
	else {
		db_set_b(entry->m_hcontact, protocolname, "isfriend", 1);
	}

	return hContact;
}

INT_PTR AddtoList(WPARAM, LPARAM lParam)
{
	CCSDATA* ccs = (CCSDATA*)lParam;

	if (ccs->hContact) {
		DBVARIANT dbv2;
		if (!db_get(ccs->hContact, protocolname, "Username", &dbv2)) {

			if (myClient != NULL)
				if (myClient->m_client->m_connected) {
					SendAcceptInvitationPacket accept;
					accept.name = dbv2.pszVal;
					myClient->m_client->send(&accept);
				}

			//tempor�ren buddy entfernen, da eh ein neues packet kommt
			db_set_b(ccs->hContact, protocolname, "DontSendDenyPacket", 1);
			CallService(MS_DB_CONTACT_DELETE, (WPARAM)ccs->hContact, 0);
		}
	}
	return 0;
}


static void __cdecl AckBasicSearch(void * pszNick)
{
	if (pszNick != NULL) {
		if (myClient != NULL)
			if (myClient->m_client->m_connected) {
				SearchBuddy search;
				search.searchfor((char*)pszNick);
				myClient->m_client->send(&search);
			}
	}
}

INT_PTR BasicSearch(WPARAM, LPARAM lParam)
{
	static char buf[50];
	if (lParam) {
		if (myClient != NULL)
			if (myClient->m_client->m_connected) {
				mir_strncpy(buf, (const char *)lParam, 49);
				mir_forkthread(AckBasicSearch, &buf);
				return 1;
			}
	}

	return 0;
}



INT_PTR SearchAddtoList(WPARAM wParam, LPARAM lParam)
{
	PROTOSEARCHRESULT *psr = (PROTOSEARCHRESULT*)lParam;

	if (!psr || psr->cbSize != sizeof(PROTOSEARCHRESULT))
		return 0;

	if ((int)wParam == 0)
		if (myClient != NULL)
			if (myClient->m_client->m_connected) {
				InviteBuddyPacket invite;
				invite.addInviteName(std::string(_T2A(psr->nick.t)), Translate("Add me to your friend list."));
				myClient->m_client->send(&invite);
			}

	return -1;
}


void CreateGroup(char*grpn, char*field)
{
	DBVARIANT dbv;
	char grp[255];

	int val = db_get_b(NULL, protocolname, field, 0);

	if (val == 0)
		strcpy_s(grp, _countof(grp), grpn);//((char*)clan->name[i].c_str());
	else {
		char temp[255];
		mir_snprintf(temp, "%d", val - 1);
		if (!db_get_s(NULL, "CListGroups", temp, &dbv)) {
			mir_snprintf(grp, "%s\\%s", &dbv.pszVal[1], grpn);
			db_free(&dbv);
		}
		else { //gruppe existiert nciht mehr, auf root alles legen
			strcpy_s(grp, _countof(grp), grpn);
			db_set_b(NULL, protocolname, field, 0);
		}
	}


	char group[255] = "";
	char temp[10];
	int i = 0;
	for (i = 0;; i++) {
		mir_snprintf(temp, "%d", i);
		if (db_get_s(NULL, "CListGroups", temp, &dbv)) {
			i--;
			break;
		}
		if (dbv.pszVal[0] != '\0' && !mir_strcmp(dbv.pszVal + 1, (char*)grp)) {
			db_free(&dbv);
			return;
		}
		db_free(&dbv);
	}
	strcpy_s(group, 255, "D");
	strcat_s(group, 255, grp);
	group[0] = 1 | GROUPF_EXPANDED;
	mir_snprintf(temp, "%d", i + 1);
	db_set_s(NULL, "CListGroups", temp, group);
	CallServiceSync(MS_CLUI_GROUPADDED, i + 1, 0);
}


INT_PTR SetAwayMsg(WPARAM wParam, LPARAM lParam)
{
	mir_cslock lck(modeMsgsMutex);
	if ((char*)lParam == NULL) {
		if (wParam == ID_STATUS_ONLINE)
			mir_strcpy(statusmessage[0], "");
		else if (wParam != ID_STATUS_OFFLINE)
			mir_snprintf(statusmessage[1], "(AFK) %s", Translate("Away from Keyboard"));
	}
	else {
		if (wParam == ID_STATUS_ONLINE)
			mir_strcpy(statusmessage[0], (char*)lParam);
		else if (wParam != ID_STATUS_OFFLINE) {
			if (db_get_b(NULL, protocolname, "nocustomaway", 0) == 0 && mir_strlen((char*)lParam) > 0)
				mir_snprintf(statusmessage[1], "(AFK) %s", (char*)lParam);
			else
				mir_snprintf(statusmessage[1], "(AFK) %s", Translate("Away from Keyboard"));
		}
	}

	if (myClient != NULL) {
		if (myClient->m_client->m_connected) {
			if (bpStatus == ID_STATUS_ONLINE)
				myClient->Status(statusmessage[0]);
			else if (wParam != ID_STATUS_ONLINE&&wParam != ID_STATUS_OFFLINE)
				myClient->Status(statusmessage[1]);
		}
	}
	return 0;
}

INT_PTR SetNickName(WPARAM newnick, LPARAM)
{
	if (newnick == NULL)
		return FALSE;

	if (myClient != NULL)
		if (myClient->m_client->m_connected) {
			myClient->setNick((char*)newnick);
			db_set_s(NULL, protocolname, "Nick", (char*)newnick);
			return TRUE;
		}
	return FALSE;
}

//sendet neue preferencen zu xfire
INT_PTR SendPrefs(WPARAM, LPARAM)
{
	if (myClient != NULL)
		if (myClient->m_client->m_connected) {
			PrefsPacket prefs;
			for (int i = 0; i < XFIRE_RECVPREFSPACKET_MAXCONFIGS; i++)
				prefs.config[i] = xfireconfig[i];

			myClient->m_client->send(&prefs);
			return TRUE;
		}
	return FALSE;
}

int ContactDeleted(WPARAM hContact, LPARAM)
{
	if (!db_get_b(hContact, protocolname, "DontSendDenyPacket", 0)) {
		if (db_get_b(hContact, "CList", "NotOnList", 0)) {
			if (myClient != NULL) {
				if (myClient->m_client->m_connected) {
					DBVARIANT dbv2;
					if (!db_get(hContact, protocolname, "Username", &dbv2)) {
						SendDenyInvitationPacket deny;
						deny.name = dbv2.pszVal;
						myClient->m_client->send(&deny);
					}
				}
			}
		}
	}
	return 0;
}

INT_PTR StartGame(WPARAM, LPARAM, LPARAM fParam)
{
	//gamelist blocken
	xgamelist.Block(TRUE);

	Xfire_game*game = xgamelist.getGamebyGameid(fParam);

	//starte das spiel
	if (game)
		game->start_game();

	//gamelist blocken
	xgamelist.Block(FALSE);


	return 0;
}

INT_PTR RemoveFriend(WPARAM hContact, LPARAM)
{
	char temp[256];
	DBVARIANT dbv;
	if (!db_get_s(hContact, protocolname, "Username", &dbv)) {
		mir_snprintf(temp, Translate("Do you really want to delete your friend %s?"), dbv.pszVal);
		if (MessageBoxA(NULL, temp, Translate("Confirm Delete"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
			if (myClient != NULL) {
				if (myClient->m_client->m_connected) {
					SendRemoveBuddyPacket removeBuddy;
					removeBuddy.userid = db_get_dw(hContact, protocolname, "UserId", 0);
					if (removeBuddy.userid != 0)
						myClient->m_client->send(&removeBuddy);
				}
			}
		}
		db_free(&dbv);
	}
	return 0;
}

INT_PTR BlockFriend(WPARAM hContact, LPARAM)
{
	DBVARIANT dbv;
	if (!db_get_s(hContact, protocolname, "Username", &dbv)) {
		if (MessageBox(NULL, TranslateT("Block this user from ever contacting you again?"), TranslateT("Block Confirmation"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
			if (myClient != NULL) {
				if (myClient->m_client->m_connected) {
					db_set_b(NULL, "XFireBlock", dbv.pszVal, 1);

					SendDenyInvitationPacket deny;
					deny.name = dbv.pszVal;
					myClient->m_client->send(&deny);
				}
			}
		}
		CallService(MS_DB_CONTACT_DELETE, hContact, 1);
		db_free(&dbv);
	}
	return 0;
}

INT_PTR StartThisGame(WPARAM wParam, LPARAM)
{
	//gamelist blocken
	xgamelist.Block(TRUE);

	//hole die gameid des spiels
	int id = db_get_w(wParam, protocolname, "GameId", 0);

	//hole passendes spielobjekt
	Xfire_game*game = xgamelist.getGamebyGameid(id);

	//starte das spiel
	if (game)
		game->start_game();

	//gamelist blocken
	xgamelist.Block(FALSE);

	return 0;
}

INT_PTR JoinGame(WPARAM hContact, LPARAM)
{
	//gamelist blocken
	xgamelist.Block(TRUE);

	//hole die gameid des spiels
	int id = db_get_w(hContact, protocolname, "GameId", 0);

	//hole passendes spielobjekt
	Xfire_game *game = xgamelist.getGamebyGameid(id);
	if (game) {
		DBVARIANT dbv; //dbv.pszVal
		int port = db_get_w(hContact, protocolname, "Port", 0);
		if (!db_get_s(hContact, protocolname, "ServerIP", &dbv)) {
			//starte spiel mit netzwerk parametern
			game->start_game(dbv.pszVal, port);
			db_free(&dbv);
		}
	}

	//gamelist unblocken
	xgamelist.Block(FALSE);
	return 0;
}

int doneQuery(WPARAM wParam, LPARAM lParam)
{
	char temp[256];
	BuddyListEntry* bud = (BuddyListEntry*)wParam;
	gServerstats* gameinfo = (gServerstats*)lParam;
	db_set_s(bud->m_hcontact, protocolname, "ServerName", gameinfo->name);
	db_set_s(bud->m_hcontact, protocolname, "GameType", gameinfo->gametype);
	db_set_s(bud->m_hcontact, protocolname, "Map", gameinfo->map);
	mir_snprintf(temp, "(%d/%d)", gameinfo->players, gameinfo->maxplayers);
	db_set_s(bud->m_hcontact, protocolname, "Players", temp);
	db_set_b(bud->m_hcontact, protocolname, "Passworded", gameinfo->password);

	if (myClient != NULL)
		handlingBuddys(bud, 0, NULL, TRUE);

	return 0;
}

static INT_PTR SetNickDlg(WPARAM, LPARAM)
{
	return ShowSetNick();
}

INT_PTR GetAvatarInfo(WPARAM, LPARAM lParam)
{
	PROTO_AVATAR_INFORMATION* pai = (PROTO_AVATAR_INFORMATION*)lParam;

	if (db_get_b(NULL, protocolname, "noavatars", -1) != 0)
		return GAIR_NOAVATAR;

	pai->format = db_get_w(pai->hContact, "ContactPhoto", "Format", 0);
	if (pai->format == 0)
		return GAIR_NOAVATAR;

	ptrW pwszPath(db_get_wsa(pai->hContact, "ContactPhoto", "File"));
	if (pwszPath == NULL)
		return GAIR_NOAVATAR;

	wcsncpy_s(pai->filename, pwszPath, _TRUNCATE);
	return GAIR_SUCCESS;
}
