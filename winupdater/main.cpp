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

#include <windows.h>
#include <wininet.h>
#include <wincrypt.h>
#include <shlwapi.h>
#include <psapi.h> 
#include <malloc.h>

#include "unzip.h"

#define BUFFERSIZE 2048

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
	int ret = DoUpdate();
	ShellExecute(NULL, NULL, "teapotnet.exe", lpCmdLine, NULL, nCmdShow);
	return ret;
}

void Error(const char *szMessage)
{
	//MessageBox(NULL, szMessage, "Error during TeapotNet update process", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
	fprintf(stderr, "Error: %s\n", szMessage);
}

int DoUpdate(void)
{
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

	PCSTR szAgent = "TeapotNet-WinUpdater/0.1";	
	HINTERNET hInternet = InternetOpen(szAgent,
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
		Error("Invalid SSL certificate");
		return 1;
	}
	
	char szTempPath[MAX_PATH];
	char szTempFileName[MAX_PATH];
	if(!GetTempPath(MAX_PATH, szTempPath)) return 1;
	if(!GetTempFileName(szTempPath, "teapotnet", 0, szTempFileName)) return false;
	
	HANDLE hTempFile = CreateFile(szTempFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
	if(hTempFile == INVALID_HANDLE_VALUE)
	{
			Error("Unable to open a temporary file");
			return 1;
	}
	
	char pBuffer[BUFFERSIZE];
	for(;;)
	{
		DWORD dwRead = 0;
		if(!InternetReadFile(hRequest, pBuffer, BUFFERSIZE, &dwRead))
		{
			Error("Error while downloading the update");
			return 1;
		}
		
		if (dwRead == 0)
			break;	// End of File.

		DWORD dwWritten = 0;
		WriteFile(hTempFile, pBuffer, dwRead, &dwWritten, NULL);
		if(dwWritten != dwRead)
		{
				Error("Error while writing to a temporary file");
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

