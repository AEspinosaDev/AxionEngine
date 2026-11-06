@echo off
rem Generates the distribution package of an application in Dist/<Target>. Can be run from any folder.
rem The target must call axion_add_distribution in its CMakeLists.
setlocal
if "%~1"=="" goto :usage

rem The package always gets a shader cache built from the current sources
call "%~dp0CookShadersDXIL.bat" || exit /b 1

cd /d "%~dp0.." || exit /b 1
call "%~dp0SetupMSVC.bat" || exit /b 1

if not exist "Build\ninja-dist-msvc\build.ninja" cmake --preset ninja-dist-msvc || exit /b 1
cmake --build Build\ninja-dist-msvc --target Package_%~1
exit /b %errorlevel%

:usage
echo Usage: PackageApp.bat ^<Target^>
echo Example: PackageApp.bat CoreGUITest
exit /b 2
