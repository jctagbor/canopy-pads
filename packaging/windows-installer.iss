; Inno Setup script - builds dist\Canopy-Pads-<version>-windows-setup.exe
; Installs the standalone app (Program Files) and the VST3 (Common Files\VST3).
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{615D0C5C-C06C-41A6-A16B-677D70D5A738}
AppName=Canopy Pads
AppVersion={#AppVersion}
AppPublisher=Tagbor Audio
AppPublisherURL=https://github.com/jctagbor/canopy-pads
DefaultDirName={autopf}\Canopy Pads
DefaultGroupName=Canopy Pads
DisableProgramGroupPage=yes
OutputDir=dist
OutputBaseFilename=Canopy-Pads-{#AppVersion}-windows-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequired=admin
UninstallDisplayName=Canopy Pads

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"

[Files]
Source: "..\build\CanopyPads_artefacts\Release\Standalone\Canopy Pads.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\CanopyPads_artefacts\Release\VST3\Canopy Pads.vst3\*"; DestDir: "{commoncf64}\VST3\Canopy Pads.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\README.md"; DestDir: "{app}"; DestName: "README.md"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Canopy Pads"; Filename: "{app}\Canopy Pads.exe"
Name: "{autodesktop}\Canopy Pads"; Filename: "{app}\Canopy Pads.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Canopy Pads.exe"; Description: "Open Canopy Pads now"; Flags: nowait postinstall skipifsilent

[Messages]
FinishedLabel=Canopy Pads is installed.%n%nStandalone: Start menu > Canopy Pads.%nIn your DAW (Ableton, Studio One, Reaper, FL Studio...): rescan plugins and look for Canopy Pads under Tagbor Audio.
