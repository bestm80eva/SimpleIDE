; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "SimpleIDE"
#define MyDocName "SimpleIDE"
#define MyAppVersion "1-0-10"
#define MyAppPublisher "ParallaxInc"
#define MyAppURL "parallax.com"
#define MyAppExeName "bin\SimpleIDE.exe"
#define FtdiChipApp "CDM v2.10.00 WHQL Certified.exe"

#define compiler "propeller-gcc"

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; ---- IMPORTANT!!! ---- Set this to your QtPath
; Qt5 library versions have trouble with printing, so we digressed to Qt4.8
; The Qt5.3 mingw path is the compiler used to build the app. 
; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
#define MyQtPath "C:\Qt5.5\5.4\mingw491_32"
; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; This path will need to be changed to match the path containing mingw
; It should be the same path used in building the application.
; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
#define MyQtMingwPath "C:\Qt5.5\5.4\mingw491_32"
; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; This path contains the propeller gcc build
; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;#define MyGccPath "C:\mingw\msys\1.0\opt\parallax"
#define MyGccPath "..\..\propeller-gcc4-win32"
; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; This path must contains the mingw path used for the propeller gcc build
; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
#define MyGccMingwPath "C:\mingw"
#define MyTranslations "..\propside\translations"
#define MyUserGuide "..\SimpleIDE-User-Guide.pdf"
#define MySpinPath "..\spin"
#define MyEduLibPath "..\..\Workspace"
#define MyAppBin "{app}\bin"
#define MyBoardFilter "..\boards.txt"
#define MyFont "..\Parallax.ttf"
;#define MyLoaderPath "..\..\proploader\build-proploader-Desktop_Qt_5_4_2_MinGW_32bit2-Release\Release"
#define MyLoaderPath "..\..\proploader\proploader-msys-build\bin"
;#define MyAppPath "..\build-propside-Qt_4_8_6-Release"
#define MyAppPath "..\build-propside-Desktop_Qt_5_4_2_MinGW_32bit2-Release"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
; AppID={{4FA91D9B-6633-4229-B3BE-DF96DFD916F3} - old v0-7-2 AppID
AppID={{CE380BA3-F51E-4DCB-A068-216961358E89}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=..\propside-build-desktop
OutputBaseFilename=Simple-IDE_{#MyAppVersion}_Windows_Setup
Compression=lzma/Max
SolidCompression=true
AlwaysShowDirOnReadyPage=true
UserInfoPage=false
UsePreviousUserInfo=false
ChangesEnvironment=true
ChangesAssociations=yes
LicenseFile=.\IDE_LICENSE.txt
WizardImageFile=images\SimpleIDE-Install-Splash5.bmp
;WizardImageStretch=no
SetupIconFile=images\SimpleIDE-all.ico
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

;[Tasks]
;Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: checkedonce;
;  GroupDescription: "{cm:AdditionalIcons}";
;Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; Flags: checkedonce; OnlyBelowVersion: 0,6.1
; GroupDescription: "{cm:AdditionalIcons}"; 
;Name: "association"; Description: "Associate *.side Files with SimpleIDE"; Flags: checkedonce;
; GroupDescription: "File Association:"; 
;Name: "modifypath"; Description: "&Add Propeller-GCC directory to your environment PATH"; Flags: checkedonce;
; GroupDescription: "Propeller-GCC Path:"
;Name: FtdiChip; Description: "Install FTDI Chip USB Serial Port Drivers"; Flags: checkedonce; 

[Files]
;Source: "..\propside-build-desktop\debug\SimpleIDE.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#MyAppPath}\release\SimpleIDE.exe"; DestDir: "{#MyAppBin}"; Flags: ignoreversion
Source: "..\{#FtdiChipApp}"; DestDir: "{app}"; Flags: ignoreversion
Source: "IDE_LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "LGPL_2_1.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "LGPL_EXCEPTION.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: ..\ctags58\README; DestDir: {app}; Flags: ignoreversion; DestName: ctags-readme.txt; 
Source: ..\ctags58\COPYING; DestDir: {app}; Flags: ignoreversion; DestName: ctags-license.txt; 
Source: ..\icons\24x24-free-application-icons\readme.txt; DestDir: {app}; Flags: ignoreversion; DestName: aha-soft-license.txt; 
Source: "{#MyBoardFilter}"; DestDir: "{app}\propeller-gcc\propeller-load\"; Flags: ignoreversion
Source: "{#MyFont}"; DestDir: "{#MyAppBin}"; Flags: ignoreversion

;Source: "{#MyGccMingwPath}\bin\libgcc_s_dw2-1.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
;Source: "{#MyGccMingwPath}\bin\mingwm10.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
;Source: "{#MyGccMingwPath}\bin\libstdc++*"; DestDir: "{app}\bin"; Flags: ignoreversion
;Source: "{#MyQtPath}\bin\quazip1.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#MyQtPath}\bin\Qt5Core.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
;Source: "{#MyQtPath}\bin\Qt5Cored.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#MyQtPath}\bin\Qt5Gui.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#MyQtPath}\bin\Qt5Network.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
;Source: "{#MyQtPath}\bin\Qt5Networkd.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#MyQtPath}\bin\Qt5PrintSupport.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#MyQtPath}\bin\Qt5SerialPort.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
;Source: "{#MyQtPath}\bin\Qt5SerialPortd.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#MyQtPath}\bin\Qt5Widgets.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#MyQtPath}\plugins\platforms\*"; DestDir: "{app}\bin\platforms"; Flags: ignoreversion
Source: "zlib1.dll"; DestDir: "{#MyAppBin}"; Flags: ignoreversion
Source: "{#MyLoaderPath}\proploader.exe"; DestDir: "{app}\bin"; Flags: ignoreversion

; These paths are based on the mingw distribution used in building the application and should match.
Source: "{#MyQtMingwPath}\bin\icuin53.dll"; DestDir: "{#MyAppBin}"; Flags: ignoreversion
Source: "{#MyQtMingwPath}\bin\icuuc53.dll"; DestDir: "{#MyAppBin}"; Flags: ignoreversion
Source: "{#MyQtMingwPath}\bin\icudt53.dll"; DestDir: "{#MyAppBin}"; Flags: ignoreversion
Source: "{#MyQtMingwPath}\bin\libgcc_s_dw2-1.dll"; DestDir: "{#MyAppBin}"; Flags: ignoreversion
Source: "{#MyQtMingwPath}\bin\libstdc++-6.dll"; DestDir: "{#MyAppBin}"; Flags: ignoreversion
Source: "{#MyQtMingwPath}\bin\libwinpthread-1.dll"; DestDir: "{#MyAppBin}"; Flags: ignoreversion

Source: "{#MyGccPath}\*"; DestDir: "{app}\propeller-gcc"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyQtMingwPath}\bin\libgcc_s_dw2-1.dll"; DestDir: "{app}\propeller-gcc\bin"; Flags: ignoreversion
Source: "{#MyQtMingwPath}\bin\libstdc++-6.dll"; DestDir: "{app}\propeller-gcc\bin"; Flags: ignoreversion
Source: "{#MyQtMingwPath}\bin\libwinpthread-1.dll"; DestDir: "{app}\propeller-gcc\bin"; Flags: ignoreversion
Source: "{#MyEduLibPath}\*"; DestDir: "{app}\Workspace"; Flags: ignoreversion recursesubdirs createallsubdirs


; Stephanie says not to include the Spin folder with all docs - this one trims the docs.
;Source: "{#MySpinPath}\*"; DestDir: "{app}\propeller-gcc\spin"; Flags: ignoreversion recursesubdirs createallsubdirs

;Source: "..\ctags-5.8\ctags.exe"; DestDir: "{app}\propeller-gcc\bin"; Flags: ignoreversion
;Source: "{#MyGccMingwPath}\bin\libi*"; DestDir: "{app}\propeller-gcc\bin"; Flags: ignoreversion
Source: "{#MyTranslations}\SimpleIDE_es.qm"; DestDir: {app}/translations; Flags: IgnoreVersion recursesubdirs createallsubdirs; 
Source: "{#MyTranslations}\SimpleIDE_fr.qm"; DestDir: {app}/translations; Flags: IgnoreVersion recursesubdirs createallsubdirs; 
Source: "{#MyTranslations}\SimpleIDE_zh.qm"; DestDir: {app}/translations; Flags: IgnoreVersion recursesubdirs createallsubdirs; 
Source: "{#MyUserGuide}"; DestDir: "{app}\propeller-gcc\bin"; Flags: IgnoreVersion; 

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}";
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}";

[Run]
;don't run: the environment variable will not be set until program restart.
;Filename: {app}\{#MyAppExeName}; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, "&", "&&")}}"; Flags: skipifsilent NoWait PostInstall; 
Filename: {app}\{#FtdiChipApp}; Flags: RunAsCurrentUser NoWait; 

[Registry]
; would like to use HKLM for these things if possible for specifying compiler and user workspace fields.
Root: HKCU; SubKey: Software\{#MyAppPublisher}; Flags: DeleteKey UninsDeleteKey; 
Root: HKCU; SubKey: Software\{#MyAppPublisher}\SimpleIDE\*; Flags: DeleteKey UninsDeleteKey; 
;Root: HKCU; Subkey: "Software\{#MyAppPublisher}\SimpleIDE"; ValueType: string; ValueName: SimpleIDE_Compiler; ValueData: "{app}\propeller-gcc\bin\propeller-elf-gcc.exe"; Flags: UninsDeleteKey; 
;Root: HKCU; Subkey: "Software\{#MyAppPublisher}\SimpleIDE"; ValueType: string; ValueName: SimpleIDE_Includes; ValueData: "{app}\propeller-gcc\propeller-load\"; Flags: UninsDeleteKey; 
;;Root: HKCU; Subkey: "Software\{#MyAppPublisher}\SimpleIDE"; ValueType: string; ValueName: SimpleIDE_Workspace; ValueData: "{userdocs}\Workspace"; Flags: UninsDeleteKey;
;;Root: HKCU; Subkey: "Software\{#MyAppPublisher}\SimpleIDE"; ValueType: string; ValueName: SimpleIDE_Library; ValueData: "{userdocs}\Workspace\Learn\Simple Libraries\"; Flags: UninsDeleteKey;
;;Root: HKCU; Subkey: "Software\{#MyAppPublisher}\SimpleIDE"; ValueType: string; ValueName: SimpleIDE_SpinCompiler; ValueData: "{app}\propeller-gcc\bin\bstc.exe"; Flags: UninsDeleteKey; 
;Root: HKCU; Subkey: "Software\{#MyAppPublisher}\SimpleIDE"; ValueType: string; ValueName: SimpleIDE_SpinLibrary; ValueData: "{app}\propeller-gcc\spin\"; Flags: UninsDeleteKey; 
;;Root: HKCU; Subkey: "Software\{#MyAppPublisher}\SimpleIDE"; ValueType: string; ValueName: SimpleIDE_SpinWorkspace; ValueData: "{userdocs}\Workspace\"; Flags: UninsDeleteKey;
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: expandsz; ValueName: "PATH"; ValueData: "{olddata}"; Check: NeedPropGccBinPath();

; File Association. Doesn't work without ChangesAssociations=yes
;Root: HKCR; Subkey: ".c"; ValueType: string; ValueData: "SimpleIDE"; Flags: UninsDeleteKey;
;Root: HKCR; Subkey: ".cpp"; ValueType: string; ValueData: "SimpleIDE"; Flags: UninsDeleteKey;
;Root: HKCR; Subkey: ".cogc"; ValueType: string; ValueData: "SimpleIDE"; Flags: UninsDeleteKey;
;Root: HKCR; Subkey: ".h"; ValueType: string; ValueData: "SimpleIDE"; Flags: UninsDeleteKey;
Root: HKCR; Subkey: ".side"; ValueType: string; ValueData: "SimpleIDE"; Flags: DeleteKey;
Root: HKCR; Subkey: ".side"; ValueType: string; ValueData: "SimpleIDE"; Flags: UninsDeleteKey;
Root: HKCR; SubKey: "SimpleIDE"; ValueType: string; ValueData: "SimpleIDE Application"; Flags: DeleteKey;
Root: HKCR; Subkey: "SimpleIDE\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\SimpleIDE.exe"" ""%1""";  Flags: DeleteKey;
Root: HKCR; SubKey: "SimpleIDE\DefaultIcon"; ValueType: string; ValueData: "{app}\bin\SimpleIDE.exe,3"; Flags: DeleteKey;
Root: HKCR; SubKey: "SimpleIDE"; ValueType: string; ValueData: "SimpleIDE Application"; Flags: UninsDeleteKey;
Root: HKCR; Subkey: "SimpleIDE\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\SimpleIDE.exe"" ""%1""";  Flags: UninsDeleteKey;
Root: HKCR; SubKey: "SimpleIDE\DefaultIcon"; ValueType: string; ValueData: "{app}\bin\SimpleIDE.exe,3"; Flags: UninsDeleteKey;

Root: HKCU; Subkey: ".side"; ValueType: string; ValueData: "SimpleIDE"; Flags: DeleteKey;
Root: HKCU; Subkey: ".side"; ValueType: string; ValueData: "SimpleIDE"; Flags: UninsDeleteKey;
Root: HKCU; SubKey: "SimpleIDE"; ValueType: string; ValueData: "SimpleIDE Application"; Flags: DeleteKey;
Root: HKCU; Subkey: "SimpleIDE\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\SimpleIDE.exe"" ""%1""";  Flags: DeleteKey;
Root: HKCU; SubKey: "SimpleIDE\DefaultIcon"; ValueType: string; ValueData: "{app}\bin\SimpleIDE.exe,3"; Flags: DeleteKey;
Root: HKCU; SubKey: "SimpleIDE"; ValueType: string; ValueData: "SimpleIDE Application"; Flags: UninsDeleteKey;
Root: HKCU; Subkey: "SimpleIDE\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\SimpleIDE.exe"" ""%1""";  Flags: UninsDeleteKey;
Root: HKCU; SubKey: "SimpleIDE\DefaultIcon"; ValueType: string; ValueData: "{app}\bin\SimpleIDE.exe,3"; Flags: UninsDeleteKey;

; Startup File
;Root: HKCU; Subkey: "Software\{#MyAppPublisher}\SimpleIDE"; ValueType: string; ValueName: SimpleIDE_LastFileName; ValueData: "{userdocs}\Workspace\My Projects\Welcome.c"; Flags: UninsDeleteKey; 

[Code]
procedure InitializeWizard;
begin
  { Create the pages }
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
var
  S: String;
begin
  { Fill the 'Ready Memo' with the normal settings and the custom settings }
  S := '';
  S := S + MemoGroupInfo + Newline + Newline;
  S := S + MemoDirInfo + Newline + Newline;
  S := S + 'Propeller-GCC folder:' + Newline + Space + ExpandConstant('{app}\{#compiler}') + NewLine + NewLine;
  { S := S + 'SimpleIDE Workspace folder:' + NewLine + Space + 'SimpleIDE will guide user choosing location.' + NewLine; }
  Result := S;
end;

function GetCompilerDir(Param: String): String;
begin
  { Return the selected CompilerDir }
  Result := ExpandConstant('{app}\{#compiler}');
end;

function CleanPathString(OldPath: string): boolean;
var
  OrigPath: string;
  PropPath: string;
  PropIndex: integer;
  PropLen: Integer;
  Deleted: boolean;
begin
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', OrigPath)
  then begin
    Result := False;
    exit;
  end;
  
  // look for string like "C:\propgcc\bin" in PATH
  // Pos() returns 0 if not found
  PropPath := OldPath;
  PropLen := Length(PropPath);
  repeat
    PropIndex := Pos(PropPath, UpperCase(OrigPath));
    if PropIndex <> 0  then begin
      Delete( OrigPath, PropIndex, PropLen );
      Deleted := true; 
    end;  
  until PropIndex = 0;

  if Deleted then
    RegWriteExpandStringValue(HKEY_LOCAL_MACHINE,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', OrigPath)
  Result := True;
end;

{
// this doesn't seem to work! can't rely on it.
function CleanDoubleSemi(): boolean;
var
  OrigPath: string;
  Semi2: string;
  Semi2Index: integer;
  Deleted: boolean;
begin
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', OrigPath)
  then begin
    Result := False;
    exit;
  end;
  
  // remove one ; from any double ;;
  Semi2 := ';;';
  repeat
    Semi2Index := Pos(OrigPath, Semi2);
    if Semi2Index <> 0 then begin
      Delete( OrigPath, Semi2Index, 1 );
      Deleted := true; 
    end;
  until Semi2Index = 0;
  
  if Deleted then
    RegWriteExpandStringValue(HKEY_LOCAL_MACHINE,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', OrigPath)
  Result := True;
end;
}

function SetPath(NewPath: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', OrigPath)
  then begin
    Result := False;
    exit;
  end;
  
  OrigPath := OrigPath + ';' + NewPath + ';';

  RegWriteExpandStringValue(HKEY_LOCAL_MACHINE,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', OrigPath)
  Result := True;
end;
   
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE,'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'Path', OrigPath)
  then begin
    Result := True;
    exit;
  end;
  // look for the path with leading semicolon
  // Pos() returns 0 if not found
  Result := Pos(';' + UpperCase(Param) + ';', ';' + UpperCase(OrigPath)) = 0;  
  //if Result = True then
  //   Result := Pos(';' + UpperCase(Param) + '\;', ';' + UpperCase(OrigPath) + ';') = 0; 
end;

function NeedPropGccBinPath(): boolean;
var
  str: string;
begin
  CleanPathString(';C:\PROPGCC\BIN');
  CleanPathString('C:\PROPGCC\BIN'); // incase it was the first entry
  str := ExpandConstant('{app}\{#compiler}')+'\bin';
  Result := NeedsAddPath(str);
  if Result then
    SetPath(str);
  //CleanDoubleSemi(); // can't make this work
end;

