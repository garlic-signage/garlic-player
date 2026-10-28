@echo off
rem =============================================== 
rem
rem garlic-player build script for 64 Bit windows
rem using Visual Studio 2017 Community Edition
rem 
rem You will need a 7Zip installation
rem 
rem This helper files/dirs  
rem dx_sdk2010_xaudio2_redist, vc_redist.x64.exe for ,
rem Visual Studio 2017,  libeay32.dll and ssleay.dll
rem must be present in %DEPLOYMENT_HELPER_PATH% 
rem
rem Visual Studio 2017 is recommended, cause prebuild
rem QWebengine binary is compiled with VS2017 only 
rem in Qt5.12.x
rem 
rem =============================================== 

rem =============================================== 
rem Configure this to your own paths
rem =============================================== 

rem path to your qt base directory
set QT_BASE_PATH=C:\Qt
rem directory of garlic-player
set GARLIC_DIR=
rem The Qt Version 5.10, 5.11, 5.12.2 etc
set QT_VERSION=5.12.6
rem debug or release												
set CONFIG_DEBUG_RELEASE=release
rem dx_sdk2010_xaudio2_redist-dir, vc_redist.x64.exe, libeay32.dll, ssleay.dll
set DEPLOYMENT_HELPER_PATH=C:\garlic-deployment
set SEVEN_ZIP_PATH="C:\Program Files\7-Zip\7z.exe"

rem =============================================== 
rem check if script si executedin a VS Native CMD
rem =============================================== 
if "%VSINSTALLDIR%" =="" (
	CALL "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
)

rem =============================================== 
rem check if neccessary heplper are present 
rem =============================================== 
if "%DEPLOYMENT_HELPER_PATH%" == "" (
	@echo on
	echo Error: Set the correct paths for DEPLOYMENT_HELPER_PATH 
	@echo off
	goto END
)

if "%QT_BASE_PATH%" == "" (
	@echo on
	echo Error: Set the correct paths for QT_BASE_PATH 
	@echo off
	goto END
)

if not exist %SEVEN_ZIP_PATH% (
	@echo on
	echo Error: You need 7zip from http://www.7-zip.org/
	@echo off
	goto END
)

rem =============================================== 
rem determine version and commit number from git 
rem and put it into a version.h
rem =============================================== 
set HOME=%USERPROFILE%

for /f "delims=" %%r in ('git --git-dir=%GARLIC_DIR%\.git rev-list --all --count') do set COMMIT_NUMBER=%%r
for /f "delims=" %%r in ('git --git-dir=%GARLIC_DIR%\.git rev-list --tags --max-count=1') do set TAGS=%%r
for /f "delims=" %%r in ('git --git-dir=%GARLIC_DIR%\.git describe --tags %TAGS%') do set VERSION_NAME=%%r

set DEPLOY_SUFFIX=%VERSION_NAME%.%COMMIT_NUMBER%
echo #define version_from_git "%DEPLOY_SUFFIX%" > %GARLIC_DIR%\src\garlic-lib\version.h

rem =============================================== 
rem start build
rem =============================================== 

set SHADOW_BUILD_DIR=msvc2017_64-build-%QT_VERSION%-%CONFIG_DEBUG_RELEASE%
set QT_BASE_PATH_RUNTIME=%QT_BASE_PATH%\%QT_VERSION%\msvc2017_64

if not exist %SHADOW_BUILD_DIR% mkdir %SHADOW_BUILD_DIR%
cd %SHADOW_BUILD_DIR%

%QT_BASE_PATH_RUNTIME%\bin\qmake.exe "%GARLIC_DIR%\src\complete.pro"
%QT_BASE_PATH%\Tools\QtCreator\bin\jom.exe
if not exist bin/garlic-player.exe (
	@echo on
	echo Error: compile failed
	@echo off
	goto END
)

%QT_BASE_PATH_RUNTIME%\bin\windeployqt.exe -opengl -webengine bin\garlic-player.exe

rem =============================================== 
rem copy missing Qt-Dlls which windeplayqt seems is
rem not able to handle
rem =============================================== 
copy /Y  %QT_BASE_PATH_RUNTIME%\bin\Qt5WebSockets.dll bin\*
copy /Y  %QT_BASE_PATH_RUNTIME%\bin\Qt5WebView.dll bin\*
copy /Y  %QT_BASE_PATH_RUNTIME%\bin\Qt5XmlPatterns.dll bin\*

rem =============================================== 
rem copy QtAV-Dlls
rem =============================================== 
copy /Y  %QT_BASE_PATH_RUNTIME%\bin\QtAV1.dll bin\*
copy /Y  %QT_BASE_PATH_RUNTIME%\bin\QtAVWidgets1.dll bin\*

rem =============================================== 
rem copy ffmpeg dlls
rem =============================================== 
copy /Y %QT_BASE_PATH_RUNTIME%\bin\av*.dll bin\*
copy /Y  %QT_BASE_PATH_RUNTIME%\bin\sw*.dll bin\*
copy /Y  %DEPLOYMENT_HELPER_PATH%\libcrypto-1_1-x64.dll bin\*
copy /Y  %DEPLOYMENT_HELPER_PATH%\libssl-1_1-x64.dll bin\*

if not exist deployment mkdir deployment
xcopy %GARLIC_DIR%\deployment\win\* deployment\* /sy

rem =============================================== 
rem create copy of the Visual Studio runtime
rem =============================================== 
if not exist deployment\packages\com.microsoft.vcredist.x64\data mkdir deployment\packages\com.microsoft.vcredist.x64\data
copy /Y %DEPLOYMENT_HELPER_PATH%\bin\vcredist_x64.exe deployment\packages\com.microsoft.vcredist.x64\data

rem =============================================== 
rem create copy xaudio2 install
rem =============================================== 
if not exist deployment\packages\com.microsoft.directx.xaudio2\data mkdir deployment\packages\com.microsoft.directx.xaudio2\data
copy /Y %DEPLOYMENT_HELPER_PATH%\dx_sdk2010_xaudio2_redist\* deployment\packages\com.microsoft.directx.xaudio2\data\*

rem =============================================== 
rem create/copy bin data icon and start-batch into install dir
rem =============================================== 
if not exist deployment\packages\com.sagiadinos.garlic.x64\data\bin mkdir deployment\packages\com.sagiadinos.garlic.x64\data\bin
xcopy bin\* deployment\packages\com.sagiadinos.garlic.x64\data\bin\* /sy

copy %GARLIC_DIR%\deployment\win\garlic-player.ico deployment\packages\com.sagiadinos.garlic.x64\data\bin
copy /Y %GARLIC_DIR%\deployment\win\start_garlic.bat deployment\packages\com.sagiadinos.garlic.x64\data

rem =============================================== 
rem create install archive
rem =============================================== 
%QT_BASE_PATH%\Tools\QtInstallerFramework\3.0\bin\binarycreator -c  deployment\config\config.xml -p  deployment\packages garlic-player-win64-%DEPLOY_SUFFIX%.exe

rem =============================================== 
rem create 7zip archive (7Zip is needed to installed)
rem =============================================== 
%SEVEN_ZIP_PATH% a garlic-player-win64-%DEPLOY_SUFFIX%.7z  .\deployment\packages\com.sagiadinos.garlic.x64\data\*

cd ..
Exit /B 0

:END
cd ..
Exit /B 1
