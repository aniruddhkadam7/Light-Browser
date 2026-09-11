; LightBrowser Windows installer.
; Built with NSIS (https://nsis.sourceforge.io/).

!include "MUI2.nsh"

Name "LightBrowser"
OutFile "LightBrowserSetup.exe"
InstallDir "$PROGRAMFILES64\LightBrowser"
InstallDirRegKey HKLM "Software\LightBrowser" "InstallDir"
RequestExecutionLevel admin

!define MUI_ABORTWARNING
!define MUI_ICON "..\resources\app.ico"
!define MUI_UNICON "..\resources\app.ico"

; ---- Pages ----
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
Page custom DesktopShortcutPageCreate DesktopShortcutPageLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ---- Desktop shortcut opt-in checkbox page ----
Var Dialog
Var DesktopCheckbox
Var CreateDesktopShortcut

Function DesktopShortcutPageCreate
    nsDialogs::Create 1018
    Pop $Dialog
    ${If} $Dialog == error
        Abort
    ${EndIf}

    ${NSD_CreateCheckbox} 0 20u 100% 10u "Create a desktop shortcut"
    Pop $DesktopCheckbox
    ${NSD_SetState} $DesktopCheckbox ${BST_CHECKED}

    nsDialogs::Show
FunctionEnd

Function DesktopShortcutPageLeave
    ${NSD_GetState} $DesktopCheckbox $CreateDesktopShortcut
FunctionEnd

; ---- Install ----
Section "LightBrowser" SecMain
    ; Without this, $SMPROGRAMS/$DESKTOP resolve to the current user's own
    ; profile even when running elevated — shortcuts would only appear for
    ; whichever account ran the installer, not every account on the PC, the
    ; way a properly installed Program Files application (Chrome, Brave,
    ; Edge) behaves.
    SetShellVarContext all

    SetOutPath "$INSTDIR"
    File /r "stage\*.*"

    WriteUninstaller "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "Software\LightBrowser" "InstallDir" "$INSTDIR"

    CreateDirectory "$SMPROGRAMS\LightBrowser"
    CreateShortcut "$SMPROGRAMS\LightBrowser\LightBrowser.lnk" "$INSTDIR\LightBrowser.exe"
    CreateShortcut "$SMPROGRAMS\LightBrowser\Uninstall LightBrowser.lnk" "$INSTDIR\Uninstall.exe"

    ${If} $CreateDesktopShortcut == ${BST_CHECKED}
        CreateShortcut "$DESKTOP\LightBrowser.lnk" "$INSTDIR\LightBrowser.exe"
    ${EndIf}

    ; Add/Remove Programs entry
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LightBrowser" \
        "DisplayName" "LightBrowser"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LightBrowser" \
        "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LightBrowser" \
        "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LightBrowser" \
        "DisplayIcon" "$INSTDIR\LightBrowser.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LightBrowser" \
        "Publisher" "LightBrowser"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LightBrowser" \
        "DisplayVersion" "1.0.0"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LightBrowser" \
        "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LightBrowser" \
        "NoRepair" 1
SectionEnd

; ---- Uninstall ----
Section "Uninstall"
    SetShellVarContext all
    RMDir /r "$INSTDIR"
    Delete "$SMPROGRAMS\LightBrowser\LightBrowser.lnk"
    Delete "$SMPROGRAMS\LightBrowser\Uninstall LightBrowser.lnk"
    RMDir "$SMPROGRAMS\LightBrowser"
    Delete "$DESKTOP\LightBrowser.lnk"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LightBrowser"
    DeleteRegKey HKLM "Software\LightBrowser"
SectionEnd
