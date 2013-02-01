
!define APPNAME "TeapotNet"
!define AUTHOR "Paul-Louis Ageneau"
!define DESCRIPTION "TeapotNet is an Easy but Advanced Privacy-Oriented Transmission Network"

!define HELPURL "http://teapotnet.org/help/" # "Support Information" link
!define ABOUTURL "http://teapotnet.org/" # "Publisher" link
 
RequestExecutionLevel admin ;Require admin rights on NT6+ (When UAC is turned on)
 
InstallDir "$PROGRAMFILES\${APPNAME}"
 
# rtf or txt file - remember if it is txt, it must be in the DOS text format (\r\n)
LicenseData "license.txt"
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
        messageBox mb_iconstop "Administrator rights required!"
        setErrorLevel 740 ;ERROR_ELEVATION_REQUIRED
        quit
${EndIf}
!macroend
 
function .onInit
	setShellVarContext all
	!insertmacro VerifyUserIsAdmin
functionEnd
 
section "install"
	# Files for the install directory - to build the installer, these should be in the same directory as the install script (this file)
	setOutPath $INSTDIR
	# Files added here should be removed by the uninstaller (see section "uninstall")
	file /R "static"
	file "teapotnet.exe"
	file "teapotnet.ico"
	file "winservice.exe"
	file "winupdater.exe"
	# Add any other files for the install directory (license files, app data, etc) here
 
	# Uninstaller - See function un.onInit and section "uninstall" for configuration
	writeUninstaller "$INSTDIR\uninstall.exe"
 
 	# Desktop
 	createShortCut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\teapotnet.exe" "" "$INSTDIR\teapotnet.ico"
 
	# Start Menu
	createDirectory "$SMPROGRAMS\${APPNAME}"
	createShortCut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\teapotnet.exe" "" "$INSTDIR\teapotnet.ico"
 
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
	#WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "TeapotNet" "$\"$INSTDIR\teapotnet.exe$\" -nointerface"

	# Install and start the service
	Exec '$INSTDIR\winservice.exe" install'
        Exec '$INSTDIR\winservice.exe" start'

sectionEnd
 
# Uninstaller
 
function un.onInit
	SetShellVarContext all
 
	#Verify the uninstaller - last chance to back out
	MessageBox MB_OKCANCEL "Uninstall  ${APPNAME} ?" IDOK next
		Abort
	next:
	!insertmacro VerifyUserIsAdmin
functionEnd
 
section "uninstall"

	# Remove startup information from the registry  
        #DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "TeapotNet"

	# Stop and uninstall the service
	Exec '"$INSTDIR\winservice.exe" stop'
	Exec '"$INSTDIR\winservice.exe" uninstall'

	# Remove desktop shortcut
	delete "$DESKTOP\${APPNAME}.lnk"

	# Remove Start Menu launcher
	delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
	# Try to remove the Start Menu folder
	rmDir "$SMPROGRAMS\${APPNAME}"
 
	# Remove files
	rmDir /R $INSTDIR\static
	delete $INSTDIR\teapotnet.exe
	delete $INSTDIR\teapotnet.ico
	delete $INSTDIR\winservice.exe
	delete $INSTDIR\winupdater.exe 

	# Others
	rmDir /R $INSTDIR\temp

	# Always delete uninstaller as the last action
	delete $INSTDIR\uninstall.exe
 
	# Try to remove the install directory
	rmDir $INSTDIR
 
	# Remove uninstaller information from the registry
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

sectionEnd

