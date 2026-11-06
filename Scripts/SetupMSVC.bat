@echo off
rem Loads the MSVC environment in the session of the caller, if it isn't there already. Ninja needs it.
where cl >nul 2>nul && exit /b 0
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
where cl >nul 2>nul && exit /b 0
echo MSVC not found: install Visual Studio with the C++ tools or run this from a Developer Command Prompt
exit /b 1
