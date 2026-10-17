; Fero Player installer (Inno Setup 6). Built by tools\build.ps1 when the
; request contains "installer"; the version comes from CMakeLists.txt.
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\FeroPlayer"
#endif

[Setup]
AppId={{6B0E5C1E-6F55-4C9B-9E0A-F3A0D2E1F7C1}
AppName=Fero Player
AppVersion={#AppVersion}
AppVerName=Fero Player {#AppVersion}
AppPublisher=Fero Player authors
AppPublisherURL=https://github.com/
DefaultDirName={autopf}\Fero Player
DefaultGroupName=Fero Player
DisableProgramGroupPage=yes
; Installs for the current user without administrator rights; an
; all-users install can be chosen in the first dialog.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\dist
OutputBaseFilename=FeroPlayer-Setup-{#AppVersion}
SetupIconFile=..\res\fero.ico
UninstallDisplayIcon={app}\FeroPlayer.exe
UninstallDisplayName=Fero Player
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[CustomMessages]
en.DefaultApp=Choose Fero Player as the default video player
ru.DefaultApp=Выбрать Fero Player плеером по умолчанию
en.Desktop=Create a desktop shortcut
ru.Desktop=Создать ярлык на рабочем столе
en.MediaFile=Media file
ru.MediaFile=Медиафайл

[Tasks]
Name: "desktopicon"; Description: "{cm:Desktop}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Fero Player"; Filename: "{app}\FeroPlayer.exe"
Name: "{autodesktop}\Fero Player"; Filename: "{app}\FeroPlayer.exe"; Tasks: desktopicon

[Registry]
; One file type for all media; Windows then lists Fero Player in "Open with"
; and in Settings > Default apps. Existing defaults are not taken over.
Root: HKA; Subkey: "Software\Classes\FeroPlayer.Media"; ValueType: string; ValueData: "{cm:MediaFile}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\FeroPlayer.Media\DefaultIcon"; ValueType: string; ValueData: "{app}\FeroPlayer.exe,0"
Root: HKA; Subkey: "Software\Classes\FeroPlayer.Media\shell\open\command"; ValueType: string; ValueData: """{app}\FeroPlayer.exe"" ""%1"""
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "Fero Player"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\shell\open\command"; ValueType: string; ValueData: """{app}\FeroPlayer.exe"" ""%1"""
Root: HKA; Subkey: "Software\FeroPlayer"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "Fero Player"
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Fast video and music player"
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities"; ValueType: string; ValueName: "ApplicationIcon"; ValueData: "{app}\FeroPlayer.exe,0"
Root: HKA; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "Fero Player"; ValueData: "Software\FeroPlayer\Capabilities"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.mkv\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mkv"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mkv"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mp4\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp4"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mp4"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.avi\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".avi"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".avi"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mov\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mov"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mov"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.webm\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".webm"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".webm"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.m4v\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4v"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".m4v"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.wmv\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wmv"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".wmv"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.flv\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flv"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".flv"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mpg\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpg"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mpg"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mpeg\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpeg"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mpeg"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.ts\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ts"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".ts"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.m2ts\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m2ts"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".m2ts"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mts\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mts"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mts"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.vob\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".vob"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".vob"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.ogv\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogv"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".ogv"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.3gp\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".3gp"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".3gp"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.3g2\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".3g2"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".3g2"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.divx\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".divx"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".divx"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.asf\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".asf"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".asf"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mp3\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp3"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mp3"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.flac\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flac"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".flac"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.wav\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wav"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".wav"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.ogg\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogg"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".ogg"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.m4a\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4a"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".m4a"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.aac\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".aac"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".aac"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.opus\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".opus"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".opus"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.wma\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wma"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".wma"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.aiff\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".aiff"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".aiff"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.aif\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".aif"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".aif"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.ape\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ape"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".ape"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.alac\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".alac"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".alac"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mp2\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp2"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mp2"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.ac3\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ac3"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".ac3"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.eac3\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".eac3"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".eac3"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.dts\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dts"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".dts"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mka\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mka"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mka"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mpv\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpv"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mpv"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.m2v\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m2v"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".m2v"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mxf\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mxf"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mxf"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.rm\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".rm"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".rm"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.rmvb\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".rmvb"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".rmvb"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.f4v\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".f4v"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".f4v"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.hevc\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".hevc"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".hevc"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.h264\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".h264"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".h264"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.265\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".265"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".265"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.264\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".264"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".264"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.y4m\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".y4m"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".y4m"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.nut\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".nut"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".nut"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.dv\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dv"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".dv"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.wv\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wv"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".wv"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.tta\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tta"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".tta"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.dsf\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dsf"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".dsf"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.dff\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dff"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".dff"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mpc\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpc"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mpc"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.spx\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".spx"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".spx"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.amr\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".amr"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".amr"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.caf\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".caf"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".caf"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mlp\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mlp"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mlp"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.thd\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".thd"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".thd"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.avs\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".avs"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".avs"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.ivf\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ivf"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".ivf"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.evo\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".evo"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".evo"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.m2p\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m2p"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".m2p"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.trp\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".trp"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".trp"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.tp\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tp"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".tp"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.ogm\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogm"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".ogm"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mk3d\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mk3d"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mk3d"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.wtv\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wtv"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".wtv"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.dvr-ms\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dvr-ms"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".dvr-ms"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.mod\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mod"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".mod"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.tod\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tod"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".tod"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.bik\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".bik"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".bik"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.smk\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".smk"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".smk"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\.xvid\OpenWithProgids"; ValueType: string; ValueName: "FeroPlayer.Media"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\FeroPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".xvid"; ValueData: "FeroPlayer.Media"
Root: HKA; Subkey: "Software\Classes\Applications\FeroPlayer.exe\SupportedTypes"; ValueType: string; ValueName: ".xvid"; ValueData: ""

[Run]
Filename: "ms-settings:defaultapps?registeredAppUser=Fero Player"; Description: "{cm:DefaultApp}"; Flags: postinstall shellexec skipifsilent nowait; Check: not IsAdminInstallMode
Filename: "ms-settings:defaultapps?registeredAppMachine=Fero Player"; Description: "{cm:DefaultApp}"; Flags: postinstall shellexec skipifsilent nowait; Check: IsAdminInstallMode
Filename: "{app}\FeroPlayer.exe"; Description: "{cm:LaunchProgram,Fero Player}"; Flags: postinstall skipifsilent nowait
