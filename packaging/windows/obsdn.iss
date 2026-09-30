; Inno Setup script for OBSDN on Windows. Build with:
;   ISCC.exe /DAppVersion=1.3.0 packaging\windows\obsdn.iss
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define Root "..\.."

[Setup]
AppId={{EABDE25B-BE65-4020-A2A0-6502EB794D43}
AppName=OBSDN
AppVersion={#AppVersion}
AppPublisher=Spark Audio
DefaultDirName={autopf}\Spark Audio\OBSDN
DefaultGroupName=OBSDN
DisableProgramGroupPage=yes
OutputDir={#Root}\dist
OutputBaseFilename=OBSDN-{#AppVersion}-Windows-Setup
SetupIconFile={#Root}\Resources\OBSDN\Icon.ico
UninstallDisplayIcon={app}\OBSDN.exe
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin

[Types]
Name: "full"; Description: "Everything"
Name: "custom"; Description: "Choose what to install"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugins (Ableton Live, FL Studio, Cubase, Bitwig, Reaper, Studio One)"; Types: full custom
Name: "apps"; Description: "Standalone apps"; Types: full custom

[Files]
Source: "{#Root}\build\OBSDN_artefacts\Release\VST3\OBSDN.vst3\*"; DestDir: "{commoncf64}\VST3\OBSDN.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\build\OBSDN_artefacts\Release\Standalone\OBSDN.exe"; DestDir: "{app}"; Components: apps; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\OBSDN"; Filename: "{app}\OBSDN.exe"; Components: apps

[Messages]
FinishedLabel=OBSDN is installed.%n%nIn your DAW, rescan plug-ins (in Ableton Live: Settings > Plug-Ins > turn on "Use VST3 Plug-in System Folders" > Rescan). OBSDN appears under Spark Audio.
