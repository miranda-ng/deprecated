#ifndef _STEAM_CHAT_H_
#define _STEAM_CHAT_H_

namespace SteamWebApi
{
	class GetChatPageRequest : public HttpGetRequest
	{
	public:
		GetChatPageRequest(const char *token, const char *steamId, const char *sessionId) :
			HttpGetRequest(STEAM_WEB_URL "/chat")
		{
			char login[MAX_PATH];
			mir_snprintf(login, SIZEOF(login), "%s%s%s", steamId, "%7C%7C", token);

			char cookie[MAX_PATH];
			mir_snprintf(cookie, SIZEOF(cookie), "steamLogin=%s; sessionid=%s; forceMobile=0", login, sessionId);

			AddHeader("Cookie", cookie);
		}
	};
}

#endif //_STEAM_CHAT_H_