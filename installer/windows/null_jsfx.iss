; NULL JSFX installer for Windows (Inno Setup 6). The user picks the formats (JSFX, JSFX GFX,
; VST3, CLAP), the plugins (by group or one by one) and the extras.
;
; tools/build_installer.py --os Windows copies this file next to components.iss, which holds the
; plugin tree and every file with the components it belongs to; then
;     iscc /DAppVersion=<version> null_jsfx.iss
; makes NULL-JSFX-Installer-Windows.exe. Unattended install (used by CI), e.g.
;     NULL-JSFX-Installer-Windows.exe /VERYSILENT /COMPONENTS="fmt\vst3,*fx\oneslider" /REAPERDIR="D:\REAPER"

#ifndef AppVersion
  #define AppVersion "dev"
#endif
; x64 or arm64: build_installer.py --arch writes the define at the top of the copy it makes
#ifndef Arch
  #define Arch "x64"
#endif

[Setup]
AppId={{287A1049-D196-490D-AD87-B04B0757988D}
AppName=NULL JSFX
AppVersion={#AppVersion}
AppVerName=NULL JSFX {#AppVersion}
AppPublisher=NULL JSFX
AppPublisherURL=https://nulljsfx.tech
AppSupportURL=https://nulljsfx.tech
AppUpdatesURL=https://github.com/koulaxizis/null-jsfx/releases
DefaultDirName={autopf}\NULL JSFX
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=NULL JSFX
UninstallDisplayIcon={app}\null_jsfx.ico
SetupIconFile=null_jsfx.ico
OutputDir=.
#if Arch == "arm64"
OutputBaseFilename=NULL-JSFX-Installer-Windows-ARM64
; ARM64 plugins load only in ARM64 hosts, so this installer runs only on Windows on Arm
ArchitecturesAllowed=arm64
ArchitecturesInstallIn64BitMode=arm64
#else
OutputBaseFilename=NULL-JSFX-Installer-Windows
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#endif
Compression=lzma2/max
SolidCompression=yes
; VST3 and CLAP belong in C:\Program Files\Common Files
PrivilegesRequired=admin
UsedUserAreasWarning=no
WizardStyle=modern
WizardSizePercent=120

[Types]
Name: full; Description: "Everything"
Name: reaper; Description: "REAPER only (JSFX and JSFX GFX)"
Name: daw; Description: "Any DAW (VST3 and CLAP)"
Name: custom; Description: "Custom"; Flags: iscustom

[Components]
Name: fmt; Description: "Formats"; Types: full reaper daw
Name: fmt\jsfx; Description: "JSFX: REAPER, with REAPER's own sliders"; Types: full reaper
Name: fmt\gfx; Description: "JSFX GFX: REAPER, with the NULL JSFX interface"; Types: full reaper
Name: fmt\vst3; Description: "VST3: any DAW"; Types: full daw
Name: fmt\clap; Description: "CLAP: any DAW"; Types: full daw
Name: extra; Description: "Extras"; Types: full reaper daw
Name: extra\chains; Description: "FX chains for REAPER (added when their plugins are)"; Types: full reaper
Name: extra\rack; Description: "Rack: up to 8 NULL JSFX effects in one VST3/CLAP"; Types: full daw
Name: extra\remote; Description: "Tablet remote for REAPER (a few steps in REAPER after)"; Types: full
Name: extra\fonts; Description: "Noto Sans font, for the exact look of the GFX versions"; Types: full reaper
Name: fx; Description: "Plugins"; Types: full reaper daw

#include "components.iss"

[Files]
Source: "null_jsfx.ico"; DestDir: "{app}"; Flags: ignoreversion

[Messages]
WelcomeLabel2=This installs NULL JSFX {#AppVersion}: free audio plugins as JSFX for REAPER and as VST3 and CLAP for any DAW. On the next pages you pick the formats, the plugins (all, a group like One Slider, or single ones) and the extras.%n%nWant to try them first? The same effects run in your browser as web plugins in orOS Studio: useoros.online/studio
SelectComponentsDesc=Pick the formats, the plugins and the extras.
SelectComponentsLabel2=Tick the formats you need and the plugins you want, by group or one by one. Click Next when you are ready.
FinishedLabel=NULL JSFX is installed. REAPER: open the FX browser and press F5 (or restart REAPER). Other DAWs: rescan plugins; they are listed under NULL JSFX.

[Code]
var
  ReaperPage: TInputDirWizardPage;
  StudioLink: TNewStaticText;
  SupportText: TNewStaticText;
  SupportLink: TNewStaticText;

function ReaperDir(Param: String): String;
begin
  Result := ReaperPage.Values[0];
end;

function NeedsReaper: Boolean;
begin
  Result := WizardIsComponentSelected('fmt\jsfx') or WizardIsComponentSelected('fmt\gfx') or
    WizardIsComponentSelected('extra\remote');
end;

procedure StudioClick(Sender: TObject);
var
  Code: Integer;
begin
  ShellExecAsOriginalUser('open', 'https://useoros.online/studio/', '', '', SW_SHOWNORMAL, ewNoWait, Code);
end;

procedure KofiClick(Sender: TObject);
var
  Code: Integer;
begin
  ShellExecAsOriginalUser('open', 'https://ko-fi.com/koulaxizis', '', '', SW_SHOWNORMAL, ewNoWait, Code);
end;

procedure InitializeWizard;
begin
  ReaperPage := CreateInputDirPage(wpSelectComponents, 'REAPER resource folder',
    'Where should the REAPER parts go?',
    'The JSFX, FX chains and remote go into the REAPER resource folder (in REAPER: Options > ' +
    'Show REAPER resource path). Change it for a portable REAPER, then click Next.',
    False, '');
  ReaperPage.Add('');
  ReaperPage.Values[0] := ExpandConstant('{param:REAPERDIR|{userappdata}\REAPER}');

  StudioLink := TNewStaticText.Create(WizardForm);
  StudioLink.Parent := WizardForm.FinishedPage;
  StudioLink.Caption := 'Try the effects in your browser too: orOS Studio web plugins';
  StudioLink.Cursor := crHand;
  StudioLink.Font.Style := [fsUnderline];
  StudioLink.Font.Color := clHotLight;
  StudioLink.OnClick := @StudioClick;

  SupportText := TNewStaticText.Create(WizardForm);
  SupportText.Parent := WizardForm.FinishedPage;
  SupportText.AutoSize := False;
  SupportText.WordWrap := True;
  SupportText.Caption := 'NULL JSFX is free and made by one person. If it helps your music, ' +
    'a coffee keeps it going. Follow it on Ko-fi for news of every new version:';
  SupportLink := TNewStaticText.Create(WizardForm);
  SupportLink.Parent := WizardForm.FinishedPage;
  SupportLink.Caption := 'Support and follow NULL JSFX on Ko-fi';
  SupportLink.Cursor := crHand;
  SupportLink.Font.Style := [fsUnderline];
  SupportLink.Font.Color := clHotLight;
  SupportLink.OnClick := @KofiClick;
end;

{ Adds the factory presets in Src (a REAPER user preset file, presets\js-*.ini) to Dst in REAPER's
  presets folder. A missing Dst is simply copied; an existing one holds presets the user saved, so
  ours go after them, and names that are already there are left alone. }
procedure MergePresets(Src, Dst: String);
var
  N, M, I, J: Integer;
  Name, Sec: String;
  Found: Boolean;
begin
  if not FileExists(Dst) then
  begin
    ForceDirectories(ExtractFileDir(Dst));
    FileCopy(Src, Dst, False);
    exit;
  end;
  N := GetIniInt('General', 'NbPresets', 0, 0, 0, Dst);
  M := GetIniInt('General', 'NbPresets', 0, 0, 0, Src);
  for I := 0 to M - 1 do
  begin
    Sec := 'Preset' + IntToStr(I);
    Name := GetIniString(Sec, 'Name', '', Src);
    Found := False;
    for J := 0 to N - 1 do
      if GetIniString('Preset' + IntToStr(J), 'Name', '', Dst) = Name then
        Found := True;
    if not Found then
    begin
      SetIniString('Preset' + IntToStr(N), 'Data', GetIniString(Sec, 'Data', '', Src), Dst);
      SetIniString('Preset' + IntToStr(N), 'Len', GetIniString(Sec, 'Len', '', Src), Dst);
      SetIniString('Preset' + IntToStr(N), 'Name', Name, Dst);
      N := N + 1;
    end;
  end;
  SetIniInt('General', 'NbPresets', N, Dst);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Find: TFindRec;
  Dir: String;
begin
  Dir := ExpandConstant('{app}\presets');
  { only the preset files of the plugins picked this time }
  if CurStep = ssInstall then
    DelTree(Dir, True, True, True);
  if (CurStep = ssPostInstall) and FindFirst(Dir + '\js-*.ini', Find) then
  begin
    try
      repeat
        MergePresets(Dir + '\' + Find.Name, ReaperDir('') + '\presets\' + Find.Name);
      until not FindNext(Find);
    finally
      FindClose(Find);
    end;
  end;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = ReaperPage.ID) and not NeedsReaper;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then
  begin
    StudioLink.Left := WizardForm.FinishedLabel.Left;
    StudioLink.Top := WizardForm.FinishedLabel.Top + WizardForm.FinishedLabel.Height + ScaleY(16);
    SupportText.Left := StudioLink.Left;
    SupportText.Width := WizardForm.FinishedLabel.Width;
    SupportText.Height := ScaleY(30);
    SupportText.Top := StudioLink.Top + StudioLink.Height + ScaleY(20);
    SupportLink.Left := StudioLink.Left;
    SupportLink.Top := SupportText.Top + SupportText.Height + ScaleY(4);
  end;
end;
