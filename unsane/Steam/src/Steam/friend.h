#ifndef _STEAM_FRIEND_H_
#define _STEAM_FRIEND_H_

namespace SteamWebApi
{
	class GetPlayerSummariesRequest : public HttpsGetRequest
	{
	public:
		GetPlayerSummariesRequest(const char *steamIds) :
			HttpsGetRequest(STEAM_API_URL "/ISteamUser/GetPlayerSummaries/v0002")
		{
			AddParameter("key", STEAM_API_KEY);
			AddParameter("steamids", steamIds);
		}
	};

	class BlockFriendRequest : public HttpsPostRequest
	{
	public:
		BlockFriendRequest(const char *token, const char *sessionId, const char *steamId, const char *who) :
			HttpsPostRequest(STEAM_WEB_URL "/profiles/%s/friends", steamId)
		{
			char login[MAX_PATH];
			mir_snprintf(login, SIZEOF(login), "%s||oauth:%s", steamId, token);

			char cookie[MAX_PATH];
			mir_snprintf(cookie, SIZEOF(cookie), "steamLogin=%s;sessionid=%s;forceMobile=1", login, sessionId);

			/*char url[MAX_PATH];
			mir_snprintf(url, SIZEOF(url), STEAM_COM_URL "/profiles/%s/friends", steamId);
			this->url = url;*/

			char data[128];
			mir_snprintf(data, SIZEOF(data),
				"sessionID=%s&action=ignore&friends[%s]=1",
				sessionId,
				who);

			SetData(data, strlen(data));
			AddHeader("Cookie", cookie);
		}
	};

	class UnblockFriendRequest : public HttpsPostRequest
	{
	public:
		UnblockFriendRequest(const char *token, const char *sessionId, const char *steamId, const char *who) :
			HttpsPostRequest(STEAM_WEB_URL "/profiles/%s/friends", steamId)
		{
			char login[MAX_PATH];
			mir_snprintf(login, SIZEOF(login), "%s||oauth:%s", steamId, token);

			char cookie[MAX_PATH];
			mir_snprintf(cookie, SIZEOF(cookie), "steamLogin=%s;sessionid=%s;forceMobile=1", login, sessionId);

			/*char url[MAX_PATH];
			mir_snprintf(url, SIZEOF(url), STEAM_COM_URL "/profiles/%s/friends", steamId);
			this->url = url;*/

			char data[128];
			mir_snprintf(data, SIZEOF(data),
				"sessionID=%s&action=unignore&friends[%s]=1",
				sessionId,
				who);

			SetData(data, strlen(data));
			AddHeader("Cookie", cookie);
		}
	};
}

#endif //_STEAM_FRIEND_H_