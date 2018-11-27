#include "stdafx.h"
#include "MraAvatars.h"

#define PA_FORMAT_MAX		7

struct MRA_AVATARS_QUEUE : public FIFO_MT
{
	HNETLIBUSER hNetlibUser;
	HANDLE hThreadEvents[64];
	int    iThreadsCount, iThreadsRunning;
};

struct MRA_AVATARS_QUEUE_ITEM : public FIFO_MT_ITEM
{
	DWORD  dwAvatarsQueueID;
	DWORD  dwFlags;
	MCONTACT hContact;
};

#define FILETIME_SECOND		((DWORDLONG)10000000)
#define FILETIME_MINUTE		((DWORDLONG)FILETIME_SECOND * 60)


char szAvtSectName[MAX_PATH];
#define MRA_AVT_SECT_NAME	szAvtSectName

HNETLIBCONN MraAvatarsHttpConnect(HNETLIBUSER hNetlibUser, LPCSTR lpszHost, DWORD dwPort);

#define MAHTRO_AVT		0
#define MAHTRO_AVTMRIM		1
#define MAHTRO_AVTSMALL		2
#define MAHTRO_AVTSMALLMRIM	3

DWORD MraAvatarsHttpTransaction(HNETLIBCONN hConnection, DWORD dwRequestType, LPCSTR lpszUser, LPCSTR lpszDomain, LPCSTR lpszHost, DWORD dwReqObj, BOOL bUseKeepAliveConn, DWORD *pdwResultCode, BOOL *pbKeepAlive, DWORD *pdwFormat, size_t *pdwAvatarSize, INTERNET_TIME *pitLastModifiedTime);

DWORD CMraProto::MraAvatarsQueueInitialize(HANDLE *phAvatarsQueueHandle)
{
	mir_snprintf(szAvtSectName, "%s Avatars", m_szModuleName);

	if (phAvatarsQueueHandle == nullptr)
		return ERROR_INVALID_HANDLE;

	MRA_AVATARS_QUEUE *pmraaqAvatarsQueue = new MRA_AVATARS_QUEUE();

	wchar_t szBuffer[MAX_PATH];
	mir_snwprintf(szBuffer, L"%s %s", m_tszUserName, TranslateT("Avatars' plugin connections"));

	NETLIBUSER nlu = {};
	nlu.flags = NUF_OUTGOING | NUF_HTTPCONNS | NUF_UNICODE;
	nlu.szSettingsModule = MRA_AVT_SECT_NAME;
	nlu.szDescriptiveName.w = szBuffer;
	pmraaqAvatarsQueue->hNetlibUser = Netlib_RegisterUser(&nlu);
	if (pmraaqAvatarsQueue->hNetlibUser) {
		pmraaqAvatarsQueue->iThreadsCount = db_get_dw(0, MRA_AVT_SECT_NAME, "WorkThreadsCount", MRA_AVT_DEFAULT_WRK_THREAD_COUNTS);
		if (pmraaqAvatarsQueue->iThreadsCount == 0)
			pmraaqAvatarsQueue->iThreadsCount = 1;
		if (pmraaqAvatarsQueue->iThreadsCount > 64)
			pmraaqAvatarsQueue->iThreadsCount = 64;

		pmraaqAvatarsQueue->iThreadsRunning = 0;
		for (int i = 0; i < pmraaqAvatarsQueue->iThreadsCount; i++)
			ForkThread(&CMraProto::MraAvatarsThreadProc, pmraaqAvatarsQueue);

		*phAvatarsQueueHandle = (HANDLE)pmraaqAvatarsQueue;
	}
	return NO_ERROR;
}

void CMraProto::MraAvatarsQueueClear(HANDLE hQueue)
{
	if (!hQueue)
		return;

	MRA_AVATARS_QUEUE *pmraaqAvatarsQueue = (MRA_AVATARS_QUEUE*)hQueue;
	MRA_AVATARS_QUEUE_ITEM *pmraaqiAvatarsQueueItem;

	PROTO_AVATAR_INFORMATION ai = { 0 };
	ai.format = PA_FORMAT_UNKNOWN;

	while (FifoMTItemPop(pmraaqAvatarsQueue, nullptr, (LPVOID*)&pmraaqiAvatarsQueueItem) == NO_ERROR) {
		ai.hContact = pmraaqiAvatarsQueueItem->hContact;
		ProtoBroadcastAck(pmraaqiAvatarsQueueItem->hContact, ACKTYPE_AVATAR, ACKRESULT_FAILED, (HANDLE)&ai, 0);
		mir_free(pmraaqiAvatarsQueueItem);
	}
}

void CMraProto::MraAvatarsQueueSuspend(HANDLE hQueue)
{
	MRA_AVATARS_QUEUE *pmraaqAvatarsQueue = (MRA_AVATARS_QUEUE*)hQueue;
	MraAvatarsQueueClear(hQueue);
	for (int i = 0; i < pmraaqAvatarsQueue->iThreadsCount; i++)
		SetEvent(pmraaqAvatarsQueue->hThreadEvents[i]);
}

void CMraProto::MraAvatarsQueueDestroy(HANDLE hQueue)
{
	if (!hQueue)
		return;

	MRA_AVATARS_QUEUE *pmraaqAvatarsQueue = (MRA_AVATARS_QUEUE*)hQueue;
	Netlib_CloseHandle(pmraaqAvatarsQueue->hNetlibUser);
	delete pmraaqAvatarsQueue;
}

DWORD CMraProto::MraAvatarsQueueAdd(HANDLE hQueue, DWORD dwFlags, MCONTACT hContact, DWORD *pdwAvatarsQueueID)
{
	MRA_AVATARS_QUEUE *pmraaqAvatarsQueue = (MRA_AVATARS_QUEUE*)hQueue;
	if (pmraaqAvatarsQueue == nullptr || Miranda_IsTerminated())
		return ERROR_INVALID_HANDLE;

	MRA_AVATARS_QUEUE_ITEM *pmraaqiAvatarsQueueItem = (MRA_AVATARS_QUEUE_ITEM*)mir_calloc(sizeof(MRA_AVATARS_QUEUE_ITEM));
	if (!pmraaqiAvatarsQueueItem)
		return GetLastError();

	pmraaqiAvatarsQueueItem->dwAvatarsQueueID = GetTickCount();
	pmraaqiAvatarsQueueItem->dwFlags = dwFlags;
	pmraaqiAvatarsQueueItem->hContact = hContact;

	FifoMTItemPush(pmraaqAvatarsQueue, pmraaqiAvatarsQueueItem, (LPVOID)pmraaqiAvatarsQueueItem);
	if (pdwAvatarsQueueID)
		*pdwAvatarsQueueID = pmraaqiAvatarsQueueItem->dwAvatarsQueueID;

	mir_cslock(pmraaqAvatarsQueue->cs);
	int threadno = (pmraaqAvatarsQueue->iThreadsRunning + 1) % pmraaqAvatarsQueue->iThreadsCount;
	SetEvent(pmraaqAvatarsQueue->hThreadEvents[threadno]);
	return NO_ERROR;
}

void CMraProto::MraAvatarsThreadProc(LPVOID lpParameter)
{
	MRA_AVATARS_QUEUE *pmraaqAvatarsQueue = (MRA_AVATARS_QUEUE*)lpParameter;
	MRA_AVATARS_QUEUE_ITEM *pmraaqiAvatarsQueueItem;

	CMStringA szEmail, szServer;
	CMStringW wszFileName;
	BOOL bContinue, bKeepAlive, bUseKeepAliveConn, bFailed, bDownloadNew;
	BYTE btBuff[BUFF_SIZE_RCV];
	DWORD dwResultCode, dwAvatarFormat = PA_FORMAT_DEFAULT, dwReceived, dwServerPort, dwErrorCode;
	size_t dwAvatarSizeServer;
	FILETIME ftLastModifiedTimeServer, ftLastModifiedTimeLocal;
	SYSTEMTIME stAvatarLastModifiedTimeLocal;
	HNETLIBCONN hConnection = nullptr;
	NETLIBSELECT nls = { 0 };
	INTERNET_TIME itAvatarLastModifiedTimeServer;
	WCHAR szErrorText[2048];

	Thread_SetName("MRA: AvatarsThreadProc");

	HANDLE hThreadEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	{
		mir_cslock lck(pmraaqAvatarsQueue->cs);
		pmraaqAvatarsQueue->hThreadEvents[pmraaqAvatarsQueue->iThreadsRunning++] = hThreadEvent;
	}

	while (!Miranda_IsTerminated()) {
		if (FifoMTItemPop(pmraaqAvatarsQueue, nullptr, (LPVOID*)&pmraaqiAvatarsQueueItem) != NO_ERROR) { // waiting until service stop or new task
			NETLIB_CLOSEHANDLE(hConnection);
			WaitForSingleObjectEx(hThreadEvent, INFINITE, FALSE);
			continue;
		}
		
		// Try download.
		bFailed = TRUE;
		bDownloadNew = FALSE;

		if (!DB_GetStringA(NULL, MRA_AVT_SECT_NAME, "Server", szServer))
			szServer = MRA_AVT_DEFAULT_SERVER;
		dwServerPort = db_get_dw(0, MRA_AVT_SECT_NAME, "ServerPort", MRA_AVT_DEFAULT_SERVER_PORT);
		bUseKeepAliveConn = db_get_b(0, MRA_AVT_SECT_NAME, "UseKeepAliveConn", MRA_AVT_DEFAULT_USE_KEEPALIVE_CONN);

		if (mraGetStringA(pmraaqiAvatarsQueueItem->hContact, "e-mail", szEmail)) {
			szEmail.MakeLower();

			int iStart = 0;
			CMStringA szUser = szEmail.Tokenize("@", iStart);
			CMStringA szDomain = szEmail.Tokenize("@", iStart);
			if (!szUser.IsEmpty() && !szDomain.IsEmpty()) {
				ProtoBroadcastAck(pmraaqiAvatarsQueueItem->hContact, ACKTYPE_AVATAR, ACKRESULT_CONNECTING, nullptr, 0);
				if (hConnection == nullptr)
					hConnection = MraAvatarsHttpConnect(pmraaqAvatarsQueue->hNetlibUser, szServer, dwServerPort);
				if (hConnection) {
					ProtoBroadcastAck(pmraaqiAvatarsQueueItem->hContact, ACKTYPE_AVATAR, ACKRESULT_CONNECTED, nullptr, 0);
					ProtoBroadcastAck(pmraaqiAvatarsQueueItem->hContact, ACKTYPE_AVATAR, ACKRESULT_SENTREQUEST, nullptr, 0);
					if (!MraAvatarsHttpTransaction(hConnection, REQUEST_HEAD, szUser, szDomain, szServer, MAHTRO_AVTMRIM, bUseKeepAliveConn, &dwResultCode, &bKeepAlive, &dwAvatarFormat, &dwAvatarSizeServer, &itAvatarLastModifiedTimeServer)) {
						switch (dwResultCode) {
						case 200:
							if (MraAvatarsGetContactTime(pmraaqiAvatarsQueueItem->hContact, "AvatarLastModifiedTime", &stAvatarLastModifiedTimeLocal)) {
								SystemTimeToFileTime(&itAvatarLastModifiedTimeServer.stTime, &ftLastModifiedTimeServer);
								SystemTimeToFileTime(&stAvatarLastModifiedTimeLocal, &ftLastModifiedTimeLocal);

								if ((*((DWORDLONG*)&ftLastModifiedTimeServer)) != (*((DWORDLONG*)&ftLastModifiedTimeLocal))) {// need check for update
									bDownloadNew = TRUE;
									//ProtoBroadcastAck(pmraaqiAvatarsQueueItem->hContact, ACKTYPE_AVATAR, ACKRESULT_STATUS, 0, 0);
								}
								else {// avatar is valid
									if (MraAvatarsGetFileName(pmraaqAvatarsQueue, pmraaqiAvatarsQueueItem->hContact, dwAvatarFormat, wszFileName) == NO_ERROR) {
										if (IsFileExist(wszFileName))
											bFailed = FALSE;
										else
											bDownloadNew = TRUE;
									}
								}
							}
							else // need update
								bDownloadNew = TRUE;

							break;
						case 404:// return def avatar
							if (MraAvatarsGetFileName((HANDLE)pmraaqAvatarsQueue, NULL, PA_FORMAT_DEFAULT, wszFileName) == NO_ERROR) {
								if (IsFileExist(wszFileName)) {
									dwAvatarFormat = ProtoGetAvatarFormat(wszFileName);
									bFailed = FALSE;
								}
								else//loading default avatar
									bDownloadNew = TRUE;
							}
							break;

						default:
							mir_snwprintf(szErrorText, TranslateT("Avatars: server return HTTP code: %lu"), dwResultCode);
							ShowFormattedErrorMessage(szErrorText, NO_ERROR);
							break;
						}
					}
					if (bUseKeepAliveConn == FALSE || bKeepAlive == FALSE) NETLIB_CLOSEHANDLE(hConnection);
				}

				if (bDownloadNew) {
					if (hConnection == nullptr)
						hConnection = MraAvatarsHttpConnect(pmraaqAvatarsQueue->hNetlibUser, szServer, dwServerPort);

					if (hConnection) {
						ProtoBroadcastAck(pmraaqiAvatarsQueueItem->hContact, ACKTYPE_AVATAR, ACKRESULT_DATA, nullptr, 0);
						if (MraAvatarsHttpTransaction(hConnection, REQUEST_GET, szUser, szDomain, szServer, MAHTRO_AVT, bUseKeepAliveConn, &dwResultCode, &bKeepAlive, &dwAvatarFormat, &dwAvatarSizeServer, &itAvatarLastModifiedTimeServer) == NO_ERROR && dwResultCode == 200) {
							if (!MraAvatarsGetFileName(pmraaqAvatarsQueue, pmraaqiAvatarsQueueItem->hContact, dwAvatarFormat, wszFileName)) {
								HANDLE hFile = CreateFile(wszFileName, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
								if (hFile != INVALID_HANDLE_VALUE) {
									DWORD dwWritten = 0;
									bContinue = TRUE;
									nls.dwTimeout = (1000 * db_get_dw(0, MRA_AVT_SECT_NAME, "TimeOutReceive", MRA_AVT_DEFAULT_TIMEOUT_RECV));
									nls.hReadConns[0] = hConnection;

									while (bContinue) {
										switch (Netlib_Select(&nls)) {
										case SOCKET_ERROR:
										case 0:// Time out
											dwErrorCode = GetLastError();
											ShowFormattedErrorMessage(L"Avatars: error on receive file data", dwErrorCode);
											bContinue = FALSE;
											break;
										case 1:
											dwReceived = Netlib_Recv(hConnection, (LPSTR)&btBuff, _countof(btBuff), 0);
											if (dwReceived == 0 || dwReceived == SOCKET_ERROR) {
												dwErrorCode = GetLastError();
												ShowFormattedErrorMessage(L"Avatars: error on receive file data", dwErrorCode);
												bContinue = FALSE;
											}
											else {
												if (WriteFile(hFile, (LPVOID)&btBuff, dwReceived, &dwReceived, nullptr)) {
													dwWritten += dwReceived;
													if (dwWritten >= dwAvatarSizeServer)
														bContinue = FALSE;
												}
												else {
													dwErrorCode = GetLastError();
													ShowFormattedErrorMessage(L"Avatars: cant write file data, error", dwErrorCode);
													bContinue = FALSE;
												}
											}
											break;
										}
									}
									CloseHandle(hFile);
									bFailed = FALSE;
								}
								else {
									dwErrorCode = GetLastError();
									mir_snwprintf(szErrorText, TranslateT("Avatars: can't open file %s, error"), wszFileName.c_str());
									ShowFormattedErrorMessage(szErrorText, dwErrorCode);
								}
							}
						}
						else _CrtDbgBreak();

						if (bUseKeepAliveConn == FALSE || bKeepAlive == FALSE)
							NETLIB_CLOSEHANDLE(hConnection);
					}
				}
			}
		}

		PROTO_AVATAR_INFORMATION ai;
		if (bFailed) {
			DeleteFile(wszFileName);
			ai.hContact = pmraaqiAvatarsQueueItem->hContact;
			ai.format = PA_FORMAT_UNKNOWN;
			ai.filename[0] = 0;
			ProtoBroadcastAck(pmraaqiAvatarsQueueItem->hContact, ACKTYPE_AVATAR, ACKRESULT_FAILED, (HANDLE)&ai, 0);
		}
		else {
			ai.hContact = pmraaqiAvatarsQueueItem->hContact;
			ai.format = dwAvatarFormat;
			if (db_get_b(0, MRA_AVT_SECT_NAME, "ReturnAbsolutePath", MRA_AVT_DEFAULT_RET_ABC_PATH))
				wcsncpy_s(ai.filename, wszFileName, _TRUNCATE);
			else
				PathToRelativeW(wszFileName, ai.filename);

			SetContactAvatarFormat(pmraaqiAvatarsQueueItem->hContact, dwAvatarFormat);
			MraAvatarsSetContactTime(pmraaqiAvatarsQueueItem->hContact, "AvatarLastModifiedTime", &itAvatarLastModifiedTimeServer.stTime);
			// write owner avatar file name to DB
			if (pmraaqiAvatarsQueueItem->hContact == NULL) // proto avatar
				CallService(MS_AV_REPORTMYAVATARCHANGED, (WPARAM)m_szModuleName, 0);

			ProtoBroadcastAck(pmraaqiAvatarsQueueItem->hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, (HANDLE)&ai, 0);
		}
		mir_free(pmraaqiAvatarsQueueItem);
	}
	CloseHandle(hThreadEvent);
}

HNETLIBCONN MraAvatarsHttpConnect(HNETLIBUSER hNetlibUser, LPCSTR lpszHost, DWORD dwPort)
{
	NETLIBOPENCONNECTION nloc = { 0 };
	nloc.cbSize = sizeof(nloc);
	nloc.flags = (NLOCF_HTTP | NLOCF_V2);
	nloc.szHost = lpszHost;
	nloc.wPort = (IsHTTPSProxyUsed(hNetlibUser)) ? MRA_SERVER_PORT_HTTPS : dwPort;
	nloc.timeout = db_get_dw(0, MRA_AVT_SECT_NAME, "TimeOutConnect", MRA_AVT_DEFAULT_TIMEOUT_CONN);
	if (nloc.timeout < MRA_TIMEOUT_CONN_MIN) nloc.timeout = MRA_TIMEOUT_CONN_MIN;
	if (nloc.timeout > MRA_TIMEOUT_CONN_MAX) nloc.timeout = MRA_TIMEOUT_CONN_MAX;

	DWORD dwConnectReTryCount = db_get_dw(0, MRA_AVT_SECT_NAME, "ConnectReTryCount", MRA_AVT_DEFAULT_CONN_RETRY_COUNT);
	DWORD dwCurConnectReTryCount = dwConnectReTryCount;
	HNETLIBCONN hConnection;
	do {
		hConnection = Netlib_OpenConnection(hNetlibUser, &nloc);
	}
		while (--dwCurConnectReTryCount && hConnection == nullptr);

	return hConnection;
}

DWORD MraAvatarsHttpTransaction(HNETLIBCONN hConnection, DWORD dwRequestType, LPCSTR lpszUser, LPCSTR lpszDomain, LPCSTR lpszHost, DWORD dwReqObj, BOOL bUseKeepAliveConn, DWORD *pdwResultCode, BOOL *pbKeepAlive, DWORD *pdwFormat, size_t *pdwAvatarSize, INTERNET_TIME *pitLastModifiedTime)
{
	if (pdwResultCode)      *pdwResultCode = 0;
	if (pbKeepAlive)        *pbKeepAlive = FALSE;
	if (pdwFormat)          *pdwFormat = PA_FORMAT_UNKNOWN;
	if (pdwAvatarSize)      *pdwAvatarSize = 0;
	if (pitLastModifiedTime) memset(pitLastModifiedTime, 0, sizeof(INTERNET_TIME));

	if (!hConnection)
		return ERROR_INVALID_HANDLE;

	LPSTR lpszReqObj;

	switch (dwReqObj) {
		case MAHTRO_AVT:          lpszReqObj = "_avatar"; break;
		case MAHTRO_AVTMRIM:      lpszReqObj = "_mrimavatar"; break;
		case MAHTRO_AVTSMALL:     lpszReqObj = "_avatarsmall"; break;
		case MAHTRO_AVTSMALLMRIM: lpszReqObj = "_mrimavatarsmall"; break;
		default:                  lpszReqObj = ""; break;
	}

	char szBuff[4096];
	mir_snprintf(szBuff, "http://%s/%s/%s/%s", lpszHost, lpszDomain, lpszUser, lpszReqObj);
	CMStringA szSelfVersionString = MraGetSelfVersionString();

	NETLIBHTTPHEADER nlbhHeaders[8] = {};
	nlbhHeaders[0].szName = "User-Agent";		nlbhHeaders[0].szValue = (LPSTR)szSelfVersionString.c_str();
	nlbhHeaders[1].szName = "Accept-Encoding";	nlbhHeaders[1].szValue = "deflate";
	nlbhHeaders[2].szName = "Pragma";		nlbhHeaders[2].szValue = "no-cache";
	nlbhHeaders[3].szName = "Connection";		nlbhHeaders[3].szValue = (bUseKeepAliveConn) ? "keep-alive" : "close";

	NETLIBHTTPREQUEST nlhr = { 0 };
	nlhr.cbSize = sizeof(nlhr);
	nlhr.requestType = dwRequestType;
	nlhr.szUrl = szBuff;
	nlhr.headers = (NETLIBHTTPHEADER*)&nlbhHeaders;
	nlhr.headersCount = 4;

	DWORD dwSent = Netlib_SendHttpRequest(hConnection, &nlhr);
	if (dwSent == SOCKET_ERROR || !dwSent)
		return GetLastError();

	NETLIBHTTPREQUEST *pnlhr = Netlib_RecvHttpHeaders(hConnection);
	if (!pnlhr)
		return GetLastError();

	for (int i = 0; i < pnlhr->headersCount; i++) {
		if (!_strnicmp(pnlhr->headers[i].szName, "Connection", 10)) {
			if (pbKeepAlive)
				*pbKeepAlive = !_strnicmp(pnlhr->headers[i].szValue, "keep-alive", 10);
		}
		else if (!_strnicmp(pnlhr->headers[i].szName, "Content-Type", 12)) {
			if (pdwFormat)
				*pdwFormat = ProtoGetAvatarFormatByMimeType(_A2T(pnlhr->headers[i].szValue));
		}
		else if (!_strnicmp(pnlhr->headers[i].szName, "Content-Length", 14)) {
			if (pdwAvatarSize)
				*pdwAvatarSize = atol(pnlhr->headers[i].szValue);
		}
		else if (!_strnicmp(pnlhr->headers[i].szName, "Last-Modified", 13)) {
			if (pitLastModifiedTime)
				InternetTimeGetTime(pnlhr->headers[i].szValue, *pitLastModifiedTime);
		}
	}

	if (pdwResultCode)
		*pdwResultCode = pnlhr->resultCode;
	Netlib_FreeHttpRequest(pnlhr);
	return 0;
}

bool CMraProto::MraAvatarsGetContactTime(MCONTACT hContact, LPSTR lpszValueName, SYSTEMTIME *pstTime)
{
	INTERNET_TIME itAvatarLastModifiedTimeLocal;
	CMStringA szBuff;

	if (nullptr == lpszValueName ||
	    nullptr == pstTime)
		return false;
	if (false == mraGetStringA(hContact, lpszValueName, szBuff))
		return false;
	if (InternetTimeGetTime(szBuff, itAvatarLastModifiedTimeLocal) != NO_ERROR)
		return false;
	memcpy(pstTime, &itAvatarLastModifiedTimeLocal.stTime, sizeof(SYSTEMTIME));
	return true;
}

void CMraProto::MraAvatarsSetContactTime(MCONTACT hContact, LPSTR lpszValueName, SYSTEMTIME *pstTime)
{
	if (!lpszValueName)
		return;

	INTERNET_TIME itTime;
	if (pstTime) {
		itTime.lTimeZone = 0;
		memcpy(&itTime.stTime, pstTime, sizeof(SYSTEMTIME));
	}
	else InternetTimeGetCurrentTime(&itTime);

	if (itTime.stTime.wYear)
		mraSetStringExA(hContact, lpszValueName, InternetTimeGetString(&itTime));
	else
		delSetting(hContact, lpszValueName);
}

DWORD CMraProto::MraAvatarsGetFileName(HANDLE hQueue, MCONTACT hContact, DWORD dwFormat, CMStringW &res)
{
	res.Empty();
	if (hQueue == nullptr)
		return ERROR_INVALID_HANDLE;

	if (IsContactChatAgent(hContact))
		return ERROR_NOT_SUPPORTED;

	wchar_t tszBase[MAX_PATH];
	mir_snwprintf(tszBase, L"%s\\%s\\", VARSW(L"%miranda_avatarcache%"), m_tszUserName);
	res = tszBase;

	// some path in buff and free space for file name is avaible
	CreateDirectoryTreeW(res);

	if (dwFormat != PA_FORMAT_DEFAULT) {
		CMStringW szEmail;
		if (mraGetStringW(hContact, "e-mail", szEmail)) {
			szEmail.MakeLower();
			res += szEmail + ProtoGetAvatarExtension(dwFormat);
			return NO_ERROR;
		}
	}
	else {
		CMStringW szDefName;
		if (!DB_GetStringW(NULL, MRA_AVT_SECT_NAME, "DefaultAvatarFileName", szDefName)) {
			res += MRA_AVT_DEFAULT_AVT_FILENAME;
			return NO_ERROR;
		}
	}

	return ERROR_INSUFFICIENT_BUFFER;
}

DWORD CMraProto::MraAvatarsQueueGetAvatar(HANDLE hQueue, DWORD dwFlags, MCONTACT hContact, DWORD *pdwAvatarsQueueID, DWORD *pdwFormat, LPTSTR lpszPath)
{
	DWORD dwRetCode = GAIR_NOAVATAR;

	if ( !hQueue)
		return GAIR_NOAVATAR;
	if ( !db_get_b(0, MRA_AVT_SECT_NAME, "Enable", MRA_AVT_DEFAULT_ENABLE))
		return GAIR_NOAVATAR;
	if (IsContactChatAgent(hContact)) // @chat.agent conference
		return GAIR_NOAVATAR;

	BOOL bQueueAdd = TRUE;// check for updates
	SYSTEMTIME stAvatarLastCheckTime;

	if ((dwFlags & GAIF_FORCE) == 0)// если флаг принудит. обновления, то даже не проверяем времени последнего обновления
	if (MraAvatarsGetContactTime(hContact, "AvatarLastCheckTime", &stAvatarLastCheckTime)) {
		CMStringW wszFileName;
		FILETIME ftCurrentTime, ftExpireTime;

		GetSystemTimeAsFileTime(&ftCurrentTime);
		SystemTimeToFileTime(&stAvatarLastCheckTime, &ftExpireTime);
		(*((DWORDLONG*)&ftExpireTime)) += (FILETIME_MINUTE*(DWORDLONG)db_get_dw(0, MRA_AVT_SECT_NAME, "CheckInterval", MRA_AVT_DEFAULT_CHK_INTERVAL));

		if ((*((DWORDLONG*)&ftExpireTime)) > (*((DWORDLONG*)&ftCurrentTime)))
		if (MraAvatarsGetFileName(hQueue, hContact, GetContactAvatarFormat(hContact, PA_FORMAT_DEFAULT), wszFileName) == NO_ERROR)
		if (IsFileExist(wszFileName)) {
			// файл с аватаром существует и не устарел/не было комманды обновлять(просто запрос имени)
			if (lpszPath) {
				if (db_get_b(0, MRA_AVT_SECT_NAME, "ReturnAbsolutePath", MRA_AVT_DEFAULT_RET_ABC_PATH))
					mir_wstrncpy(lpszPath, wszFileName, MAX_PATH);
				else
					PathToRelativeW(wszFileName, lpszPath);
			}
			if (pdwFormat)
				*pdwFormat = ProtoGetAvatarFormat(lpszPath);
			dwRetCode = GAIR_SUCCESS;
			bQueueAdd = FALSE;
		}
	}

	if (bQueueAdd || (dwFlags & GAIF_FORCE))
	if (!MraAvatarsQueueAdd(hQueue, dwFlags, hContact, pdwAvatarsQueueID)) {
		MraAvatarsSetContactTime(hContact, "AvatarLastCheckTime", nullptr);
		dwRetCode = GAIR_WAITFOR;
	}
	return dwRetCode;
}

DWORD CMraProto::MraAvatarsQueueGetAvatarSimple(HANDLE hQueue, DWORD dwFlags, MCONTACT hContact)
{
	if ( !hQueue)
		return GAIR_NOAVATAR;

	PROTO_AVATAR_INFORMATION ai = { 0 };
	ai.hContact = hContact;
	DWORD dwRetCode = MraAvatarsQueueGetAvatar(hQueue, dwFlags, hContact, nullptr, (DWORD*)&ai.format, ai.filename);
	if (dwRetCode != GAIR_SUCCESS)
		return dwRetCode;
	
	// write owner avatar file name to DB
	if (hContact == NULL)
		CallService(MS_AV_REPORTMYAVATARCHANGED, (WPARAM)m_szModuleName, 0);
	ProtoBroadcastAck(hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, (HANDLE)&ai, 0);
	return GAIR_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Avatars options

WORD wMraAvatarsControlsList[] = {
	IDC_SERVER,
	IDC_SERVERPORT,
	IDC_BUTTON_DEFAULT,
	IDC_USE_KEEPALIVE_CONN,
	IDC_UPD_CHECK_INTERVAL,
	IDC_RETURN_ABC_PATH,
	IDC_DELETE_AVT_ON_CONTACT_DELETE
};

INT_PTR CALLBACK MraAvatarsQueueDlgProcOpts(HWND hWndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	CMraProto *ppro = (CMraProto*)GetWindowLongPtr(hWndDlg, GWLP_USERDATA);

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hWndDlg);
		SetWindowLongPtr(hWndDlg, GWLP_USERDATA, lParam);
		ppro = (CMraProto*)lParam;
		{
			CheckDlgButton(hWndDlg, IDC_ENABLE, db_get_b(0, MRA_AVT_SECT_NAME, "Enable", MRA_AVT_DEFAULT_ENABLE) ? BST_CHECKED : BST_UNCHECKED);

			CMStringW szServer;
			if (DB_GetStringW(NULL, MRA_AVT_SECT_NAME, "Server", szServer))
				SetDlgItemText(hWndDlg, IDC_SERVER, szServer.c_str());
			else
				SetDlgItemTextA(hWndDlg, IDC_SERVER, MRA_AVT_DEFAULT_SERVER);

			SetDlgItemInt(hWndDlg, IDC_SERVERPORT, db_get_dw(0, MRA_AVT_SECT_NAME, "ServerPort", MRA_AVT_DEFAULT_SERVER_PORT), FALSE);
			CheckDlgButton(hWndDlg, IDC_USE_KEEPALIVE_CONN, db_get_b(0, MRA_AVT_SECT_NAME, "UseKeepAliveConn", MRA_AVT_DEFAULT_USE_KEEPALIVE_CONN) ? BST_CHECKED : BST_UNCHECKED);
			SetDlgItemInt(hWndDlg, IDC_UPD_CHECK_INTERVAL, db_get_dw(0, MRA_AVT_SECT_NAME, "CheckInterval", MRA_AVT_DEFAULT_CHK_INTERVAL), FALSE);
			CheckDlgButton(hWndDlg, IDC_RETURN_ABC_PATH, db_get_b(0, MRA_AVT_SECT_NAME, "ReturnAbsolutePath", MRA_AVT_DEFAULT_RET_ABC_PATH) ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWndDlg, IDC_DELETE_AVT_ON_CONTACT_DELETE, db_get_b(0, MRA_AVT_SECT_NAME, "DeleteAvtOnContactDelete", MRA_DELETE_AVT_ON_CONTACT_DELETE) ? BST_CHECKED : BST_UNCHECKED);

			EnableControlsArray(hWndDlg, (WORD*)&wMraAvatarsControlsList, _countof(wMraAvatarsControlsList), IsDlgButtonChecked(hWndDlg, IDC_ENABLE));
		}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_ENABLE)
			EnableControlsArray(hWndDlg, (WORD*)&wMraAvatarsControlsList, _countof(wMraAvatarsControlsList), IsDlgButtonChecked(hWndDlg, IDC_ENABLE));

		if (LOWORD(wParam) == IDC_BUTTON_DEFAULT) {
			SetDlgItemTextA(hWndDlg, IDC_SERVER, MRA_AVT_DEFAULT_SERVER);
			SetDlgItemInt(hWndDlg, IDC_SERVERPORT, MRA_AVT_DEFAULT_SERVER_PORT, FALSE);
		}

		if ((LOWORD(wParam) == IDC_SERVER || LOWORD(wParam) == IDC_SERVERPORT || LOWORD(wParam) == IDC_UPD_CHECK_INTERVAL) && (HIWORD(wParam) != EN_CHANGE || (HWND)lParam != GetFocus())) return FALSE;
		SendMessage(GetParent(hWndDlg), PSM_CHANGED, 0, 0);
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {
		case PSN_APPLY:
			db_set_b(0, MRA_AVT_SECT_NAME, "Enable", IsDlgButtonChecked(hWndDlg, IDC_ENABLE));
			db_set_b(0, MRA_AVT_SECT_NAME, "DeleteAvtOnContactDelete", IsDlgButtonChecked(hWndDlg, IDC_DELETE_AVT_ON_CONTACT_DELETE));
			db_set_b(0, MRA_AVT_SECT_NAME, "ReturnAbsolutePath", IsDlgButtonChecked(hWndDlg, IDC_RETURN_ABC_PATH));
			db_set_dw(0, MRA_AVT_SECT_NAME, "CheckInterval", GetDlgItemInt(hWndDlg, IDC_UPD_CHECK_INTERVAL, nullptr, FALSE));
			db_set_b(0, MRA_AVT_SECT_NAME, "UseKeepAliveConn", IsDlgButtonChecked(hWndDlg, IDC_USE_KEEPALIVE_CONN));
			db_set_dw(0, MRA_AVT_SECT_NAME, "ServerPort", GetDlgItemInt(hWndDlg, IDC_SERVERPORT, nullptr, FALSE));

			wchar_t szServer[MAX_PATH];
			GetDlgItemText(hWndDlg, IDC_SERVER, szServer, _countof(szServer));
			db_set_ws(0, MRA_AVT_SECT_NAME, "Server", szServer);
			return TRUE;
		}
		break;
	}
	return FALSE;
}


DWORD CMraProto::MraAvatarsDeleteContactAvatarFile(HANDLE hQueue, MCONTACT hContact)
{
	if (hQueue == nullptr)
		return ERROR_INVALID_HANDLE;

	DWORD dwAvatarFormat = GetContactAvatarFormat(hContact, PA_FORMAT_UNKNOWN);
	if (db_get_b(0, MRA_AVT_SECT_NAME, "DeleteAvtOnContactDelete", MRA_DELETE_AVT_ON_CONTACT_DELETE) && dwAvatarFormat != PA_FORMAT_DEFAULT) {
		CMStringW szFileName;
		if (!MraAvatarsGetFileName(hQueue, hContact, dwAvatarFormat, szFileName))
			return DeleteFile(szFileName);
	}
	return NO_ERROR;
}
