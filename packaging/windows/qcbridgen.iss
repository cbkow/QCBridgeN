; QCBridgeN — Windows installer (Inno Setup 6).
;
;   packaging\windows\build-setup.cmd   (runs windeployqt, then this)
;
; Installs the app under Program Files\QCBridgeN with the Qt it needs and
; the NDI runtime DLL from the SDK (the SDK documentation allows the
; runtime in the application folder; an installed NDI Runtime, found
; through NDI_RUNTIME_DIR_V6, is preferred by the app), and registers the
; output with QCView by writing %ProgramData%\QCView\Outputs\ndi.json.

#define MyAppName "QCBridgeN"
#define MyAppVersion "0.0.1"
#define MyAppPublisher "cbkow"
#define MyAppExeName "QCBridgeN.exe"
#define StageDir "..\..\build\stage"

[Setup]
AppId={{96C2D872-FB03-4AF9-8EC2-FF48F79D0532}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename={#MyAppName}-{#MyAppVersion}-Setup-x64
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
LicenseFile=..\..\LICENSE
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=yes

[Files]
; Everything windeployqt staged beside the exe (Qt DLLs, platforms, imageformats, styles) plus the NDI runtime.
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; QCView's registration of this output (O12).
Source: "ndi.json"; DestDir: "{commonappdata}\QCView\Outputs"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: files; Name: "{commonappdata}\QCView\Outputs\ndi.json"

[Run]
; The VC++ runtime the Qt DLLs need, silently; nothing else starts at the
; end — QCView starts the sink when its output is switched on.
Filename: "{app}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing the Visual C++ runtime…"; Flags: waituntilterminated skipifdoesntexist
