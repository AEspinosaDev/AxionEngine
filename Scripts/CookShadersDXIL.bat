@echo off
rem Generates the DXIL shader cache in Build/ShaderCache. Can be run from any folder.
setlocal
cd /d "%~dp0.." || exit /b 1

rem Ninja needs the MSVC environment
where cl >nul 2>nul && goto :compiler_ready
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
where cl >nul 2>nul && goto :compiler_ready
echo MSVC not found: install Visual Studio with the C++ tools or run this from a Developer Command Prompt
exit /b 1

:compiler_ready
if not exist "Build\ninja-tools-msvc\build.ninja" cmake --preset ninja-tools-msvc || exit /b 1
cmake --build Build\ninja-tools-msvc --target CookShaders
exit /b %errorlevel%
