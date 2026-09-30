; Inno Setup script for SparkLead on Windows. Build with:
;   ISCC.exe /DAppVersion=1.3.0 packaging\windows\sparklead.iss
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define Root "..\.."

[Setup]
AppId={{23FEA6F8-E26E-4CFC-9368-BF85AA80D6CE}
AppName=SparkLead
AppVersion={#AppVersion}
AppPublisher=Spark Audio
DefaultDirName={autopf}\Spark Audio\SparkLead
DefaultGroupName=SparkLead
DisableProgramGroupPage=yes
OutputDir={#Root}\dist
OutputBaseFilename=SparkLead-{#AppVersion}-Windows-Setup
SetupIconFile={#Root}\Resources\Icon.ico
UninstallDisplayIcon={app}\SparkLead.exe
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
Source: "{#Root}\build\SparkLead_artefacts\Release\VST3\SparkLead.vst3\*"; DestDir: "{commoncf64}\VST3\SparkLead.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\build\SparkLead_artefacts\Release\Standalone\SparkLead.exe"; DestDir: "{app}"; Components: apps; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\SparkLead"; Filename: "{app}\SparkLead.exe"; Components: apps

[Messages]
FinishedLabel=SparkLead is installed.%n%nIn your DAW, rescan plug-ins (in Ableton Live: Settings > Plug-Ins > turn on "Use VST3 Plug-in System Folders" > Rescan). SparkLead appears under Spark Audio.
