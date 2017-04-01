
!define APPNAME "Teapotnet"
!define AUTHOR "Paul-Louis Ageneau"
!define DESCRIPTION "Teapotnet is an Easy but Advanced Privacy-Oriented Transmission Network"

!define HELPURL "http://teapotnet.org/help/" # "Support Information" link
!define ABOUTURL "http://teapotnet.org/" # "Publisher" link
 
RequestExecutionLevel admin ;Require admin rights on NT6+ (When UAC is turned on)
 
InstallDir "$PROGRAMFILES\${APPNAME}"
 
# rtf or txt file - remember if it is txt, it must be in the DOS text format (\r\n)
LicenseData "LICENSE.txt"
# This will be in the installer/uninstaller's title bar
Name "${APPNAME}"
Icon "teapotnet.ico"
outFile "teapotnet_installer.exe"
 
!include LogicLib.nsh
 
# Just three pages - license agreement, install location, and installation
page license
page directory
Page instfiles
 
!macro VerifyUserIsAdmin
UserInfo::GetAccountType
pop $0
${If} $0 != "admin" ;Require admin rights on NT4+
        MessageBox mb_iconstop "Administrator rights required!"
        setErrorLevel 740 ;ERROR_ELEVATION_REQUIRED
        Quit
${EndIf}
!macroend
 
function .onInit
	setShellVarContext all
	!insertmacro VerifyUserIsAdmin
functionEnd
 
section "install"
	# Stop the process
	ExecWait 'taskkill /F /IM teapotnet.exe'
	Sleep 1000
	
	# Files for the install directory - to build the installer, these should be in the same directory as the install script (this file)
	setOutPath $INSTDIR
	# Files added here should be removed by the uninstaller (see section "uninstall")
	File /R "static"
	File "teapotnet.exe"
	File "teapotnet.ico"
	File "winservice.exe"
	File "winupdater.exe"
	# Add any other files for the install directory (license files, app data, etc) here
 
	# Uninstaller - See function un.onInit and section "uninstall" for configuration
	WriteUninstaller "$INSTDIR\uninstall.exe"
 
 	# Desktop
 	CreateShortCut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\teapotnet.exe" "" "$INSTDIR\teapotnet.ico"
 
	# Start Menu
	CreateDirectory "$SMPROGRAMS\${APPNAME}"
	CreateShortCut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\teapotnet.exe" "" "$INSTDIR\teapotnet.ico"
 
	# Registry information for add/remove programs
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "QuietUninstallString" "$\"$INSTDIR\uninstall.exe$\" /S"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "InstallLocation" "$\"$INSTDIR$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon" "$\"$INSTDIR\teapotnet.ico$\""
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher" "${AUTHOR}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "HelpLink" "${HELPURL}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "URLInfoAbout" "${ABOUTURL}"
	# There is no option for modifying or repairing the install
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoRepair" 1

	# Registry information for startup
	#WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "Teapotnet" "$\"$INSTDIR\teapotnet.exe$\" -nointerface"

	# Install and start the service
	setOutPath $INSTDIR
	ExecWait '"$INSTDIR\winservice.exe" install'
        ExecWait '"$INSTDIR\winservice.exe" start'

sectionEnd
 
# Uninstaller
 
function un.onInit
	setShellVarContext all
 
	#Verify the uninstaller - last chance to back out
	MessageBox MB_OKCANCEL "Uninstall ${APPNAME} ?" IDOK next
		Abort
	next:
	!insertmacro VerifyUserIsAdmin
functionEnd
 
section "uninstall"

	# Remove startup information from the registry  
        #DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "Teapotnet"

	# Stop and uninstall the service
	ExecWait '"$INSTDIR\winservice.exe" stop'
	ExecWait '"$INSTDIR\winservice.exe" uninstall'
	ExecWait 'taskkill /F /IM teapotnet.exe'
	Sleep 1000
	
	# Remove desktop shortcut
	Delete "$DESKTOP\${APPNAME}.lnk"

	# Remove Start Menu launcher
	Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
	# Try to remove the Start Menu folder
	RmDir "$SMPROGRAMS\${APPNAME}"
 
	# Remove files
	Delete $INSTDIR\teapotnet.exe
	Delete $INSTDIR\teapotnet.ico
	Delete $INSTDIR\winservice.exe
	Delete $INSTDIR\winupdater.exe 
	RmDir /R $INSTDIR\static

	# Remove dynamic files
	RmDir /R $INSTDIR\temp
	RmDir /R $INSTDIR\cache
	RmDir /R $INSTDIR\shared
	RmDir /R $INSTDIR\profiles
	
	# Always delete uninstaller as the last action
	Delete $INSTDIR\uninstall.exe
 
	# Remove the install directory
	RmDir /R $INSTDIR
 
	# Remove uninstaller information from the registry
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

sectionEnd

