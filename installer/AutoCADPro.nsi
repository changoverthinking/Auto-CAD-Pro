!include "MUI2.nsh"

!ifndef VERSION
  !define VERSION "0.2.0"
!endif
!ifndef SOURCE_EXE
  !error "SOURCE_EXE must be provided"
!endif
!ifndef README_FILE
  !error "README_FILE must be provided"
!endif
!ifndef OUTPUT_DIR
  !define OUTPUT_DIR "."
!endif

Name "Auto CAD Pro"
OutFile "${OUTPUT_DIR}\AutoCADPro-${VERSION}-Windows-x64-Setup.exe"
InstallDir "$PROGRAMFILES64\Auto CAD Pro"
InstallDirRegKey HKLM "Software\Auto CAD Pro" "InstallDir"
RequestExecutionLevel admin
Unicode true

VIProductVersion "0.2.0.0"
VIAddVersionKey "ProductName" "Auto CAD Pro"
VIAddVersionKey "CompanyName" "Auto CAD Pro"
VIAddVersionKey "FileDescription" "Auto CAD Pro Windows Installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"

!define MUI_ABORTWARNING
!define MUI_ICON "$%WINDIR%\System32\shell32.dll"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Auto CAD Pro" SEC_MAIN
  SetOutPath "$INSTDIR"
  File /oname=AutoCADPro.exe "${SOURCE_EXE}"
  File /oname=README.md "${README_FILE}"

  CreateDirectory "$SMPROGRAMS\Auto CAD Pro"
  CreateShortcut "$SMPROGRAMS\Auto CAD Pro\Auto CAD Pro.lnk" "$INSTDIR\AutoCADPro.exe"
  CreateShortcut "$DESKTOP\Auto CAD Pro.lnk" "$INSTDIR\AutoCADPro.exe"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "Software\Auto CAD Pro" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AutoCADPro" "DisplayName" "Auto CAD Pro"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AutoCADPro" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AutoCADPro" "Publisher" "Auto CAD Pro"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AutoCADPro" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AutoCADPro" "DisplayIcon" "$INSTDIR\AutoCADPro.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AutoCADPro" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AutoCADPro" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AutoCADPro" "NoRepair" 1
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\Auto CAD Pro.lnk"
  Delete "$SMPROGRAMS\Auto CAD Pro\Auto CAD Pro.lnk"
  RMDir "$SMPROGRAMS\Auto CAD Pro"

  Delete "$INSTDIR\AutoCADPro.exe"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AutoCADPro"
  DeleteRegKey HKLM "Software\Auto CAD Pro"
SectionEnd
