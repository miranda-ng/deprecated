#ifndef _STEAM_H_
#define _STEAM_H_

#include <stdarg.h>

namespace SteamWebApi
{
	#define STEAM_API_URL "https://api.steampowered.com"
	#define STEAM_WEB_URL "https://steamcommunity.com"

	#include "..\..\..\miranda-private-keys\Steam\api_key.h"

	class HttpRequest : protected NETLIBHTTPREQUEST, public MZeroedObject
	{
	protected:
		CMStringA url;

		HttpRequest()
		{
			cbSize = sizeof(NETLIBHTTPREQUEST);
		}

		HttpRequest(int type, LPCSTR urlFormat, va_list args)
		{
			//timeout = 0;
			requestType = type;
			flags = NLHRF_HTTP11 | NLHRF_NODUMPSEND | NLHRF_DUMPASTEXT;

			url.AppendFormatV(urlFormat, args);
			szUrl = url.GetBuffer();
		}

		void AddParameter(const char *fmt, ...)
		{
			va_list args;
			va_start(args, fmt);
			if (url.Find('?') == -1)
				url += '?';
			else
				url += '&';
			url.AppendFormatV(fmt, args);
			va_end(args);

			szUrl = url.GetBuffer();
		}

		void AddParameter(LPCSTR name, LPCSTR value)
		{
			AddParameter("%s=%s", name, value);
		}

		void AddHeader(LPCSTR szName, LPCSTR szValue)
		{
			headers = (NETLIBHTTPHEADER*)mir_realloc(headers, sizeof(NETLIBHTTPHEADER)*(headersCount + 1));
			headers[headersCount].szName = mir_strdup(szName);
			headers[headersCount].szValue = mir_strdup(szValue);
			headersCount++;
		}

		/*void AddParameter(LPCSTR name, LPCSTR value)
		{
		InternalAddParameter("%s=%s", name, value);
		}*/

		/*void AddParameter(LPCSTR name, time_t value)
		{
		AddParameter("%s=%lld", name, value);
		}*/

		/*void AddParameter(LPCSTR value)
		{
		InternalAddParameter("%s", value);
		}*/

		void SetData(const char *data, size_t size)
		{
			if (pData != NULL)
				mir_free(pData);

			dataLength = (int)size;
			pData = (char*)mir_alloc(size + 1);
			memcpy(pData, data, size);
			pData[size] = 0;
		}

	public:
		HttpRequest(int type, LPCSTR urlFormat, ...)
		{
			va_list args;
			va_start(args, urlFormat);
			this->HttpRequest::HttpRequest(type, urlFormat, args);
			va_end(args);
		}

		~HttpRequest()
		{
			for (int i = 0; i < headersCount; i++)
			{
				mir_free(headers[i].szName);
				mir_free(headers[i].szValue);
			}
			mir_free(headers);
			mir_free(pData);
		}

		NETLIBHTTPREQUEST * Send(HANDLE hConnection)
		{
			char message[1024];
			mir_snprintf(message, SIZEOF(message), "Send request to %s", szUrl);
			CallService(MS_NETLIB_LOG, (WPARAM)hConnection, (LPARAM)&message);

			return (NETLIBHTTPREQUEST*)CallService(MS_NETLIB_HTTPTRANSACTION, (WPARAM)hConnection, (LPARAM)this);
		}
	};

	class HttpGetRequest : public HttpRequest
	{
	public:
		HttpGetRequest(LPCSTR urlFormat, ...) : HttpRequest()
		{
			va_list args;
			va_start(args, urlFormat);
			this->HttpRequest::HttpRequest(REQUEST_GET, urlFormat, args);
			va_end(args);
		}
	};

	class HttpPostRequest : public HttpRequest
	{
	public:
		HttpPostRequest(LPCSTR urlFormat, ...) : HttpRequest()
		{
			va_list args;
			va_start(args, urlFormat);
			this->HttpRequest::HttpRequest(REQUEST_POST, urlFormat, args);
			va_end(args);

			AddHeader("Content-Type", "application/x-www-form-urlencoded; charset=utf-8");
		}
	};

	class HttpsRequest : public HttpRequest
	{
	protected:
		HttpsRequest() : HttpRequest() { }

	public:
		HttpsRequest(int type, LPCSTR urlFormat, ...) : HttpRequest()
		{
			va_list args;
			va_start(args, urlFormat);
			this->HttpRequest::HttpRequest(type, urlFormat, args);
			va_end(args);

			flags = NLHRF_HTTP11 | NLHRF_SSL | NLHRF_NODUMPSEND | NLHRF_DUMPASTEXT;
		}
	};

	class HttpsGetRequest : public HttpsRequest
	{
	public:
		HttpsGetRequest(LPCSTR urlFormat, ...) : HttpsRequest()
		{
			va_list args;
			va_start(args, urlFormat);
			this->HttpRequest::HttpRequest(REQUEST_GET, urlFormat, args);
			va_end(args);
		}
	};

	class HttpsPostRequest : public HttpsRequest
	{
	public:
		HttpsPostRequest(LPCSTR urlFormat, ...) : HttpsRequest()
		{
			va_list args;
			va_start(args, urlFormat);
			this->HttpRequest::HttpRequest(REQUEST_POST, urlFormat, args);
			va_end(args);

			AddHeader("Content-Type", "application/x-www-form-urlencoded; charset=utf-8");
		}
	};
}

#include "Steam\rsa_key.h"
#include "Steam\authorization.h"
#include "Steam\login.h"
#include "Steam\session.h"
#include "Steam\friend_list.h"
#include "Steam\pending.h"
#include "Steam\friend.h"
#include "Steam\poll.h"
#include "Steam\message.h"
#include "Steam\search.h"
#include "Steam\avatar.h"
#include "Steam\captcha.h"
#include "Steam\chat.h"

#endif //_STEAM_H_