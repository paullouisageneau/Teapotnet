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

#include "windows.h" 
#include "winsvc.h" 
#include "string.h"
#include "stdio.h"

SERVICE_STATUS serviceStatus; 
SERVICE_STATUS_HANDLE hServiceStatus; 
BOOL bServiceStarted = FALSE;

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv); 
void WINAPI ServCtrlHandler(DWORD Opcode); 


int main(int argc, char **argv)
{
	if(argc >= 2)
	{
		if(argc >= 3)
		{
			fprintf(stderr, "Too many arguments\n");
			return 1;
		}
		
		char *szAction = argv[1];
		if(!strcmp(szAction, "install"))
		{
			char szPath[MAX_PATH];
			GetModuleFileName(NULL, szPath, MAX_PATH);

			SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS); 
			SC_HANDLE hService = CreateService(hSCManager, "TeapotNet", "TeapotNet", 
				SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, 
				SERVICE_ERROR_NORMAL, szPath, NULL, NULL, NULL, NULL, NULL); 

			if(!hService)
			{
				fprintf(stderr, "Unable to install the service\n");
				return 1;
			}
			
			SERVICE_DESCRIPTION sd;
			sd.lpDescription = "TeapotNet service"; 
			ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &sd);
			CloseServiceHandle(hService);
			return 0;
		}
		
		if(!strcmp(szAction, "uninstall"))
		{
			SC_HANDLE hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS); 
			SC_HANDLE hService = OpenService(hSCManager,"TeapotNet",SERVICE_ALL_ACCESS); 
			if(!hService) return 1;
			DeleteService(hService); 
			CloseServiceHandle(hService); 
			CloseServiceHandle(hSCManager); 
			return 0;
		 }
		 
		if(!strcmp(szAction, "start"))
		{
			SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, GENERIC_EXECUTE) ; 
			SC_HANDLE hService = OpenService(hSCManager, "TeapotNet", SERVICE_START); 
			if(!hService) return 1;
			StartService(hService, 0, NULL);
			CloseServiceHandle(hService); 
			CloseServiceHandle(hSCManager); 
			return 0;
		}
		
		if(!strcmp(szAction, "stop"))
		{
			SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, GENERIC_EXECUTE) ; 
			SC_HANDLE hService = OpenService(hSCManager, "TeapotNet", SERVICE_STOP);
			if(!hService) return 1;
			SERVICE_STATUS_PROCESS ssp; 
			ControlService(hService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS) &ssp);
			CloseServiceHandle(hService); 
			CloseServiceHandle(hSCManager); 
			return 0;
		}
		
		fprintf(stderr, "Unknown operation: %s\n", szAction);
		return 1;
	}
	
	SERVICE_TABLE_ENTRY table[] = {{"TeapotNet", ServiceMain}, {NULL, NULL}}; 
        if(!StartServiceCtrlDispatcher(table))
	{
		DWORD error = GetLastError();
		if(error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
			fprintf(stderr, "Missing argument\n");
		return 1;
	}
	
        return 0; 
} 

void WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv) 
{  
	char szDirectory[MAX_PATH];
	DWORD dwLength = GetModuleFileName(NULL, szDirectory, MAX_PATH);
	while(dwLength && szDirectory[dwLength] != '\\') --dwLength;
	szDirectory[dwLength] = '\0';
	if(!SetCurrentDirectory(szDirectory)) return;
	
	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInfo;
	ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
	ZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));
	startupInfo.cb = sizeof(STARTUPINFO);
	
	BOOL result = CreateProcessA("teapotnet.exe", "--nointerface", 
			NULL, NULL,
			FALSE, 
			CREATE_NO_WINDOW|BELOW_NORMAL_PRIORITY_CLASS,
			NULL,
			szDirectory,
			&startupInfo,
			&processInfo);
	
	if(!result) return;
	
	ZeroMemory(&serviceStatus, sizeof(SERVICE_STATUS)); 
	serviceStatus.dwServiceType = SERVICE_WIN32; 
	serviceStatus.dwCurrentState = SERVICE_START_PENDING; 
	serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP; 

	hServiceStatus = RegisterServiceCtrlHandler("TeapotNet", ServCtrlHandler); 

	serviceStatus.dwWin32ExitCode = 0;
        serviceStatus.dwCurrentState = SERVICE_RUNNING;
        serviceStatus.dwCheckPoint = 0;
        serviceStatus.dwWaitHint = 0;

        SetServiceStatus (hServiceStatus, &serviceStatus);
        bServiceStarted = TRUE;
	
	CloseHandle(processInfo.hThread);
        while(WaitForSingleObject(processInfo.hProcess, 1000) == WAIT_TIMEOUT)
	{	
		if(!bServiceStarted)
		{
			TerminateProcess(processInfo.hThread, 0);
			break;
		}
        } 
        
        CloseHandle(processInfo.hProcess);
	
	serviceStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(hServiceStatus,&serviceStatus); 
} 

void WINAPI ServCtrlHandler(DWORD SCCode) 
{ 
	switch(SCCode) 
	{
	case SERVICE_CONTROL_CONTINUE: 
		  serviceStatus.dwCurrentState = SERVICE_RUNNING; 
		  SetServiceStatus(hServiceStatus,&serviceStatus); 
		  break; 

	case SERVICE_CONTROL_PAUSE: 
		  serviceStatus.dwCurrentState = SERVICE_PAUSED; 
		  SetServiceStatus(hServiceStatus,&serviceStatus); 
		  break; 

	case SERVICE_CONTROL_STOP:
		  bServiceStarted = FALSE; 
		  break; 
	}
} 

