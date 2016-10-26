#ifndef _STEAM_RSA_KEY_H_
#define _STEAM_RSA_KEY_H_

namespace SteamWebApi
{
	class GetRsaKeyRequest : public HttpsGetRequest
	{
	private:
		void AddTimeParameter(LPCSTR name, time_t value)
		{
			AddParameter("%s=%lld", name, value);
		}

	public:
		GetRsaKeyRequest(const char *username) :
			HttpsGetRequest(STEAM_WEB_URL "/login/getrsakey")
		{
			flags = NLHRF_HTTP11 | NLHRF_SSL | NLHRF_NODUMP;

			AddParameter("username", username);
			AddTimeParameter("donotcache", time(NULL));
		}
	};
}

#endif //_STEAM_RSA_KEY_H_