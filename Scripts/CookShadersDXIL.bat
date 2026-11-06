@echo off
rem Generates the DXIL shader cache in Build/ShaderCache. Can be run from any folder.
setlocal
cd /d "%~dp0.." || exit /b 1

call "%~dp0SetupMSVC.bat" || exit /b 1

if not exist "Build\ninja-tools-msvc\build.ninja" cmake --preset ninja-tools-msvc || exit /b 1
cmake --build Build\ninja-tools-msvc --target CookShaders
exit /b %errorlevel%
