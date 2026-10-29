@echo off
setlocal
set "GAME_ROOT=%~dp0"
set "GAME_EXE=%GAME_ROOT%build\native\walk_window.exe"
if not exist "%GAME_EXE%" (
  echo Compile o jogo primeiro seguindo README-PC.md.
  pause
  exit /b 1
)
if not exist "%GAME_ROOT%build\native\settings.json" (
  copy /y "%GAME_ROOT%pc\default-settings.json" "%GAME_ROOT%build\native\settings.json" >nul
  if errorlevel 1 exit /b 1
)
if not exist "%GAME_ROOT%build\user\save" mkdir "%GAME_ROOT%build\user\save"
set "SM64DS_ASSET_ROOT=%GAME_ROOT%"
set "SM64DS_ONE_SCREEN_MENUS=1"
set "SM64DS_SUB_PANEL=0"
set "SM64DS_SAVE_PATH=%GAME_ROOT%build\user\save\sm64ds.sav"
pushd "%GAME_ROOT%build\native"
"%GAME_EXE%"
set "GAME_EXIT=%ERRORLEVEL%"
popd
if not "%GAME_EXIT%"=="0" (
  echo O jogo terminou com codigo %GAME_EXIT%.
  pause
)
exit /b %GAME_EXIT%
