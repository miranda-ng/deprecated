#include "common.h"

bool CSteamProto::IsOnline()
{
	return m_iStatus > ID_STATUS_OFFLINE && m_hPollingThread;
}

bool CSteamProto::IsMe(const char *steamId)
{
	ptrA mySteamId(getStringA("SteamID"));
	if (!lstrcmpA(steamId, mySteamId))
		return true;

	return false;
}

void CSteamProto::OnGotRsaKey(const NETLIBHTTPREQUEST *response, void *arg)
{
	// load rsa key parts
	JSONNODE *root = json_parse(response->pData), *node;
	if (!root) return;

	node = json_get(root, "success");
	if (!json_as_bool(node)) return;

	node = json_get(root, "publickey_mod");
	ptrA modulus(mir_u2a(json_as_string(node)));

	// exponent "010001" is used as constant in CSteamProto::RsaEncrypt
	/*node = json_get(root, "publickey_exp");
	ptrA exponent(mir_u2a(json_as_string(node)));*/

	node = json_get(root, "timestamp");
	ptrA timestamp(mir_u2a(json_as_string(node)));
	//setString("Timestamp", timestamp);

	// encrypt password
	ptrA base64RsaEncryptedPassword;
	ptrA password(getStringA("Password"));

	DWORD error = 0;
	DWORD encryptedSize = 0;
	DWORD passwordSize = (DWORD)strlen(password);
	if ((error = RsaEncrypt(modulus, password, NULL, encryptedSize)) != 0)
	{
		debugLogA("CSteamProto::OnGotRsaKey: encryption error (%lu)", error);
		return;
	}

	BYTE *encryptedPassword = (BYTE*)mir_calloc(encryptedSize);
	if ((error = RsaEncrypt(modulus, password, encryptedPassword, encryptedSize)) != 0)
	{
		debugLogA("CSteamProto::OnGotRsaKey: encryption error (%lu)", error);
		return;
	}

	base64RsaEncryptedPassword = mir_base64_encode(encryptedPassword, encryptedSize);
	mir_free(encryptedPassword);

	//setString("EncryptedPassword", base64RsaEncryptedPassword);

	PasswordParam *param = (PasswordParam*)mir_alloc(sizeof(PasswordParam));
	strcpy(param->password, base64RsaEncryptedPassword);
	strcpy(param->timestamp, timestamp);
	
	// run authorization request
	ptrA username(mir_utf8encodeW(getWStringA("Username")));

	PushRequest(
		new SteamWebApi::DoLoginRequest(username, base64RsaEncryptedPassword, timestamp),
		&CSteamProto::OnAuthorization,
		param);
}

void CSteamProto::OnAuthorization(const NETLIBHTTPREQUEST *response, void *arg)
{
	JSONNODE *root = json_parse(response->pData), *node;

	node = json_get(root, "success");
	if (json_as_bool(node) == 0)
	{
		node = json_get(root, "message");
		const wchar_t *message = json_as_string(node);
		if (!lstrcmpi(json_as_string(node), L"Incorrect login"))
		{
			ShowNotification(TranslateTS(message));
			SetStatus(ID_STATUS_OFFLINE);
			mir_free(arg);
			return;
		}

		node = json_get(root, "emailauth_needed");
		if (json_as_bool(node) > 0)
		{
			node = json_get(root, "emailsteamid");
			ptrA guardId(mir_u2a(json_as_string(node)));

			node = json_get(root, "emaildomain");
			ptrA emailDomain(mir_utf8encodeW(json_as_string(node)));

			GuardParam guard;
			lstrcpyA(guard.domain, emailDomain);

			if (DialogBoxParam(
				g_hInstance,
				MAKEINTRESOURCE(IDD_GUARD),
				NULL,
				CSteamProto::GuardProc,
				(LPARAM)&guard) != 1)
				return;

			ptrA username(mir_utf8encodeW(getWStringA("Username")));
			PasswordParam *param = (PasswordParam*)arg;

			PushRequest(
				new SteamWebApi::DoLoginRequest(username, param->password, param->timestamp, guardId, guard.code, "MirandaNG"),
				&CSteamProto::OnAuthorization);
			return;
		}

		node = json_get(root, "captcha_needed");
		if (json_as_bool(node) > 0)
		{
			node = json_get(root, "captcha_gid");
			ptrA captchaId(mir_u2a(json_as_string(node)));

			mir_ptr<SteamWebApi::GetCaptchaRequest> request(new SteamWebApi::GetCaptchaRequest(captchaId));
			NETLIBHTTPREQUEST *response = request->Send(m_hNetlibUser);

			CaptchaParam captcha = { 0 };
			captcha.size = response->dataLength;
			captcha.data = (BYTE*)mir_alloc(captcha.size);
			memcpy(captcha.data, response->pData, captcha.size);

			CallService(MS_NETLIB_FREEHTTPREQUESTSTRUCT, 0, (LPARAM)response);

			int res = DialogBoxParam(
				g_hInstance,
				MAKEINTRESOURCE(IDD_CAPTCHA),
				NULL,
				CSteamProto::CaptchaProc,
				(LPARAM)&captcha);

			mir_free(captcha.data);

			if (res != 1)
			{
				SetStatus(ID_STATUS_OFFLINE);
				mir_free(arg);
				return;
			}

			ptrA username(mir_utf8encodeW(getWStringA("Username")));
			PasswordParam *param = (PasswordParam*)arg;

			PushRequest(
				new SteamWebApi::DoLoginRequest(username, param->password, param->timestamp, captchaId, captcha.text),
				&CSteamProto::OnAuthorization);
			return;
		}

		SetStatus(ID_STATUS_OFFLINE);
		mir_free(arg);
		return;
	}

	mir_free(arg);

	node = json_get(root, "login_complete");
	if (!json_as_bool(node))
	{
		SetStatus(ID_STATUS_OFFLINE);
		return;
	}

	node = json_get(root, "transfer_parameters");
	root = json_as_node(node);

	node = json_get(root, "steamid");
	ptrA steamId(mir_u2a(json_as_string(node)));
	setString("SteamID", steamId);

	node = json_get(root, "auth");
	ptrA auth(mir_u2a(json_as_string(node)));
	setString("TokenAuth", auth);

	node = json_get(root, "token");
	ptrA token(mir_u2a(json_as_string(node)));
	setString("TokenSecret", token);

	node = json_get(root, "token_secure");
	ptrA tokenSecure(mir_u2a(json_as_string(node)));
	setString("TokenSecure", tokenSecure);

	node = json_get(root, "webcookie");
	ptrA webcookie(mir_u2a(json_as_string(node)));
	setString("WebCookie", webcookie);

	PushRequest(
		new SteamWebApi::TransferRequest(token, steamId, auth, webcookie),
		&CSteamProto::OnTransfer);
}

void CSteamProto::OnTransfer(const NETLIBHTTPREQUEST *response, void *arg)
{
	ptrA token(getStringA("TokenSecret"));
	ptrA steamId(getStringA("SteamID"));

	PushRequest(
		new SteamWebApi::RedirectToHomeRequest(token, steamId),
		&CSteamProto::OnGotSession);
}

void CSteamProto::OnGotSession(const NETLIBHTTPREQUEST *response, void *arg)
{
	std::string sessionId;

	for (int i = 0; i < response->headersCount; i++)
	{
		if (lstrcmpiA(response->headers[i].szName, "Set-Cookie"))
			continue;

		std::string cookies = response->headers[i].szValue;
		size_t start = cookies.find("sessionid=") + 10;
		size_t end = cookies.substr(start).find(';');
		sessionId = cookies.substr(start, end - start + 10);
		setString("SessionID", sessionId.c_str());
		break;
	}

	ptrA token(getStringA("TokenSecret"));
	ptrA steamId(getStringA("SteamID"));

	PushRequest(
		new SteamWebApi::GetChatPageRequest(token, steamId, sessionId.c_str()),
		&CSteamProto::OnGotChatPage);
}

void CSteamProto::OnGotChatPage(const NETLIBHTTPREQUEST *response, void *arg)
{
	std::string chatToken;
	std::smatch match;
	std::regex regex("WebAPI = new CWebAPI\\( '.+?', '.+?', \"(.+?)\" \\);");

	const std::string content = response->pData;

	if (std::regex_search(content, match, regex))
	{
		chatToken = match[1];
		setString("TokenApi", chatToken.c_str());
	}

	PushRequest(
		new SteamWebApi::LogonRequest(chatToken.c_str()),
		&CSteamProto::OnLoggedOn);
}

void CSteamProto::OnLoggedOn(const NETLIBHTTPREQUEST *response, void *arg)
{
	JSONNODE *root = json_parse(response->pData), *node;

	node = json_get(root, "error");
	ptrW error(json_as_string(node));
	if (lstrcmpi(error, L"OK")/* || response->resultCode == HTTP_STATUS_UNAUTHORIZED*/)
	{
		// set status to offline
		m_iStatus = m_iDesiredStatus = ID_STATUS_OFFLINE;
		ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)ID_STATUS_CONNECTING, ID_STATUS_OFFLINE);
		return;
	}

	node = json_get(root, "umqid");
	setString("UMQID", ptrA(mir_u2a(json_as_string(node))));
	
	node = json_get(root, "message");
	setDword("MessageID", json_as_int(node));

	// load contact list
	ptrA steamId(getStringA("SteamID"));

	PushRequest(
		new SteamWebApi::GetFriendListRequest(steamId),
		&CSteamProto::OnGotFriendList);

	// start polling thread
	m_hPollingThread = ForkThreadEx(&CSteamProto::PollingThread, 0, NULL);

	// go to online now
	ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)ID_STATUS_CONNECTING, m_iStatus = m_iDesiredStatus);
}