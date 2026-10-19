@echo off
REM Bulletproof RisingSlime editor open ? always absolute uproject + UE_5.8
setlocal
set "UE=D:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set "PROJ=D:\UnrealGames\RisingSlime\RisingSlime.uproject"
if not exist "%UE%" (
  echo ERROR: UnrealEditor not found: %UE%
  exit /b 1
)
if not exist "%PROJ%" (
  echo ERROR: Project not found: %PROJ%
  exit /b 1
)
REM Clear AutoLoad InProgress lock if a prior crash left it
del /q "%LOCALAPPDATA%\UnrealEngine\5.8\Saved\AutoLoadProject.txt.InProgress" 2>nul
REM Keep AutoLoad absolute for Epic "Launch" with no args
echo D:/UnrealGames/RisingSlime/RisingSlime.uproject> "%LOCALAPPDATA%\UnrealEngine\5.8\Saved\AutoLoadProject.txt"
start "" "%UE%" "%PROJ%" %*
