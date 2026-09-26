; Inno Setup script for Spark on Windows. Build with:
;   ISCC.exe /DAppVersion=1.2.0 packaging\windows\spark.iss
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define Root "..\.."

[Setup]
AppId={{6E2C5F4A-5B1D-4C8E-9A57-5A1C0D7B2F11}
AppName=Spark
AppVersion={#AppVersion}
AppPublisher=Spark Audio
DefaultDirName={autopf}\Spark Audio\Spark
DefaultGroupName=Spark
DisableProgramGroupPage=yes
OutputDir={#Root}\dist
OutputBaseFilename=Spark-{#AppVersion}-Windows-Setup
SetupIconFile={#Root}\Resources\Icon.ico
UninstallDisplayIcon={app}\Spark.exe
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
Source: "{#Root}\build\Spark_artefacts\Release\VST3\Spark.vst3\*"; DestDir: "{commoncf64}\VST3\Spark.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\build\SparkFX_artefacts\Release\VST3\Spark FX.vst3\*"; DestDir: "{commoncf64}\VST3\Spark FX.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\build\Spark_artefacts\Release\Standalone\Spark.exe"; DestDir: "{app}"; Components: apps; Flags: ignoreversion
Source: "{#Root}\build\SparkFX_artefacts\Release\Standalone\Spark FX.exe"; DestDir: "{app}"; Components: apps; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Spark"; Filename: "{app}\Spark.exe"; Components: apps
Name: "{autoprograms}\Spark FX"; Filename: "{app}\Spark FX.exe"; Components: apps

[Messages]
FinishedLabel=Spark is installed.%n%nIn your DAW, rescan plug-ins (in Ableton Live: Settings > Plug-Ins > turn on "Use VST3 Plug-in System Folders" > Rescan). Spark appears under Spark Audio.
