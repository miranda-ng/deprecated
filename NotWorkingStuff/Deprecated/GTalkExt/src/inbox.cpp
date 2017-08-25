//***************************************************************************************
//
//   Google Extension plugin for the Miranda IM's Jabber protocol
//   Copyright (c) 2011 bems@jabber.org, George Hazan (ghazan@jabber.ru)
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//***************************************************************************************

#include "stdafx.h"
#include "inbox.h"
#include "notifications.h"
#include "db.h"
#include "options.h"

#define COMMON_GMAIL_HOST1 L"gmail.com"
#define COMMON_GMAIL_HOST2 L"googlemail.com"

#define AUTH_REQUEST_URL    "https://www.google.com/accounts/ClientAuth"
#define AUTH_REQUEST_PARAMS "Email=%s&Passwd=%s&accountType=HOSTED_OR_GOOGLE&skipvpage=true&PersistentCookie=false"

#define ISSUE_TOKEN_REQUEST_URL    "https://www.google.com/accounts/IssueAuthToken"
#define ISSUE_TOKEN_REQUEST_PARAMS "SID=%s&LSID=%s&Session=true&skipvpage=true&service=gaia"

#define TOKEN_AUTH_URL "https://www.google.com/accounts/TokenAuth?auth=%s&service=mail&continue=%s&source=googletalk"

const NETLIBHTTPHEADER HEADER_URL_ENCODED = { "Content-Type", "application/x-www-form-urlencoded" };
#define HTTP_OK 200

#define SID_KEY_NAME  "SID="
#define LSID_KEY_NAME "LSID="

#define LOGIN_PASS_SETTING_NAME "Password"

#define INBOX_URL_FORMAT  L"https://mail.google.com/%s%s/#inbox"

// 3 lines from netlib.h
#define GetNetlibHandleType(h)  (h?*(int*)h:NLH_INVALID)
#define NLH_INVALID      0
#define NLH_USER         'USER'

LPSTR HttpPost(HNETLIBUSER hUser, LPSTR reqUrl, LPSTR reqParams)
{
	NETLIBHTTPREQUEST nlhr = { sizeof(nlhr) };
	nlhr.requestType = REQUEST_POST;
	nlhr.flags = NLHRF_HTTP11 | NLHRF_SSL | NLHRF_NODUMP | NLHRF_NODUMPHEADERS;
	nlhr.szUrl = reqUrl;
	nlhr.headers = (NETLIBHTTPHEADER*)&HEADER_URL_ENCODED;
	nlhr.headersCount = 1;
	nlhr.pData = reqParams;
	nlhr.dataLength = (int)mir_strlen(reqParams);

	NLHR_PTR pResp(Netlib_HttpTransaction(hUser, &nlhr));
	return ((pResp && pResp->resultCode == HTTP_OK) ? mir_strdup(pResp->pData) : nullptr);
}

LPSTR MakeRequest(HNETLIBUSER hUser, LPSTR reqUrl, LPSTR reqParamsFormat, LPSTR p1, LPSTR p2)
{
	ptrA encodedP1(mir_urlEncode(p1)), encodedP2(mir_urlEncode(p2));
	size_t size = mir_strlen(reqParamsFormat) + 1 + mir_strlen(encodedP1) + mir_strlen(encodedP2);
	LPSTR reqParams = (LPSTR)alloca(size);
	mir_snprintf(reqParams, size, reqParamsFormat, encodedP1, encodedP2);
	return HttpPost(hUser, reqUrl, reqParams);
}

LPSTR FindSid(LPSTR resp, LPSTR *LSID)
{
	LPSTR SID = strstr(resp, SID_KEY_NAME);
	*LSID = strstr(resp, LSID_KEY_NAME);
	if (!SID || !*LSID) return NULL;

	if (SID - 1 == *LSID) SID = strstr(SID + 1, SID_KEY_NAME);
	if (!SID) return NULL;

	SID += mir_strlen(SID_KEY_NAME);
	LPSTR term = strstr(SID, "\n");
	if (term) term[0] = 0;

	*LSID += mir_strlen(LSID_KEY_NAME);
	term = strstr(*LSID, "\n");
	if (term) term[0] = 0;

	return SID;
}

void DoOpenUrl(LPSTR tokenResp, LPSTR url)
{
	ptrA encodedUrl(mir_urlEncode(url)), encodedToken(mir_urlEncode(tokenResp));
	size_t size = mir_strlen(TOKEN_AUTH_URL) + 1 + mir_strlen(encodedToken) + mir_strlen(encodedUrl);
	LPSTR composedUrl = (LPSTR)alloca(size);
	mir_snprintf(composedUrl, size, TOKEN_AUTH_URL, encodedToken, encodedUrl);
	Utils_OpenUrl(composedUrl);
}

BOOL AuthAndOpen(HNETLIBUSER hUser, LPSTR url, LPSTR mailbox, LPSTR pwd)
{
	ptrA authResp(MakeRequest(hUser, AUTH_REQUEST_URL, AUTH_REQUEST_PARAMS, mailbox, pwd));
	if (!authResp)
		return FALSE;

	LPSTR LSID;
	LPSTR SID = FindSid(authResp, &LSID);
	ptrA tokenResp(MakeRequest(hUser, ISSUE_TOKEN_REQUEST_URL, ISSUE_TOKEN_REQUEST_PARAMS, SID, LSID));
	if (!tokenResp)
		return FALSE;

	DoOpenUrl(tokenResp, url);
	return TRUE;
}

struct OPEN_URL_HEADER {
	LPSTR url;
	LPSTR mailbox;
	LPSTR pwd;
	LPCSTR acc;
};

HNETLIBUSER FindNetUserHandle(LPCSTR acc)
{
	IJabberInterface *ji = getJabberApi(acc);
	if (!ji)
		return NULL;

	return ji->GetHandle();
}

void OpenUrlThread(void *param)
{
	OPEN_URL_HEADER* data = (OPEN_URL_HEADER*)param;

	HNETLIBUSER hUser = FindNetUserHandle(data->acc);
	if (!hUser || !AuthAndOpen(hUser, data->url, data->mailbox, data->pwd))
		Utils_OpenUrl(data->url);
	free(data);
}

int GetMailboxPwd(LPCSTR acc, LPSTR *pwd, int buffSize)
{
	char buff[256];
	if (db_get_static(NULL, acc, LOGIN_PASS_SETTING_NAME, buff, sizeof(buff)))
		return 0;

	int result = (int)mir_strlen(buff);

	if (pwd) {
		if (buffSize < result + 1)
			result = buffSize - 1;
		memcpy(*pwd, &buff, result + 1);
	}

	return result;
}

BOOL OpenUrlWithAuth(LPCSTR acc, LPCTSTR mailbox, LPCTSTR url)
{
	int pwdLen = GetMailboxPwd(acc, NULL, 0);
	if (!pwdLen++) return FALSE;

	size_t urlLen = mir_wstrlen(url) + 1;
	size_t mailboxLen = mir_wstrlen(mailbox) + 1;

	OPEN_URL_HEADER *data = (OPEN_URL_HEADER*)malloc(sizeof(OPEN_URL_HEADER) + urlLen + mailboxLen + pwdLen);
	data->url = (LPSTR)data + sizeof(OPEN_URL_HEADER);
	memcpy(data->url, _T2A(url), urlLen);

	data->mailbox = data->url + urlLen;
	memcpy(data->mailbox, _T2A(mailbox), mailboxLen);

	data->pwd = data->mailbox + mailboxLen;
	if (!GetMailboxPwd(acc, &data->pwd, pwdLen))
		return FALSE;

	data->acc = acc;
	mir_forkthread(OpenUrlThread, data);
	return TRUE;
}

void OpenUrl(LPCSTR acc, LPCTSTR mailbox, LPCTSTR url)
{
	extern DWORD itlsSettings;
	if (!ReadCheckbox(0, IDC_AUTHONMAILBOX, (UINT_PTR)TlsGetValue(itlsSettings)) || !OpenUrlWithAuth(acc, mailbox, url))
		Utils_OpenUrlW(url);
}

void OpenContactInbox(LPCSTR szModuleName)
{
	ptrW tszJid(db_get_wsa(0, szModuleName, "jid"));
	if (tszJid == NULL)
		return;

	LPTSTR host = wcschr(tszJid, '@');
	if (!host)
		return;
	*host++ = 0;

	wchar_t buf[1024];
	if (mir_wstrcmpi(host, COMMON_GMAIL_HOST1) && mir_wstrcmpi(host, COMMON_GMAIL_HOST2))
		mir_snwprintf(buf, INBOX_URL_FORMAT, L"a/", host);   // hosted
	else
		mir_snwprintf(buf, INBOX_URL_FORMAT, L"", L"mail"); // common
	OpenUrl(szModuleName, tszJid, buf);
}
