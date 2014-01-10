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

#define _WIN32_WINNT 0x0501

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <windows.h>
#include <wincrypt.h>
#include <shlwapi.h>
#include <psapi.h> 
#include <malloc.h>
#include "unzip.h"

#define BUFFERSIZE 2048
#define USE_WINHTTP


#ifdef USE_WINHTTP

typedef LPVOID HINTERNET;
typedef HINTERNET * LPHINTERNET;
typedef WORD INTERNET_PORT;
typedef INTERNET_PORT * LPINTERNET_PORT;

#define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY	0
#define WINHTTP_ACCESS_TYPE_NO_PROXY		1
#define WINHTTP_FLAG_SECURE			0x00800000
#define WINHTTP_ERROR_BASE			12000
#define ERROR_WINHTTP_SECURE_FAILURE		(WINHTTP_ERROR_BASE + 175)

typedef HINTERNET (WINAPI *WINHTTPOPEN)(LPCWSTR pwszUserAgent, DWORD dwAccessType, LPCWSTR pwszProxyName, LPCWSTR pwszProxyBypass, DWORD dwFlags);
typedef HINTERNET (WINAPI *WINHTTPCONNECT)(HINTERNET hSession, LPCWSTR pswzServerName, INTERNET_PORT nServerPort, DWORD dwReserved);
typedef HINTERNET (WINAPI *WINHTTPOPENREQUEST)(HINTERNET hConnect, LPCWSTR pwszVerb, LPCWSTR pwszObjectName, LPCWSTR pwszVersion, LPCWSTR pwszReferrer, LPCWSTR *ppwszAcceptTypes, DWORD dwFlags);
typedef BOOL (WINAPI *WINHTTPSENDREQUEST)(HINTERNET hRequest, LPCWSTR pwszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength, DWORD dwTotalLength, DWORD_PTR dwContext);
typedef BOOL (WINAPI *WINHTTPRECEIVERESPONSE)(HINTERNET hRequest, LPVOID lpReserved);
typedef BOOL (WINAPI *WINHTTPQUERYHEADERS)(HINTERNET hRequest, DWORD dwInfoLevel, LPCWSTR pwszName, LPVOID lpBuffer, LPDWORD lpdwBufferLength, LPDWORD lpdwIndex);
typedef BOOL (WINAPI *WINHTTPQUERYDATAAVAILABLE)(HINTERNET hRequest, LPDWORD lpdwNumberOfBytesAvailable);
typedef BOOL (WINAPI *WINHTTPREADDATA)(HINTERNET hRequest, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead);
typedef BOOL (WINAPI *WINHTTPCLOSEHANDLE)(HINTERNET hInternet);

WINHTTPOPEN			WinHttpOpen;
WINHTTPCONNECT			WinHttpConnect;
WINHTTPOPENREQUEST		WinHttpOpenRequest;
WINHTTPSENDREQUEST		WinHttpSendRequest;
WINHTTPRECEIVERESPONSE		WinHttpReceiveResponse;
WINHTTPQUERYHEADERS		WinHttpQueryHeaders;
WINHTTPQUERYDATAAVAILABLE 	WinHttpQueryDataAvailable;
WINHTTPREADDATA			WinHttpReadData;
WINHTTPCLOSEHANDLE		WinHttpCloseHandle;

#else
#include <wininet.h>
#endif


int CALLBACK WinMain(   HINSTANCE hInstance,
                        HINSTANCE hPrevInstance,
                        LPSTR lpCmdLine,
                        int nCmdShow);
void Error(const char *szMessage);
int DoUpdate(void);

using namespace std;


int CALLBACK WinMain(	HINSTANCE hInstance,
			HINSTANCE hPrevInstance,
			LPSTR lpCmdLine,
			int nCmdShow)
{
#ifdef USE_WINHTTP
	HMODULE hWinHttp = LoadLibrary("winhttp.dll");
	if(!hWinHttp)
	{
		Error("Unable to find winhttp.dll");
		return 1;
	}
	
	WinHttpOpen			= (WINHTTPOPEN)			GetProcAddress(hWinHttp, "WinHttpOpen");
	WinHttpConnect			= (WINHTTPCONNECT)		GetProcAddress(hWinHttp, "WinHttpConnect");
	WinHttpOpenRequest 		= (WINHTTPOPENREQUEST)		GetProcAddress(hWinHttp, "WinHttpOpenRequest");
	WinHttpSendRequest 		= (WINHTTPSENDREQUEST)		GetProcAddress(hWinHttp, "WinHttpSendRequest");
	WinHttpReceiveResponse 		= (WINHTTPRECEIVERESPONSE)	GetProcAddress(hWinHttp, "WinHttpReceiveResponse");
	WinHttpQueryHeaders		= (WINHTTPQUERYHEADERS)		GetProcAddress(hWinHttp, "WinHttpQueryHeaders");
	WinHttpQueryDataAvailable	= (WINHTTPQUERYDATAAVAILABLE)	GetProcAddress(hWinHttp, "WinHttpQueryDataAvailable");
	WinHttpReadData			= (WINHTTPREADDATA)		GetProcAddress(hWinHttp, "WinHttpReadData");
	WinHttpCloseHandle		= (WINHTTPCLOSEHANDLE)		GetProcAddress(hWinHttp, "WinHttpCloseHandle");
#endif
	
	int ret = DoUpdate();
	ShellExecute(NULL, NULL, "teapotnet.exe", lpCmdLine, NULL, nCmdShow);

#ifdef USE_WINHTTP
	FreeLibrary(hWinHttp);
#endif
	
	return ret;
}

void Error(const char *szMessage)
{
	//MessageBox(NULL, szMessage, "Error during Teapotnet update process", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
	fprintf(stderr, "Error: %s\n", szMessage);
}

int DoUpdate(void)
{
#ifdef USE_WINHTTP
	HINTERNET hOpen = WinHttpOpen(L"Teapotnet-WinUpdater", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
	if(!hOpen)
	{
		Error("WinHttpOpen failed");
		return 2;
	}

	HINTERNET hConnect = WinHttpConnect(hOpen, L"www.teapotnet.org", 443, 0);
	if(!hConnect)
	{
		Error("WinHttpConnect failed");
		return 2;
	}
	
	LPCWSTR types[2];
	types[0] = L"application/zip";
	types[1] = NULL;

	// use flag WINHTTP_FLAG_SECURE to initiate SSL
	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/download/?release=win32&update=1", NULL, NULL, types, WINHTTP_FLAG_SECURE);
	if(!hRequest)
	{
		Error("WinHttpOpenRequest failed");
		return 2;
	}

	if(!WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0))
	{
		if(GetLastError() == ERROR_WINHTTP_SECURE_FAILURE) Error("Invalid SSL certificate");
		else Error("Connection failed");
		return 3;
	}
	
	if(!WinHttpReceiveResponse(hRequest, 0))
	{
		Error("No response from server");
		return 4;
	}
		
	char szTempPath[MAX_PATH];
	char szTempFileName[MAX_PATH];
	if(!GetTempPath(MAX_PATH, szTempPath)) return 5;
	if(!GetTempFileName(szTempPath, "teapotnet", 0, szTempFileName)) return 5;
	
	HANDLE hTempFile = CreateFile(szTempFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
	if(hTempFile == INVALID_HANDLE_VALUE)
	{
		Error("Unable to open temporary file");
		return 5;
	}
	
	char pBuffer[BUFFERSIZE];
	DWORD dwAvailable = 0;
	while(WinHttpQueryDataAvailable(hRequest, &dwAvailable) && dwAvailable)
	{
		if(dwAvailable > BUFFERSIZE) dwAvailable = BUFFERSIZE;
	  
		DWORD dwRead = 0;
		if(!WinHttpReadData(hRequest, pBuffer, dwAvailable, &dwRead))
		{
			Error("Error while downloading the update");
			return 6;
		}

		DWORD dwWritten = 0;
		WriteFile(hTempFile, pBuffer, dwRead, &dwWritten, NULL);
		if(dwWritten != dwRead)
		{
			Error("Error while writing to temporary file");
			return 5;
		}
	}
	
	CloseHandle(hTempFile);
	
	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hOpen);
  
#else
  
	/*
	PCSTR szCertFileName = "root.der";
	HANDLE hCertFile = CreateFile(szCertFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if(hCertFile != INVALID_HANDLE_VALUE)
	{
		DWORD dwCertSize = GetFileSize(hCertFile, NULL);
		char *pCertData = (char*)malloc(dwCertSize);
		
		DWORD dwRead = 0;
		ReadFile(hCertFile, pCertData, dwCertSize, &dwRead, NULL);
		CloseHandle(hCertFile);
		
		if(dwRead == dwCertSize)
		{
			PCCERT_CONTEXT pContext = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
							(BYTE*)pCertData, dwCertSize);
			HCERTSTORE hRootCertStore = CertOpenSystemStore((HCRYPTPROV)NULL, "ROOT");
			CertAddCertificateContextToStore(hRootCertStore,
							pContext,
							CERT_STORE_ADD_USE_EXISTING,
							NULL);
			CertCloseStore(hRootCertStore, 0);
			CertFreeCertificateContext(pContext);
		}
		
		free(pCertData);
	}
	*/
	
	HINTERNET hInternet = InternetOpen("Teapotnet-WinUpdater",
				INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	
	PCSTR szServerName = "teapotnet.org";
	//INTERNET_PORT nServerPort = INTERNET_DEFAULT_HTTP_PORT;
	INTERNET_PORT nServerPort = INTERNET_DEFAULT_HTTPS_PORT;
	
	PCSTR szUserName = NULL;
	PCSTR szPassword = NULL;
	DWORD dwConnectFlags = 0;
	DWORD dwConnectContext = 0;
	HINTERNET hConnect = InternetConnect(hInternet,
				szServerName, nServerPort,
				szUserName, szPassword,
				INTERNET_SERVICE_HTTP,
				dwConnectFlags, dwConnectContext);
	
	PCSTR szVerb = "GET";
	PCSTR szObjectName = "/download/?release=win32";
	PCSTR szVersion = NULL;		// Use default.
	PCSTR szReferrer = NULL;	// No referrer.
	PCSTR *lpszAcceptTypes = NULL;	// We don't care
	DWORD dwOpenRequestFlags = INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP |
		INTERNET_FLAG_KEEP_CONNECTION |
		INTERNET_FLAG_NO_AUTH |
		INTERNET_FLAG_NO_UI |
		INTERNET_FLAG_RELOAD;
	dwOpenRequestFlags|= INTERNET_FLAG_SECURE;			// SSL
	dwOpenRequestFlags|= INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;	// Ignore if expired
	// dwOpenRequestFlags|= INTERNET_FLAG_IGNORE_CERT_CN_INVALID;	// Ignore invalid name
	DWORD dwOpenRequestContext = 0;
	HINTERNET hRequest = HttpOpenRequest(hConnect, szVerb, szObjectName, szVersion,
				szReferrer, lpszAcceptTypes,
				dwOpenRequestFlags, dwOpenRequestContext);
	
	/*while(!HttpSendRequest(hRequest, NULL, 0, NULL, 0))
	{
		DWORD dwError = GetLastError();
		
		if(dwError == ERROR_INTERNET_INVALID_CA)
		{
			DWORD dwRet = InternetErrorDlg (GetDesktopWindow(),
					hRequest,
					ERROR_INTERNET_INVALID_CA,
					FLAGS_ERROR_UI_FILTER_FOR_ERRORS |
					FLAGS_ERROR_UI_FLAGS_GENERATE_DATA |
					FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS,
					NULL);
	
			if(dwRet == ERROR_SUCCESS) continue;
		}
		else {
			Error("Invalid SSL certificate");
		}
		
		return 1;
	}*/

	if(!HttpSendRequest(hRequest, NULL, 0, NULL, 0))
	{
		if(dwError == ERROR_INTERNET_INVALID_CA) Error("Invalid SSL certificate");
		else Error("Connection failed");
		return 3;
	}
	
	char szTempPath[MAX_PATH];
	char szTempFileName[MAX_PATH];
	if(!GetTempPath(MAX_PATH, szTempPath)) return 5;
	if(!GetTempFileName(szTempPath, "teapotnet", 0, szTempFileName)) return 5;
	
	HANDLE hTempFile = CreateFile(szTempFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
	if(hTempFile == INVALID_HANDLE_VALUE)
	{
			Error("Unable to open temporary file");
			return 5;
	}
	
	char pBuffer[BUFFERSIZE];
	for(;;)
	{
		DWORD dwRead = 0;
		if(!InternetReadFile(hRequest, pBuffer, BUFFERSIZE, &dwRead))
		{
			Error("Error while downloading the update");
			return 6;
		}
		
		if (dwRead == 0)
			break;	// End of File.

		DWORD dwWritten = 0;
		WriteFile(hTempFile, pBuffer, dwRead, &dwWritten, NULL);
		if(dwWritten != dwRead)
		{
				Error("Error while writing to temporary file");
				return 6;
		}
	}
	
	CloseHandle(hTempFile);
#endif
	
	// Unzip files
	HZIP hZip = OpenZip(szTempFileName, 0);
	ZIPENTRY ze; 
	GetZipItem(hZip, -1, &ze);
	int nbItems = ze.index;
	for (int i=0; i<nbItems; ++i)
	{
		GetZipItem(hZip, i, &ze);

		const char *name = ze.name;
		if(!strcmp(name,"teapotnet/")) continue;
		if(!strncmp(name,"teapotnet/",10)) name+= 10; 

		UnzipItem(hZip, i, name);
	}
	CloseZip(hZip);

	DeleteFile(szTempPath);
	return 0;
}

