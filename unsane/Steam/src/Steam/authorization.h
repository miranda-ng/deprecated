#ifndef _STEAM_AUTHORIZATION_H_
#define _STEAM_AUTHORIZATION_H_

namespace SteamWebApi
{
	class DoLoginRequest : public HttpsPostRequest
	{
	private:
		void InitData(const char *username, const char *password, const char *timestamp, const char *guardId = "", const char *guardCode = "", const char *loginFriendlyName = "", const char *captchaId = "-1", const char *captchaText = "")
		{
			char data[1024];
			mir_snprintf(data, SIZEOF(data),
				"username=%s&password=%s&emailsteamid=%s&emailauth=%s&loginfriendlyname=%s&captchagid=%s&captcha_text=%s&rsatimestamp=%s&donotcache=%lld&remember_login=false",
				ptrA(mir_urlEncode(username)),
				ptrA(mir_urlEncode(password)),
				guardId,
				guardCode,
				loginFriendlyName,
				captchaId,
				ptrA(mir_urlEncode(captchaText)),
				timestamp,
				time(NULL));

			SetData(data, strlen(data));

			/*AddHeader("Accept", "text/javascript, text/html, application/xml, text/xml, *//**");
			AddHeader("Accept-Encoding", "gzip, deflate, lzma, sdch");
			AddHeader("Accept-Language", "ru-RU,ru;q=0.8,en-US;q=0.6,en;q=0.4");
			AddHeader("X-Prototype-Version", "1.7");
			AddHeader("X-Requested-With", "XMLHttpRequest");
			AddHeader("Host", "steamcommunity.com");
			AddHeader("Origin", "https://steamcommunity.com");
			AddHeader("Referer", "https://steamcommunity.com/login/home");
			AddHeader("Connection", "keep-alive");*/
		}

	public:
		DoLoginRequest(const char *username, const char *password, const char *timestamp) :
			HttpsPostRequest(STEAM_WEB_URL "/login/dologin")
		{
			flags = NLHRF_HTTP11 | NLHRF_SSL | NLHRF_NODUMP;

			InitData(username, password, timestamp);
		}

		DoLoginRequest(const char *username, const char *password, const char *timestamp, const char *guardId, const char *guardCode, const char *loginFriendlyName) :
			HttpsPostRequest(STEAM_WEB_URL "/login/dologin")
		{
			flags = NLHRF_HTTP11 | NLHRF_SSL | NLHRF_NODUMP;

			InitData(username, password, timestamp, guardId, guardCode, loginFriendlyName);
		}

		DoLoginRequest(const char *username, const char *password, const char *timestamp, const char *captchaId, const char *captchaText) :
			HttpsPostRequest(STEAM_WEB_URL "/login/dologin")
		{
			flags = NLHRF_HTTP11 | NLHRF_SSL | NLHRF_NODUMP;

			InitData(username, password, timestamp, "", "", "", captchaId, captchaText);
		}
	};

	class TransferRequest : public HttpsPostRequest
	{
	public:
		TransferRequest(const char *token, const char *steamId, const char *auth, const char *webcookie) :
			HttpsPostRequest(STEAM_WEB_URL "/login/transfer")
		{
			char data[256];
			mir_snprintf(data, SIZEOF(data), "token=%s&steamid=%s&auth=%s&webcookie=%s&remember_login=false",
				token,
				steamId,
				auth,
				webcookie);

			SetData(data, strlen(data));
		}
	};

	class RedirectToHomeRequest : public HttpsGetRequest
	{
	public:
		RedirectToHomeRequest(const char *token, const char *steamId) :
			HttpsGetRequest(STEAM_WEB_URL "/actions/RedirectToHome")
		{
			char cookie[256];
			mir_snprintf(cookie, SIZEOF(cookie), "steamLogin=%s||%s",
				steamId,
				token);

			AddHeader("Cookie", ptrA(mir_urlEncode(cookie)));
		}
	};
}


#endif //_STEAM_AUTHORIZATION_H_