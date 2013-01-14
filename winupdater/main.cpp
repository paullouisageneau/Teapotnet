/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of TeapotNet.                                     *
 *                                                                       *
 *   TeapotNet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   TeapotNet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with TeapotNet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#define _WIN32_WINNT 0x0501

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <malloc.h>
#include <windows.h>
#include <wininet.h>
#include <shlwapi.h>
#include <psapi.h> 

#include "unzip.h"

#define BUFFERSIZE 2048

using namespace std;

int CALLBACK WinMain(	HINSTANCE hInstance,
			HINSTANCE hPrevInstance,
			LPSTR lpCmdLine,
			int nCmdShow)
{
	PCSTR szAgent = "TeapotNet-WinUpdater/0.1";
	HINTERNET hInternet = InternetOpen(szAgent,
				INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	
	PCSTR szServerName = "teapotnet.org";
	INTERNET_PORT nServerPort = INTERNET_DEFAULT_HTTP_PORT;
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
	PCSTR szObjectName = "/download/last";
	PCSTR szVersion = NULL;		// Use default.
	PCSTR szReferrer = NULL;	// No referrer.
	PCSTR *lpszAcceptTypes = NULL;	// We don't care
	DWORD dwOpenRequestFlags = INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP |
		INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS |
		INTERNET_FLAG_KEEP_CONNECTION |
		INTERNET_FLAG_NO_AUTH |
		INTERNET_FLAG_NO_AUTO_REDIRECT |
		INTERNET_FLAG_NO_COOKIES |
		INTERNET_FLAG_NO_UI |
		INTERNET_FLAG_RELOAD;
	DWORD dwOpenRequestContext = 0;
	HINTERNET hRequest = HttpOpenRequest(hConnect, szVerb, szObjectName, szVersion,
				szReferrer, lpszAcceptTypes,
				dwOpenRequestFlags, dwOpenRequestContext);
	
	if(!HttpSendRequest(hRequest, NULL, 0, NULL, 0))
	{
		// TODO: error
		return 1;
	}

	char szTempPath[MAX_PATH];
	char szTempFileName[MAX_PATH];
	if(!GetTempPath(MAX_PATH, szTempPath)) return 1;
	if(!GetTempFileName(szTempPath, "teapotnet", 0, szTempFileName)) return false;
	
	HANDLE hTempFile = CreateFile(szTempFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
	if(hTempFile == INVALID_HANDLE_VALUE)
	{
			// TODO: ERROR
			return 1;
	}
	
	char pBuffer[BUFFERSIZE];
	for(;;)
	{
		DWORD dwRead = 0;
		if(!InternetReadFile(hRequest, pBuffer, BUFFERSIZE, &dwRead))
		{
			// TODO: ERROR
			return 1;
		}
		
		if (dwRead == 0)
			break;	// End of File.

		DWORD dwWritten = 0;
		WriteFile(hTempFile, pBuffer, dwRead, &dwWritten, NULL);
		if(dwWritten != dwRead)
		{
				// TODO: ERROR
				return 1;
		}
	}
	
	CloseHandle(hTempFile);
	
	HZIP hZip = OpenZip(szTempFileName, 0);
	ZIPENTRY ze; 
	GetZipItem(hZip, -1, &ze);
	int nbItems = ze.index;
	for (int i=0; i<nbItems; ++i)
	{
		GetZipItem(hZip, i, &ze);	// fetch individual details
		UnzipItem(hZip, i, ze.name);	// e.g. the item's name.
	}
	CloseZip(hZip);
  
	DeleteFile(szTempPath);
	return 0;
}

