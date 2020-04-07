/*
Plugin of Miranda IM for communicating with users of the MSN Messenger protocol.

Copyright (c) 2012-2020 Miranda NG team
Copyright (c) 2009-2012 Boris Krasnovskiy.

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

#ifndef _MSN_PROTO_H_
#define _MSN_PROTO_H_

#include <m_protoint.h>

struct CMsnProto : public PROTO<CMsnProto>
{
	CMsnProto(const char*, const wchar_t*);
	~CMsnProto();

	//====================================================================================
	// PROTO_INTERFACE
	//====================================================================================

	MCONTACT AddToList(int flags, PROTOSEARCHRESULT* psr) override;
	MCONTACT AddToListByEvent(int flags, int iContact, MEVENT hDbEvent) override;

	int      Authorize(MEVENT hDbEvent) override;
	int      AuthDeny(MEVENT hDbEvent, const wchar_t* szReason) override;
	int      AuthRecv(MCONTACT hContact, PROTORECVEVENT*) override;
	int      AuthRequest(MCONTACT hContact, const wchar_t* szMessage) override;

	HANDLE   FileAllow(MCONTACT hContact, HANDLE hTransfer, const wchar_t* szPath) override;
	int      FileCancel(MCONTACT hContact, HANDLE hTransfer) override;
	int      FileDeny(MCONTACT hContact, HANDLE hTransfer, const wchar_t* szReason) override;
	int      FileResume(HANDLE hTransfer, int* action, const wchar_t** szFilename) override;

	INT_PTR  GetCaps(int type, MCONTACT hContact = NULL) override;
	int      GetInfo(MCONTACT hContact, int infoType) override;

	HANDLE   SearchBasic(const wchar_t* id) override;
	HANDLE   SearchByEmail(const wchar_t* email) override;

	MEVENT   RecvMsg(MCONTACT hContact, PROTORECVEVENT*) override;
	int      RecvContacts(MCONTACT hContact, PROTORECVEVENT*) override;

	int      SendMsg(MCONTACT hContact, int flags, const char* msg) override;
	int      SendContacts(MCONTACT hContact, int flags, int nContacts, MCONTACT *hContactsList) override;

	int      SetApparentMode(MCONTACT hContact, int mode) override;
	int      SetStatus(int iNewStatus) override;

	HANDLE   GetAwayMsg(MCONTACT hContact) override;
	int      SetAwayMsg(int m_iStatus, const wchar_t* msg) override;

	int      UserIsTyping(MCONTACT hContact, int type) override;

	void     OnBuildProtoMenu(void) override;
	void     OnContactDeleted(MCONTACT) override;
	void     OnErase() override;
	void     OnModulesLoaded() override;
	void     OnShutdown() override;

	//====| Services |====================================================================

	INT_PTR  __cdecl SvcCreateAccMgrUI(WPARAM wParam, LPARAM lParam);

	INT_PTR  __cdecl GetAvatarInfo(WPARAM wParam, LPARAM lParam);
	INT_PTR  __cdecl GetMyAwayMsg(WPARAM wParam, LPARAM lParam);

	INT_PTR  __cdecl GetAvatar(WPARAM wParam, LPARAM lParam);
	INT_PTR  __cdecl SetAvatar(WPARAM wParam, LPARAM lParam);
	INT_PTR  __cdecl GetAvatarCaps(WPARAM wParam, LPARAM lParam);

	INT_PTR  __cdecl SetNickName(WPARAM wParam, LPARAM lParam);
	INT_PTR  __cdecl SendNudge(WPARAM wParam, LPARAM lParam);

	INT_PTR  __cdecl GetUnreadEmailCount(WPARAM wParam, LPARAM lParam);

	INT_PTR  __cdecl OnLeaveChat(WPARAM wParam, LPARAM lParam);

	//====| Events |======================================================================

	int  __cdecl OnIdleChanged(WPARAM wParam, LPARAM lParam);
	int  __cdecl OnGroupChange(WPARAM wParam, LPARAM lParam);
	int  __cdecl OnOptionsInit(WPARAM wParam, LPARAM lParam);
	int  __cdecl OnPrebuildContactMenu(WPARAM wParam, LPARAM lParam);
	int  __cdecl OnContactDoubleClicked(WPARAM wParam, LPARAM lParam);
	int  __cdecl OnDbSettingChanged(WPARAM wParam, LPARAM lParam);
	int  __cdecl OnWindowPopup(WPARAM wParam, LPARAM lParam);

	//====| Data |========================================================================

	// Security Tokens
	char *pAuthToken, *tAuthToken;
	char *oimSendToken;
	char *authSecretToken;
	OAuthToken authStrToken, authContactToken, authStorageToken, authSSLToken, authSkypeComToken;
	SkypeToken authSkypeToken;
	char *hotSecretToken, *hotAuthToken;
	char *authUser, *authUIC, *authCookies, *authRefreshToken;
	bool bAskingForAuth, bPassportAuth;
	int  authMethod;
	bool bSentBND, bIgnoreATH;

	char *abCacheKey, *sharingCacheKey, *storageCacheKey;

	mir_cs m_csLists;
	OBJLIST<MsnContact> m_arContacts;

	LIST<ServerGroupItem> m_arGroups;

	mir_cs m_csThreads;
	OBJLIST<ThreadData> m_arThreads;
	LIST<GCThreadData> m_arGCThreads;

	mir_cs m_csSessions;

	mir_cs csMsgQueue;
	int msgQueueSeq;
	OBJLIST<MsgQueueEntry> lsMessageQueue;

	mir_cs csAvatarQueue;
	LIST<AvatarQueueEntry> lsAvatarQueue;
	HANDLE hevAvatarQueue;

	LONG m_chatID;

	int msnPingTimeout;
	unsigned __int64  lastMsgId;
	HANDLE hKeepAliveThreadEvt;

	char* msnModeMsgs[MSN_NUM_MODES];

	LISTENINGTOINFO     msnCurrentMedia;
	MYOPTIONS			MyOptions;
	MyConnectionType	MyConnection;

	ThreadData*	msnNsThread;
	bool        msnLoggedIn;

	char*       msnExternalIP;
	char*       msnRegistration;
	char*       msnPreviousUUX;
	char*       msnLastStatusMsg;

	char*       mailsoundname;
	char*       alertsoundname;

	unsigned    langpref;
	bool        emailEnabled;
	unsigned    abchMigrated;
	unsigned    myFlags;

	unsigned    msnOtherContactsBlocked;
	int			mUnreadMessages;
	int			mUnreadJunkEmails;
	clock_t		mHttpsTS;
	clock_t		mStatusMsgTS;

	HANDLE		msnSearchId;
	HNETLIBCONN hHttpsConnection;
	HANDLE		hMSNNudge;
	HANDLE      hPopupError, hPopupHotmail, hPopupNotify;

	HANDLE		hCustomSmileyFolder;
	bool        InitCstFldRan;
	bool        isConnectSuccess;
	bool        isIdle;

	void        InitCustomFolders(void);

	char*       getSslResult(char** parUrl, const char* parAuthInfo, const char* hdrs, unsigned& status);
	bool        getMyAvatarFile(char *url, wchar_t *fname);

	void        MSN_GoOffline(void);
	void        MSN_GetCustomSmileyFileName(MCONTACT hContact, wchar_t* pszDest, size_t cbLen, const char* SmileyName, int Type);

	const char*	MirandaStatusToMSN(int status);
	WORD        MSNStatusToMiranda(const char *status);
	char**		GetStatusMsgLoc(int status);

	void        MSN_SendStatusMessage(const char* msg);
	void        MSN_SetServerStatus(int newStatus);
	void        MSN_FetchRecentMessages(time_t since = 0);
	void        MSN_StartStopTyping(GCThreadData* info, bool start);
	void        MSN_SendTyping(ThreadData* info, const char* email, int netId, bool bTyping);

	void        MSN_ReceiveMessage(ThreadData* info, char* cmdString, char* params);
	int			MSN_HandleCommands(ThreadData* info, char* cmdString);
	int			MSN_HandleErrors(ThreadData* info, char* cmdString);
	void        MSN_ProcessNotificationMessage(char* buf, size_t bufLen);
	void        MSN_ProcessStatusMessage(ezxml_t xmli, const char* wlid);
	void        MSN_ProcessNLN(const char *userStatus, const char *wlid, char *userNick, const char *objid, char *cmdstring);
	void        MSN_ProcessYFind(char* buf, size_t len);
	void        MSN_ProcessURIObject(MCONTACT hContact, ezxml_t xmli);
	void        MSN_SetMirVer(MCONTACT hContact, MsnPlace *place);

	void        LoadOptions(void);

	void        InitPopups(void);
	void        MSN_ShowPopup(const wchar_t* nickname, const wchar_t* msg, int flags, const char* url);
	void        MSN_ShowPopup(const MCONTACT hContact, const wchar_t* msg, int flags);
	void        MSN_ShowError(const char* msgtext, ...);
	void        MSN_SendNicknameUtf(const char* nickname);

	typedef struct { wchar_t *szName; const char *szMimeType; unsigned char *data; size_t dataSize; } StoreAvatarData;
	void __cdecl msn_storeAvatarThread(void* arg);

	void __cdecl msn_storeProfileThread(void*);

	/////////////////////////////////////////////////////////////////////////////////////////
	// MSN menus

	HGENMENU menuItemsMain[4];

	void MSN_EnableMenuItems(bool parEnable);
	void MsnInvokeMyURL(bool ismail, const char* url);

	INT_PTR __cdecl MsnBlockCommand(WPARAM wParam, LPARAM lParam);
	INT_PTR __cdecl MsnGotoInbox(WPARAM, LPARAM);
	INT_PTR __cdecl MsnSendHotmail(WPARAM wParam, LPARAM);
	INT_PTR __cdecl MsnEditProfile(WPARAM, LPARAM);
	INT_PTR __cdecl MsnInviteCommand(WPARAM wParam, LPARAM lParam);
	INT_PTR __cdecl MsnViewProfile(WPARAM wParam, LPARAM lParam);
	INT_PTR __cdecl MsnSetupAlerts(WPARAM wParam, LPARAM lParam);

	/////////////////////////////////////////////////////////////////////////////////////////
	// MSN thread functions

	void __cdecl msn_keepAliveThread(void* arg);
	void __cdecl msn_loginThread(void* arg);
	void __cdecl msn_IEAuthThread(void* arg);
	void __cdecl msn_refreshOAuthThread(void*);
	void __cdecl MSNServerThread(void* arg);

	void __cdecl MsnFileAckThread(void* arg);
	void __cdecl MsnSearchAckThread(void* arg);

	void __cdecl MsnGetAwayMsgThread(void* arg);

	/////////////////////////////////////////////////////////////////////////////////////////
	// MSN thread support

	void         Threads_Uninit(void);
	void         MSN_CloseConnections(void);
	ThreadData*  MSN_GetThreadByConnection(HANDLE hConn);
	GCThreadData* MSN_GetThreadByChatId(const wchar_t* chatId);

	void __cdecl ThreadStub(void* arg);

	/////////////////////////////////////////////////////////////////////////////////////////
	// MSN message reassembly support

	OBJLIST<chunkedmsg> msgCache;

	int    addCachedMsg(const char* id, const char* msg, const size_t offset, const size_t portion, const size_t totsz, const bool bychunk);
	size_t getCachedMsgSize(const char* id);
	bool   getCachedMsg(const int idx, char*& msg, size_t& size);
	bool   getCachedMsg(const char* id, char*& msg, size_t& size);
	void   clearCachedMsg(int idx = -1);
	void   CachedMsg_Uninit(void);

	/////////////////////////////////////////////////////////////////////////////////////////
	//	MSN Chat support

	int  MSN_ChatInit(GCThreadData *info, const char *pszID, const char *pszTopic);
	void MSN_ChatStart(ezxml_t xmli);
	void MSN_KillChatSession(const wchar_t* id);
	void MSN_Kickuser(GCHOOK *gch);
	void MSN_Promoteuser(GCHOOK *gch, const char *pszRole);
	const wchar_t* MSN_GCGetRole(GCThreadData* thread, const char *pszWLID);
	void MSN_GCProcessThreadActivity(ezxml_t xmli, const wchar_t *mChatID);
	void MSN_GCAddMessage(wchar_t *mChatID, MCONTACT hContact, char *email, time_t ts, bool sentMsg, char *msgBody);
	void MSN_GCRefreshThreadsInfo(void);

	MCONTACT MSN_GetChatInernalHandle(MCONTACT hContact);

	int __cdecl MSN_GCEventHook(WPARAM wParam, LPARAM lParam);
	int __cdecl MSN_GCMenuHook(WPARAM wParam, LPARAM lParam);

	/////////////////////////////////////////////////////////////////////////////////////////
	//	MSN contact list

	int      Lists_Add(int list, int netId, const char* email, MCONTACT hContact = NULL, const char* nick = nullptr, const char* invite = nullptr);
	bool     Lists_IsInList(int list, const char* email);
	int      Lists_GetMask(const char* email);
	int      Lists_GetNetId(const char* email);
	void     Lists_Remove(int list, const char* email);
	void     Lists_Populate(void);
	void     Lists_Wipe(void);

	MsnContact* Lists_Get(const char* email);
	MsnContact* Lists_Get(MCONTACT hContact);
	MsnContact* Lists_GetNext(int& i);

	MsnPlace* Lists_GetPlace(const char* wlid);
	MsnPlace* Lists_GetPlace(const char* szEmail, const char *szInst);
	MsnPlace* Lists_AddPlace(const char* email, const char* id, unsigned cap1, unsigned cap2);

	void     Lists_Uninit(void);

	void     AddDelUserContList(const char* email, const int list, const int netId, const bool del);

	void     MSN_CreateContList(void);
	void     MSN_CleanupLists(void);
	bool     MSN_RefreshContactList(void);

	bool     MSN_IsMyContact(MCONTACT hContact);
	bool     MSN_IsMeByContact(MCONTACT hContact, char* szEmail = nullptr);
	bool     MSN_AddUser(MCONTACT hContact, const char* email, int netId, int flags, const char *msg = nullptr);
	void     MSN_AddAuthRequest(const char *email, const char *nick, const char *reason);
	void     MSN_SetContactDb(MCONTACT hContact, const char *szEmail);
	MCONTACT MSN_HContactFromEmail(const char* msnEmail, const char* msnNick = nullptr, bool addIfNeeded = false, bool temporary = false);
	MCONTACT MSN_HContactFromChatID(const char* wlid);
	MCONTACT AddToListByEmail(const char *email, const char *nick, DWORD flags);

	/////////////////////////////////////////////////////////////////////////////////////////
	//	MSN server groups

	void     MSN_AddGroup(const char* pName, const char* pId, bool init);
	void     MSN_DeleteGroup(const char* pId);
	void     MSN_FreeGroups(void);
	LPCSTR   MSN_GetGroupById(const char* pId);
	LPCSTR   MSN_GetGroupByName(const char* pName);
	void     MSN_SetGroupName(const char* pId, const char* pName);

	void     MSN_MoveContactToGroup(MCONTACT hContact, const char* grpName);
	void     MSN_RenameServerGroup(LPCSTR szId, const char* newName);
	void     MSN_DeleteServerGroup(LPCSTR szId);
	void     MSN_RemoveEmptyGroups(void);
	void     MSN_SyncContactToServerGroup(MCONTACT hContact, const char* szContId, ezxml_t cgrp);
	void     MSN_UploadServerGroups(char* group);

	/////////////////////////////////////////////////////////////////////////////////////////
	//	MSN Authentication

	int       MSN_GetPassportAuth(void);
	int       MSN_SkypeAuth(const char *pszNonce, char *pszUIC);
	void      LoadAuthTokensDB(void);
	void      SaveAuthTokensDB(void);
	bool	    parseLoginPage(char *pszHTML, NETLIBHTTPREQUEST *nlhr, CMStringA *post);
	int       LoginSkypeOAuth(const char *pRefreshToken);
	bool      RefreshOAuth(const char *pszRefreshToken, const char *pszService, CMStringA *pszAccessToken, CMStringA *pszOutRefreshToken, time_t *ptExpires);
	int       MSN_AuthOAuth(void);
	int       MSN_RefreshOAuthTokens(bool bJustCheck);
	void      MSN_SendATH(ThreadData *info);
	CMStringA HotmailLogin(const char *url);
	void	    FreeAuthTokens(void);
	int       GetMyNetID(void);
	LPCSTR    GetMyUsername(int netId);

	/////////////////////////////////////////////////////////////////////////////////////////
	//	MSN avatars support

	void   AvatarQueue_Init(void);
	void   AvatarQueue_Uninit(void);

	void   MSN_GetAvatarFileName(MCONTACT hContact, wchar_t* pszDest, size_t cbLen, const wchar_t *ext);
	int    MSN_SetMyAvatar(const wchar_t* szFname, void* pData, size_t cbLen);

	void   __cdecl MSN_AvatarsThread(void*);

	void   pushAvatarRequest(MCONTACT hContact, LPCSTR pszUrl);
	bool   loadHttpAvatar(AvatarQueueEntry *p);

	/////////////////////////////////////////////////////////////////////////////////////////
	//	MSN Mail & Offline messaging support

	bool nickChg;

	void getMetaData(void);
	void getOIMs(ezxml_t xmli);
	ezxml_t oimRecvHdr(const char* service, ezxml_t& tbdy, char*& httphdr);

	void processMailData(char* mailData);
	void sttNotificationMessage(char* msgBody, bool isInitial);
	void displayEmailCount(MCONTACT hContact);

	/////////////////////////////////////////////////////////////////////////////////////////
	//	SKYPE JSON Address Book
	bool MSN_SKYABRefreshClist(void);
	bool MSN_SKYABBlockContact(const char *wlid, const char *pszAction);
	bool MSN_SKYABAuthRq(const char *wlid, const char *pszGreeting);
	bool MSN_SKYABAuthRsp(const char *wlid, const char *pszAction);
	bool MSN_SKYABDeleteContact(const char *wlid);
	bool MSN_SKYABSearch(const char *keyWord, HANDLE hSearch);
	bool MSN_SKYABGetProfiles(const char *pszPOST);
	bool MSN_SKYABGetProfile(const char *wlid);

	bool APISkypeComRequest(NETLIBHTTPREQUEST *nlhr, NETLIBHTTPHEADER *headers);

	/////////////////////////////////////////////////////////////////////////////////////////
	//	MSN SOAP Address Book

	bool MSN_SharingFindMembership(bool deltas = false, bool allowRecurse = true);
	bool MSN_SharingAddDelMember(const char* szEmail, const int listId, const int netId, const char* szMethod, bool allowRecurse = true);
	bool MSN_SharingMyProfile(bool allowRecurse = true);
	bool MSN_ABAdd(bool allowRecurse = true);
	bool MSN_ABFind(const char* szMethod, const char* szGuid, bool deltas = false, bool allowRecurse = true);
	bool MSN_ABAddDelContactGroup(const char* szCntId, const char* szGrpId, const char* szMethod, bool allowRecurse = true);
	void MSN_ABAddGroup(const char* szGrpName, bool allowRecurse = true);
	void MSN_ABRenameGroup(const char* szGrpName, const char* szGrpId, bool allowRecurse = true);
	void MSN_ABUpdateNick(const char* szNick, const char* szCntId);
	void MSN_ABUpdateAttr(const char* szCntId, const char* szAttr, const char* szValue, bool allowRecurse = true);
	bool MSN_ABUpdateProperty(const char* szCntId, const char* propName, const char* propValue, bool allowRecurse = true);
	bool MSN_ABAddRemoveContact(const char* szCntId, int netId, bool add, bool allowRecurse = true);
	unsigned MSN_ABContactAdd(const char* szEmail, const char* szNick, int netId, const char* szInvite, bool search, bool retry = false, bool allowRecurse = true);
	void MSN_ABUpdateDynamicItem(bool allowRecurse = true);
	bool MSN_ABRefreshClist(unsigned int nTry = 0);

	ezxml_t abSoapHdr(const char* service, const char* scenario, ezxml_t& tbdy, char*& httphdr);
	char* GetABHost(const char* service, bool isSharing);
	void SetAbParam(MCONTACT hContact, const char *name, const char *par);
	void UpdateABHost(const char* service, const char* url);
	void UpdateABCacheKey(ezxml_t bdy, bool isSharing);

	ezxml_t getSoapResponse(ezxml_t bdy, const char* service);
	ezxml_t getSoapFault(ezxml_t bdy, bool err);

	char mycid[32];
	char mypuid[32];

	/////////////////////////////////////////////////////////////////////////////////////////
	//	MSN SOAP Roaming Storage

	bool MSN_StoreGetProfile(bool allowRecurse = true);
	bool MSN_StoreUpdateProfile(const char* szNick, const char* szStatus, bool lock, bool allowRecurse = true);
	bool MSN_StoreCreateProfile(bool allowRecurse = true);
	bool MSN_StoreShareItem(const char* id, bool allowRecurse = true);
	bool MSN_StoreCreateRelationships(bool allowRecurse = true);
	bool MSN_StoreDeleteRelationships(bool tile, bool allowRecurse = true);
	bool MSN_StoreCreateDocument(const wchar_t *sztName, const char *szMimeType, const char *szPicData, bool allowRecurse = true);
	bool MSN_StoreUpdateDocument(const wchar_t *sztName, const char *szMimeType, const char *szPicData, bool allowRecurse = true);
	bool MSN_StoreFindDocuments(bool allowRecurse = true);

	ezxml_t storeSoapHdr(const char* service, const char* scenario, ezxml_t& tbdy, char*& httphdr);
	char* GetStoreHost(const char* service);
	void UpdateStoreHost(const char* service, const char* url);
	void UpdateStoreCacheKey(ezxml_t bdy);

	char proresid[64];
	char expresid[64];
	char photoid[64];

	//////////////////////////////////////////////////////////////////////////////////////

	wchar_t *m_DisplayNameCache;
	wchar_t* GetContactNameT(MCONTACT hContact);

	int    getStringUtf(MCONTACT hContact, const char* name, DBVARIANT* result);
	int    getStringUtf(const char* name, DBVARIANT* result);
	void   setStringUtf(MCONTACT hContact, const char* name, const char* value);
};

struct CMPlugin : public ACCPROTOPLUGIN<CMsnProto>
{
	CMPlugin();

	int Load() override;
	int Unload() override;
};

#endif
