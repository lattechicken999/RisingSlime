@echo off
REM Fully quit Unreal Editor / Live Coding before running this.
setlocal
set ENGINE=D:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat
set PROJECT=D:\UnrealGames\RisingSlime\RisingSlime.uproject
echo Building RaisingSlimeEditor Win64 Development...
call "%ENGINE%" RaisingSlimeEditor Win64 Development -Project="%PROJECT%" -WaitMutex
echo EXITCODE=%ERRORLEVEL%
if %ERRORLEVEL% NEQ 0 (
  echo BUILD FAILED. If DLL is locked, close all UnrealEditor / LiveCodingConsole windows and retry.
  exit /b %ERRORLEVEL%
)
echo BUILD OK. Open RisingSlime.uproject, then PIE and test WASD.
endlocal
