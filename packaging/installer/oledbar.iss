; OLEDbar per-user installer: no admin rights, installs to %LOCALAPPDATA%\Programs\OLEDbar.
; Built by packaging/package.ps1, e.g.: ISCC /DAppVersion=0.1.0 /DSourceExe=...\OLEDbar.exe /DOutputDir=...\dist oledbar.iss

#ifndef AppVersion
  #error AppVersion must be defined, e.g. ISCC /DAppVersion=0.1.0
#endif
#ifndef SourceExe
  #define SourceExe "..\..\out\build\x64-release\OLEDbar.exe"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\out\dist"
#endif
#define AppUrl "https://github.com/midimaxi-dev/OLEDbar"

[Setup]
; Fixed AppId: keeps upgrades and uninstall tied to the same entry across versions.
AppId={{6E4E3C1A-9B57-4F0C-8E2D-7A51C3B9D402}
AppName=OLEDbar
AppVersion={#AppVersion}
AppVerName=OLEDbar {#AppVersion}
AppPublisher=OLEDbar contributors
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}/releases
VersionInfoVersion={#AppVersion}
VersionInfoDescription=OLEDbar Setup
PrivilegesRequired=lowest
DefaultDirName={autopf}\OLEDbar
DisableDirPage=auto
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22621
LicenseFile=..\..\LICENSE
SetupIconFile=..\..\src\app\app.ico
UninstallDisplayIcon={app}\OLEDbar.exe
UninstallDisplayName=OLEDbar
OutputDir={#OutputDir}
OutputBaseFilename=OLEDbar-{#AppVersion}-x64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; The running app is closed in [Code] instead of via Restart Manager.
CloseApplications=no

[Tasks]
Name: "startup"; Description: "Start OLEDbar when I sign in"

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\OLEDbar"; Filename: "{app}\OLEDbar.exe"

[Registry]
; Same value the app's "Start OLEDbar when I sign in" checkbox writes.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "OLEDbar"; ValueData: """{app}\OLEDbar.exe"""; Tasks: startup
; Always remove on uninstall, also if it was enabled later from the settings panel.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "OLEDbar"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run"; ValueType: none; ValueName: "OLEDbar"; Flags: uninsdeletevalue dontcreatekey

[Run]
Filename: "{app}\OLEDbar.exe"; Description: "Start OLEDbar now"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Settings and logs.
Type: filesandordirs; Name: "{localappdata}\OLEDbar"

[Code]
const
  WM_CLOSE = $0010;

{ Asks a running OLEDbar to exit (its hidden host window handles WM_CLOSE) and waits for it. }
procedure CloseRunningOLEDbar();
var
  Wnd: HWND;
  I: Integer;
begin
  Wnd := FindWindowByClassName('OLEDbar.Host');
  if Wnd <> 0 then
  begin
    SendMessage(Wnd, WM_CLOSE, 0, 0);
    for I := 1 to 30 do
    begin
      if FindWindowByClassName('OLEDbar.Host') = 0 then
        Break;
      Sleep(100);
    end;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  CloseRunningOLEDbar();
  Result := '';
end;

function InitializeUninstall(): Boolean;
begin
  CloseRunningOLEDbar();
  Result := True;
end;
