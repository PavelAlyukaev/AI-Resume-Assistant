; Inno Setup script for AI Resume Assistant.
; Built by the `installer` CMake target (see CMakeLists.txt), which passes:
;   /DSourceDir=<deployed install prefix>  /DAppVersion=<project version>

#ifndef SourceDir
  #error SourceDir is not defined: build via the CMake `installer` target or pass /DSourceDir=...
#endif
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define AppName "AI Resume Assistant"
#define AppExeName "AIResumeAssistant.exe"

[Setup]
AppId={{BD049C49-5768-4751-8D08-0E21B05C12C4}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Pavel Alyukaev
AppPublisherURL=https://github.com/PavelAlyukaev/AI-Resume-Assistant
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputBaseFilename=AIResumeAssistant-{#AppVersion}-setup
UninstallDisplayIcon={app}\bin\{#AppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Resume database lives in %APPDATA% and is intentionally left on uninstall.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\bin\{#AppExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\bin\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
