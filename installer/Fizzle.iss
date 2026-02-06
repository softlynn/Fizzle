#define MyAppName "Fizzle"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Fizzle Audio"
#define MyAppExeName "Fizzle.exe"
#ifndef MyOutputBaseFilename
  #define MyOutputBaseFilename "Fizzle-Setup-" + MyAppVersion
#endif

[Setup]
AppId={{8ABF6D67-145A-4D8D-ACAA-5D9A4B9229AA}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Fizzle
DefaultGroupName=Fizzle
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=out
OutputBaseFilename={#MyOutputBaseFilename}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\resources\icon\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=yes
CloseApplicationsFilter=Fizzle.exe
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "..\build-x64\Fizzle_artefacts\Release\Fizzle.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "install_vbcable.ps1"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\Fizzle"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\Fizzle"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon; IconFilename: "{app}\{#MyAppExeName}"

[Run]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""{tmp}\install_vbcable.ps1"""; Flags: runhidden waituntilterminated
Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Parameters: "--show"; Description: "Launch Fizzle"; Flags: nowait postinstall skipifsilent
