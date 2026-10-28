@echo off
rem =============================================== 
rem
rem Qt build script for 64 Bit windows
rem using Visual Studio 2019 Community Edition
rem
rem For use in Powershell and standard Command line
rem Tutorial in https://garlic-player.com/garlic-player/docs/build-qt-lib/build-qt-windows-with-qtwebengine/
rem =============================================== 

rem path to your qt base directory
set QT_BASE_PATH=C:\Qt

set QT_SRC=C:\Qt\qt-everywhere-src-5.15.11
set PATH=%QT_SRC%\qtbase\bin;%QT_SRC%\gnuwin32\bin;%PATH%

rem =============================================== 
rem check if script si executedin a VS Native CMD
rem =============================================== 
if "%VSINSTALLDIR%" =="" (
	CALL "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
)
set LLVM_INSTALL_DIR=C:\BuildEssentials\libclang
set PATH=C:\BuildEssentials\libclang\bin;%PATH%

set SHADOW_BUILD_DIR=build_qt

if not exist %SHADOW_BUILD_DIR% mkdir %SHADOW_BUILD_DIR%
cd %SHADOW_BUILD_DIR%

%QT_SRC%\configure  -platform win32-msvc -opensource -confirm-license ^
 -I "C:\Program Files\OpenSSL-Win64\include" -L "C:\Program Files\OpenSSL-Win64\lib" ^
 -prefix C:\Qt\5.15.11\msvc2019_64 ^
 -release -nomake examples -nomake tests -silent ^
 -skip qtwayland -skip qtactiveqt -skip qtserialport ^
 -skip qtwebengine


set CL=/MP
nmake

nmake install

cd ..

set "PERL_ROOT=C:\BuildEssentials\Strawberry\perl\bin"
set "PYTHON_ROOT=C:\BuildEssentials\Python27"
set "GNU_ROOT=C:\Program Files (x86)\GnuWin32\bin"
set "NODEJS_ROOT=C:\Program Files\nodejs"

set PATH=%PERL_ROOT%;%PYTON_ROOT%;%GNU_ROOT%;%NODEJS_ROOT%;%PATH%

set SHADOW_BUILD_DIR=build_qt_webengine

if not exist %SHADOW_BUILD_DIR% mkdir %SHADOW_BUILD_DIR%
cd %SHADOW_BUILD_DIR%

set CL_MPCount=16
set NINJAFLAGS=-j16

C:\Qt\5.15.11\msvc2019_64\bin\qmake %QT_SRC%\qtwebengine\qtwebengine.pro -- -no-webengine-jumbo-build -no-feature-webengine-system-ninja -proprietary-codecs

nmake

nmake install

cd ..

Exit /B 0

cd ..
Exit /B 0
