; Inno Setup 6 script: installs every HyperCut format into the system plugin folders.
; Built in CI with: ISCC.exe /DPkgVersion=<numeric version> packaging\hypercut.iss

#ifndef PkgVersion
  #define PkgVersion "0.0.0"
#endif

[Setup]
AppId={{8B1D4C3E-7A2F-4D69-9C14-6F0B5A2E7D31}
AppName=HyperCut
AppVersion={#PkgVersion}
AppVerName=HyperCut {#PkgVersion}
AppPublisher=eventual-recluse
AppPublisherURL=https://github.com/hexamorphical/hypercut
AppSupportURL=https://github.com/hexamorphical/hypercut/issues
AppUpdatesURL=https://github.com/hexamorphical/hypercut/releases
DefaultDirName={autopf}\HyperCut
DefaultGroupName=HyperCut
LicenseFile=..\LICENSE
OutputDir=..\dist
OutputBaseFilename=HyperCut-{#PkgVersion}-windows-x64-installer
SetupIconFile=..\packaging\hypercut.ico
UninstallDisplayIcon={app}\hypercut.ico
Compression=lzma2/max
SolidCompression=yes
PrivilegesRequired=admin
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut for the standalone"; GroupDescription: "Shortcuts:"

[Files]
; Shared plugin folders. Only our own files are removed by the uninstaller.
Source: ..\bin\HyperCut.vst3\*; DestDir: {commoncf}\VST3\HyperCut.vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: ..\bin\HyperCut.clap; DestDir: {commoncf}\CLAP; Flags: ignoreversion
Source: ..\bin\HyperCut.lv2\*; DestDir: {commoncf}\LV2\HyperCut.lv2; Flags: ignoreversion recursesubdirs createallsubdirs
Source: ..\bin\HyperCut-vst.dll; DestDir: {commoncf}\VST2; Flags: ignoreversion
Source: ..\bin\HyperCut.exe; DestDir: {app}; Flags: ignoreversion
Source: ..\packaging\hypercut.ico; DestDir: {app}; Flags: ignoreversion

[Icons]
Name: {group}\HyperCut; Filename: {app}\HyperCut.exe; IconFilename: {app}\hypercut.ico
Name: {autodesktop}\HyperCut; Filename: {app}\HyperCut.exe; IconFilename: {app}\hypercut.ico; Tasks: desktopicon

[Run]
Filename: {app}\HyperCut.exe; Description: "Launch HyperCut standalone"; Flags: nowait postinstall skipifsilent
