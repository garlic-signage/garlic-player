@echo off
if "%VSINSTALLDIR%" =="" (
	CALL "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
)

SET FFMPEG=C:\ffmpeg
SET PATH=%FFMPEG%\lib\x64;%FFMPEG%\bin;%PATH%
SET INCLUDE=%FFMPEG%\include;%INCLUDE%
SET LIB=%FFMPEG%\lib\x64;%FFMPEG%\lib\x64;%LIB%

set SHADOW_BUILD_DIR=build
if not exist %SHADOW_BUILD_DIR% mkdir %SHADOW_BUILD_DIR%
cd %SHADOW_BUILD_DIR%

"C:\Qt\5.12.12\msvc2017_64\bin\qmake.exe" "C:\Users\Niko\Desktop\compile\QtAVPlayer\QtAVPlayer.pro"
"C:\Qt\Tools\QtCreator\bin\jom.exe"

rem "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\bin\Hostx64\x64\nmake.exe"

cd ..

