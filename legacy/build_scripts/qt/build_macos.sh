#!/bin/bash
#
# Build Qt Lib for macOS in Ventura
# Tutorial in https://garlic-player.com/garlic-player/docs/build-qt-lib/build-qt-macos-with-qtwebengine/
#
# Niko Sagiadinos ns@smil-control.com

set -e
SCRIPTDIR=$(dirname "$0")

export QT_BASE_PATH=											# path to your qt base directory
export QT_VERSION=5.15.11										# the Qt Version >= Qt5.15.8 is supported
export CONFIG_DEBUG_RELEASE=release   		 				 	# set if debug or release
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer # set Xcode Dir if more than one
export DEV_JOBS=8												# lower this if you run out of memory during webengine compilation

##################################################################

if [ -z "QT_BASE_PATH" ]; then
	echo Error: Set the correct paths in QT_BASE_PATH
	exit 1;
fi

export QT_SRC=$QT_BASE_PATH/$QT_VERSION/Src
export QT_INSTALL_DIR=$QT_BASE_PATH/$QT_VERSION/clang_64			#
export SHADOW_BUILD_DIR=build-$QT_VERSION										 # create directory name

##################################################################

echo
echo ========== enter shadowbuild dir
echo
mkdir -p $SHADOW_BUILD_DIR
cd $SHADOW_BUILD_DIR

#-c++std c++17 -make libs -no-pch

echo
echo "== Configuring qtbase =="
echo
$QT_SRC/configure QMAKE_APPLE_DEVICE_ARCHS="x86_64 arm64" \
    -release -optimize-size \
	-opensource -confirm-license -nomake tests -nomake examples \
    -no-openssl -securetransport \
    -prefix $QT_INSTALL_DIR \
    -webengine-proprietary-codecs \
    -skip qttranslations \
    -skip qtserialport \
    -skip qtwayland \
    -skip qtdoc \
    -skip qtpurchasing \
    -skip qtvirtualkeyboard \
    -skip qtspeech

echo
echo "== Building qt =="
echo

export NINJAFLAGS=-j$DEV_JOBS
make -j$DEV_JOBS
if [ $? -eq 0 ] ; then
	echo
	echo "== Installing qt =="
	echo
	make install
else
	echo make failed
	exit 1
fi
