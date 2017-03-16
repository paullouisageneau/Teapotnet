/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Plateform.                                     *
 *                                                                       *
 *   Plateform is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Plateform is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Plateform.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "pla/proxy.hpp"
#include "pla/exception.hpp"
#include "pla/http.hpp"

namespace pla
{

String Proxy::HttpProxy = "";

bool Proxy::GetProxyForUrl(const String &url, Address &addr)
{
	if(HttpProxy == "none")
		return false;

	if(!HttpProxy.empty() && HttpProxy != "auto")
	{
		addr.fromString(HttpProxy);
		return true;
	}

#ifdef WINDOWS
	typedef LPVOID HINTERNET;

	typedef struct {
		DWORD  dwAccessType;
		LPWSTR lpszProxy;
		LPWSTR lpszProxyBypass;
	} WINHTTP_PROXY_INFO;

	typedef struct {
		BOOL   fAutoDetect;
		LPWSTR lpszAutoConfigUrl;
		LPWSTR lpszProxy;
		LPWSTR lpszProxyBypass;
	} WINHTTP_CURRENT_USER_IE_PROXY_CONFIG;

	typedef struct {
		DWORD   dwFlags;
		DWORD   dwAutoDetectFlags;
		LPCWSTR lpszAutoConfigUrl;
		LPVOID  lpvReserved;
		DWORD   dwReserved;
		BOOL    fAutoLogonIfChallenged;
	} WINHTTP_AUTOPROXY_OPTIONS;

	#define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY	0
	#define WINHTTP_ACCESS_TYPE_NO_PROXY		1
	#define WINHTTP_ACCESS_TYPE_NAMED_PROXY		3
	#define WINHTTP_AUTOPROXY_AUTO_DETECT		0x00000001
	#define WINHTTP_AUTOPROXY_CONFIG_URL		0x00000002
	#define WINHTTP_AUTO_DETECT_TYPE_DHCP		0x00000001
	#define WINHTTP_AUTO_DETECT_TYPE_DNS_A		0x00000002
	#define WINHTTP_ERROR_BASE                      12000
	#define ERROR_WINHTTP_LOGIN_FAILURE             (WINHTTP_ERROR_BASE + 15)
	#define ERROR_WINHTTP_AUTODETECTION_FAILED      (WINHTTP_ERROR_BASE + 180)

	typedef HINTERNET (WINAPI *WINHTTPOPEN)(LPCWSTR pwszUserAgent, DWORD dwAccessType, LPCWSTR pwszProxyName, LPCWSTR pwszProxyBypass, DWORD dwFlags);
	typedef BOOL (WINAPI *WINHTTPGETPROXYFORURL)(HINTERNET hSession, LPCWSTR lpcwszUrl, WINHTTP_AUTOPROXY_OPTIONS *pAutoProxyOptions, WINHTTP_PROXY_INFO *pProxyInfo);
	typedef BOOL (WINAPI *WINHTTPGETDEFAULTPROXYCONFIGURATION)(WINHTTP_PROXY_INFO *pProxyInfo);
	typedef BOOL (WINAPI *WINHTTPGETIEPROXYCONFIGFORCURRENTUSER)(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG *pProxyConfig);
	typedef BOOL (WINAPI *WINHTTPCLOSEHANDLE)(HINTERNET hInternet);

	static HMODULE hWinHttp = NULL;
	static WINHTTPOPEN WinHttpOpen;
	static WINHTTPGETPROXYFORURL WinHttpGetProxyForUrl;
	static WINHTTPGETDEFAULTPROXYCONFIGURATION WinHttpGetDefaultProxyConfiguration;
	static WINHTTPGETIEPROXYCONFIGFORCURRENTUSER WinHttpGetIEProxyConfigForCurrentUser;
	static WINHTTPCLOSEHANDLE WinHttpCloseHandle;

	if(!hWinHttp)
	{
		hWinHttp = LoadLibrary("winhttp.dll");
		if(!hWinHttp) return false;

		WinHttpOpen = (WINHTTPOPEN) GetProcAddress(hWinHttp, "WinHttpOpen");
		WinHttpGetProxyForUrl = (WINHTTPGETPROXYFORURL) GetProcAddress(hWinHttp, "WinHttpGetProxyForUrl");
		WinHttpGetDefaultProxyConfiguration = (WINHTTPGETDEFAULTPROXYCONFIGURATION) GetProcAddress(hWinHttp, "WinHttpGetDefaultProxyConfiguration");
		WinHttpGetIEProxyConfigForCurrentUser = (WINHTTPGETIEPROXYCONFIGFORCURRENTUSER) GetProcAddress(hWinHttp, "WinHttpGetIEProxyConfigForCurrentUser");
		WinHttpCloseHandle = (WINHTTPCLOSEHANDLE) GetProcAddress(hWinHttp, "WinHttpCloseHandle");
	}

	bool hasProxy = false;
	std::string ua(Http::UserAgent);
	std::wstring wua(ua.begin(), ua.end());
	HINTERNET hSession = WinHttpOpen(wua.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
	if(hSession)
	{
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG userProxyConfig;
		bool hasUserProxyConfig = (WinHttpGetIEProxyConfigForCurrentUser(&userProxyConfig) == TRUE);

		if(hasUserProxyConfig && !userProxyConfig.fAutoDetect && userProxyConfig.lpszProxy)
		{
			hasProxy = ParseWinHttpProxy(userProxyConfig.lpszProxy, addr);
		}
		else {
			WINHTTP_AUTOPROXY_OPTIONS autoProxyOptions;
			std::memset(&autoProxyOptions, 0, sizeof(autoProxyOptions));
			autoProxyOptions.fAutoLogonIfChallenged = TRUE;

			if(hasUserProxyConfig) autoProxyOptions.lpszAutoConfigUrl = userProxyConfig.lpszAutoConfigUrl;
			else autoProxyOptions.lpszAutoConfigUrl = NULL;

			if(autoProxyOptions.lpszAutoConfigUrl)
			{
				autoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
				autoProxyOptions.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP|WINHTTP_AUTO_DETECT_TYPE_DNS_A;
			}
			else {
				autoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
				autoProxyOptions.dwAutoDetectFlags = 0;
			}

			WINHTTP_PROXY_INFO proxyInfo;
			std::wstring wurl(url.begin(), url.end());
			if(WinHttpGetProxyForUrl(hSession, wurl.c_str(), &autoProxyOptions, &proxyInfo))
			{
				if(proxyInfo.lpszProxy && proxyInfo.dwAccessType == WINHTTP_ACCESS_TYPE_NAMED_PROXY)
					hasProxy = ParseWinHttpProxy(proxyInfo.lpszProxy, addr);

				if(proxyInfo.lpszProxy)         GlobalFree(proxyInfo.lpszProxy);
				if(proxyInfo.lpszProxyBypass)   GlobalFree(proxyInfo.lpszProxyBypass);
			}
			else if(WinHttpGetDefaultProxyConfiguration(&proxyInfo))
			{
				if(proxyInfo.lpszProxy && proxyInfo.dwAccessType == WINHTTP_ACCESS_TYPE_NAMED_PROXY)
					hasProxy = ParseWinHttpProxy(proxyInfo.lpszProxy, addr);

				if(proxyInfo.lpszProxy) GlobalFree(proxyInfo.lpszProxy);
				if(proxyInfo.lpszProxyBypass) GlobalFree(proxyInfo.lpszProxyBypass);
			}
		}

		if(hasUserProxyConfig)
		{
			if(userProxyConfig.lpszProxy) GlobalFree(userProxyConfig.lpszProxy);
			if(userProxyConfig.lpszProxyBypass) GlobalFree(userProxyConfig.lpszProxyBypass);
			if(userProxyConfig.lpszAutoConfigUrl) GlobalFree(userProxyConfig.lpszAutoConfigUrl);
		}

		WinHttpCloseHandle(hSession);
	}

	return hasProxy;
#else
	// TODO: Get proxy on Linux and MacOS
	return false;
#endif
}

bool Proxy::HasProxyForUrl(const String &url)
{
	Address dummy;
	return GetProxyForUrl(url, dummy);
}

#ifdef WINDOWS

bool Proxy::ParseWinHttpProxy(LPWSTR lpszProxy, Address &addr)
{
	Assert(lpszProxy);
	String proxy(lpszProxy, lpszProxy + std::char_traits<wchar_t>::length(lpszProxy));
	proxy.toLower();
	size_t pos = proxy.find("http://");
	if(pos != String::NotFound) proxy = proxy.substr(pos + 7);
	else {
		pos = proxy.find("http=");
		if(pos != String::NotFound) proxy = proxy.substr(pos + 5);
	}
	proxy.cut('/'); proxy.cut(','); proxy.cut(' ');

	if(!proxy.empty())
	{
		try {
			addr.fromString(proxy);
			return true;
		}
		catch(...)
		{
			LogDebug("Proxy::ParseWinHtppProxy", "Got invalid proxy address: " + proxy);
		}
	}

	return false;
}

#endif

}
