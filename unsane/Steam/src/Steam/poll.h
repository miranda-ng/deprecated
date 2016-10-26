#ifndef _STEAM_POLL_H_
#define _STEAM_POLL_H_

namespace SteamWebApi
{
	class PollRequest : public HttpsPostRequest
	{
	private:
		void AddUintParameter(LPCSTR name, UINT32 value)
		{
			AddParameter("%s=%u", name, value);
		}

	public:
		PollRequest(HANDLE hConnection, const char *token, const char *umqId, UINT32 messageId) :
			HttpsPostRequest(STEAM_API_URL "/ISteamWebUserPresenceOAuth/Poll/v0001")
		{
			timeout = 30000;
			flags |= NLHRF_PERSISTENT;

			nlc = hConnection;
			
			char data[256];
			mir_snprintf(data, SIZEOF(data), "access_token=%s&umqid=%s&message=%u", token, umqId, messageId);

			SetData(data, strlen(data));
			AddHeader("Connection", "keep-alive");
		}
	};
}

#endif //_STEAM_POLL_H_