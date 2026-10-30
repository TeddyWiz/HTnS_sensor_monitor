; ============================================================
;  MeterMonitor 설치 프로그램 스크립트 (Inno Setup 6)
;
;  사용법)
;    1) deploy.bat 을 먼저 실행해서 deploy\ 폴더를 만든다.
;    2) Inno Setup Compiler 로 이 파일을 열고 [Build] - [Compile]
;    3) Output\MeterMonitor_Setup_x64.exe 가 생성된다.
;
;  Inno Setup 내려받기 : https://jrsoftware.org/isdl.php
; ============================================================

#define MyAppName    "MeterMonitor"
#define MyAppVersion "1.0.0"
#define MyAppExeName "MeterMonitor.exe"

[Setup]
AppId={{8B4C1E52-2F1A-4C6E-9E5B-6B7C2A9D4F10}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=Output
OutputBaseFilename={#MyAppName}_Setup_x64
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardStyle=modern

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"

[Tasks]
Name: "desktopicon"; Description: "바탕 화면에 바로 가기 만들기"; GroupDescription: "추가 아이콘:"

[Files]
; deploy.bat 이 만든 폴더 전체를 담는다 (Qt DLL 포함)
Source: "deploy\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

; Visual C++ 재배포 패키지를 함께 배포하려면 아래 주석을 풀고 파일을 준비한다.
; Source: "redist\vc_redist.x64.exe"; DestDir: {tmp}; Flags: deleteafterinstall

[Icons]
Name: "{group}\{#MyAppName}";           Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}";     Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/quiet /norestart"; StatusMsg: "Visual C++ 런타임 설치 중..."; Check: not VCRedistInstalled
Filename: "{app}\{#MyAppExeName}"; Description: "{#MyAppName} 실행"; Flags: nowait postinstall skipifsilent

[Dirs]
; 로그 폴더는 모든 사용자가 쓸 수 있도록 권한을 준다.
Name: "{app}\log"; Permissions: users-modify

[Code]
function VCRedistInstalled: Boolean;
var
  Installed: Cardinal;
begin
  Result := RegQueryDWordValue(HKLM,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed)
    and (Installed = 1);
end;
