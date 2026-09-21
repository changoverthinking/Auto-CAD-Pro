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
!ifndef PDF_FONT_FILE
  !error "PDF_FONT_FILE must be provided"
!endif
!ifndef NOTO_LICENSE_FILE
  !error "NOTO_LICENSE_FILE must be provided"
!endif
!ifndef NOTO_SOURCE_FILE
  !error "NOTO_SOURCE_FILE must be provided"
!endif
!ifndef STB_LICENSE_FILE
  !error "STB_LICENSE_FILE must be provided"
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

  SetOutPath "$INSTDIR\assets"
  File /oname=NotoSansJP.ttf "${PDF_FONT_FILE}"

  SetOutPath "$INSTDIR\licenses\NotoSansJP"
  File /oname=OFL.txt "${NOTO_LICENSE_FILE}"
  File /oname=SOURCE.md "${NOTO_SOURCE_FILE}"

  SetOutPath "$INSTDIR\licenses\stb"
  File /oname=LICENSE "${STB_LICENSE_FILE}"

  SetOutPath "$INSTDIR"
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
  Delete "$INSTDIR\assets\NotoSansJP.ttf"
  RMDir "$INSTDIR\assets"
  Delete "$INSTDIR\licenses\NotoSansJP\OFL.txt"
  Delete "$INSTDIR\licenses\NotoSansJP\SOURCE.md"
  RMDir "$INSTDIR\licenses\NotoSansJP"
  Delete "$INSTDIR\licenses\stb\LICENSE"
  RMDir "$INSTDIR\licenses\stb"
  RMDir "$INSTDIR\licenses"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AutoCADPro"
  DeleteRegKey HKLM "Software\Auto CAD Pro"
SectionEnd
