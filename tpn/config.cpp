/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "tpn/config.h"
#include "tpn/file.h"
#include "tpn/core.h"
#include "tpn/portmapping.h"
#include "tpn/httptunnel.h"	// for user agent

namespace tpn
{

StringMap Config::Params;
Mutex Config::ParamsMutex;
bool Config::UpdateAvailableFlag = false;  

String Config::Get(const String &key)
{
	ParamsMutex.lock();
	String value;
	if(Params.get(key, value))
	{
	 	 ParamsMutex.unlock();
		return value;
	}
	
	ParamsMutex.unlock();
	//throw Exception("Config: no entry for \""+key+"\"");
	return "";
}

void Config::Put(const String &key, const String &value)
{
  	ParamsMutex.lock();
	Params.insert(key, value);
	ParamsMutex.unlock();
}

void Config::Default(const String &key, const String &value)
{
	ParamsMutex.lock();
	if(!Params.contains(key)) Params.insert(key, value);
	ParamsMutex.unlock();
}

void Config::Load(const String &filename)
{
	ParamsMutex.lock();
	try {
		File file(filename, File::Read);
		file.read(Params);
		file.close();
	}
	catch(const Exception &e) 
	{
		LogError("Config", String("Unable to load config: ") + e.what());
	}
	ParamsMutex.unlock();
}

void Config::Save(const String &filename)
{
	ParamsMutex.lock();
	try {
		File file(filename, File::Truncate);
		file.write(Params);
		file.close();
	}
	catch(...)
	{
		ParamsMutex.unlock();
		throw;
	}
	ParamsMutex.unlock();
}

void Config::GetExternalAddresses(List<Address> &list)
{
	list.clear();
	
	String externalAddress = Config::Get("external_address");
	if(!externalAddress.empty() && externalAddress != "auto")
	{
		Address addr;
		if(externalAddress.contains(':')) addr.set(externalAddress);
		else {
			String port = Config::Get("port");
			String externalPort = Config::Get("external_port");
                	if(!externalPort.empty() && externalPort != "auto") port = externalPort;
			addr.set(externalAddress, port);
		}
		list.push_back(addr);
	}

	List<Address> tmp;
	Core::Instance->getAddresses(tmp);
		
	for(List<Address>::const_iterator it = tmp.begin();
		it != tmp.end();
		++it)
	{
		const Address &addr = *it;
		if(addr.addrFamily() == AF_INET)
		{
			String host = PortMapping::Instance->getExternalHost();
			if(!host.empty()) 
			{
				uint16_t port;
				PortMapping::Instance->get(PortMapping::TCP, addr.port(), port);
				list.push_back(Address(host, port));
			}
		}
			
		String host = addr.host();
		if(host != "127.0.0.1" && host != "::1"
			&& std::find(list.begin(), list.end(), addr) == list.end())
		{
			list.push_back(addr);
		}
	}
}

bool Config::CheckUpdate(void)
{
	String release;

#if defined(WINDOWS)
        release = "win32";
#elif defined(MACOSX)
        release = "osx";
#else
	release = "src";
#endif

        try {
                LogInfo("Config::CheckUpdate", "Looking for updates...");
                String url = String(DOWNLOADURL) + "?version&release=" + release + "&current=" + APPVERSION;

                String content;
                int result = Http::Get(url, &content);
                if(result != 200)
                        throw Exception("HTTP error code " + String::number(result));

                unsigned lastVersion = content.trimmed().dottedToInt();
                unsigned appVersion = String(APPVERSION).dottedToInt();

                Assert(appVersion != 0);
                if(lastVersion > appVersion)
		{
			UpdateAvailableFlag = true;
			return true;
		}
        }
        catch(const Exception &e)
        {
                LogWarn("Config::CheckUpdate", String("Unable to look for updates: ") + e.what());
        }

        return false;
}

bool Config::IsUpdateAvailable(void)
{
	return UpdateAvailableFlag;
}

bool Config::GetProxyForUrl(const String &url, Address &addr)
{
	String proxy = Get("http_proxy").trimmed();
	
	if(proxy == "none")
	{
		return false;
	}
	
        if(!proxy.empty() && proxy != "auto")
        {
                addr.fromString(proxy);
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
	static WINHTTPOPEN				WinHttpOpen;
	static WINHTTPGETPROXYFORURL			WinHttpGetProxyForUrl;
	static WINHTTPGETDEFAULTPROXYCONFIGURATION	WinHttpGetDefaultProxyConfiguration;
	static WINHTTPGETIEPROXYCONFIGFORCURRENTUSER	WinHttpGetIEProxyConfigForCurrentUser;
	static WINHTTPCLOSEHANDLE			WinHttpCloseHandle;
	
	if(!hWinHttp)
	{
		hWinHttp = LoadLibrary("winhttp.dll");
		if(!hWinHttp) return false;
		
		WinHttpOpen				= (WINHTTPOPEN)					GetProcAddress(hWinHttp, "WinHttpOpen");
		WinHttpGetProxyForUrl			= (WINHTTPGETPROXYFORURL)			GetProcAddress(hWinHttp, "WinHttpGetProxyForUrl");
		WinHttpGetDefaultProxyConfiguration	= (WINHTTPGETDEFAULTPROXYCONFIGURATION)		GetProcAddress(hWinHttp, "WinHttpGetDefaultProxyConfiguration");
		WinHttpGetIEProxyConfigForCurrentUser	= (WINHTTPGETIEPROXYCONFIGFORCURRENTUSER)	GetProcAddress(hWinHttp, "WinHttpGetIEProxyConfigForCurrentUser");
		WinHttpCloseHandle			= (WINHTTPCLOSEHANDLE)				GetProcAddress(hWinHttp, "WinHttpCloseHandle");
	}
	
	bool hasProxy = false;
	std::string ua(HttpTunnel::UserAgent);
	std::wstring wua(ua.begin(), ua.end());
	HINTERNET hSession = WinHttpOpen(wua.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
	if(hSession)
	{
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG userProxyConfig;
		bool hasUserProxyConfig = (WinHttpGetIEProxyConfigForCurrentUser(&userProxyConfig) == TRUE);
		
		if(hasUserProxyConfig && !userProxyConfig.fAutoDetect && userProxyConfig.lpszProxy)
		{
			hasProxy = parseWinHttpProxy(userProxyConfig.lpszProxy, addr);
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
					hasProxy = parseWinHttpProxy(proxyInfo.lpszProxy, addr);

				if(proxyInfo.lpszProxy)         GlobalFree(proxyInfo.lpszProxy);
				if(proxyInfo.lpszProxyBypass)   GlobalFree(proxyInfo.lpszProxyBypass);
			}
			else if(WinHttpGetDefaultProxyConfiguration(&proxyInfo))
			{
				if(proxyInfo.lpszProxy && proxyInfo.dwAccessType == WINHTTP_ACCESS_TYPE_NAMED_PROXY)
					hasProxy = parseWinHttpProxy(proxyInfo.lpszProxy, addr);

				if(proxyInfo.lpszProxy)         GlobalFree(proxyInfo.lpszProxy);
				if(proxyInfo.lpszProxyBypass)   GlobalFree(proxyInfo.lpszProxyBypass);
			}
		}
		
		if(hasUserProxyConfig)
		{
			if(userProxyConfig.lpszProxy)		GlobalFree(userProxyConfig.lpszProxy);
			if(userProxyConfig.lpszProxyBypass)	GlobalFree(userProxyConfig.lpszProxyBypass);
			if(userProxyConfig.lpszAutoConfigUrl)	GlobalFree(userProxyConfig.lpszAutoConfigUrl);
		}

		WinHttpCloseHandle(hSession);
	}
	
	return hasProxy;
#else
	// TODO
	return false;
#endif
}

#ifdef WINDOWS

bool Config::parseWinHttpProxy(LPWSTR lpszProxy, Address &addr)
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
			LogDebug("Config::parseWinHtppProxy", "Got invalid proxy address: " + proxy);
		}
	}

	return false;
}

#endif

}

