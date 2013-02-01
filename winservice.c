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
		if(strcmp(szAction, "install"))
		{
			char szPath[MAX_PATH]; 
			GetCurrentDirectory(MAX_PATH, szPath); 
			strcat(strDir,"\\winservice.exe"); 

			SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS); 
			SC_HANDLE hService = CreateService(hSCManager, "TeapotNet", "TeapotNet", 
				SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, 
				SERVICE_ERROR_NORMAL, szPath, NULL, NULL, NULL, NULL, NULL); 

			if(!hService) return 1;
			
			SERVICE_DESCRIPTION sd;
			sd.lpDescription = "TeapotNet service"; 
			ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &sd);
			CloseServiceHandle(hService);
			return 0;
		}
		 
		if(strcmp(szAction, "uninstall"))
		{
			SC_HANDLE hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS); 
			SC_HANDLE hService = OpenService(hSCManager,"TeapotNet",SERVICE_ALL_ACCESS); 
			DeleteService(hService); 
			CloseServiceHandle(hService); 
			CloseServiceHandle(hSCManager); 
			return 0;
		 }
		 
		 fprintf(stderr, "Unknown operation: %s\n", szAction);
		 return 1;
	}
  
	SERVICE_TABLE_ENTRY table[] = {{"TeapotNet",ServiceMain},{NULL,NULL}}; 
        StartServiceCtrlDispatcher(table); 
        return 0; 
} 

void WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv) 
{  
	char szDirectory[MAX_PATH];
	DWORD dwLength = GetModuleFileName(NULL, szDirectory, MAX_PATH];
	while(dwLength && szDirectory[dwLength] != '\\') --dwLength;
	szDirectory[dwLength] = '\0';
	if(!SetCurrentDirectory(szDirectory)) return;
	
	PROCESS_INFORMATION processInformation;
	BOOL result = CreateProcess("teapotnet.exe", "--nointerface", 
			NULL, NULL, 
			FALSE, 
			CREATE_NO_WINDOW|BELOW_NORMAL_PRIORITY_CLASS,
			NULL,
			szDirectory,
			NULL,
			&processInformation);

	if(!result) return;
	
	ZeroMemory(&serviceStatus, sizeof(SERVICE_STATUS)); 
	serviceStatus.dwServiceType = SERVICE_WIN32; 
	serviceStatus.dwCurrentState = SERVICE_START_PENDING; 
	serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP; 

	hServiceStatus = RegisterServiceCtrlHandler("TeapotNet", ServCtrlHandler); 

        serviceStatus.dwCurrentState = SERVICE_RUNNING; 
        serviceStatus.dwCheckPoint = 0; 
        serviceStatus.dwWaitHint = 0; 

        SetServiceStatus (hServiceStatus, &serviceStatus);
        bServiceStarted = TRUE;
	
        while(WaitForSingleObject(processInformation.hProcess, 2000) == WAIT_TIMEOUT)
	{	
		if(!bServiceStarted)
		{
			TerminateProcess(processInformation.hProcess, 0);
			break;
		}
        } 
        
        CloseHandle(processInformation.hThread);
        CloseHandle(processInformation.hProcess);
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
		  serviceStatus.dwWin32ExitCode = 0;
		  serviceStatus.dwCurrentState = SERVICE_STOPPED;
		  serviceStatus.dwCheckPoint = 0;
		  serviceStatus.dwWaitHint = 0;
		  SetServiceStatus(hServiceStatus,&serviceStatus); 
		  bServiceStarted = FALSE; 
		  break; 
	}
} 

