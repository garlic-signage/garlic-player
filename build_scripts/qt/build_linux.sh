#!/bin/bash
#
# Build Qt Lib for macOS in Ventura
# Tutorial in https://garlic-player.com/garlic-player/docs/build-qt-lib/build-qt-linux-with-qtwebengine/
#
# Niko Sagiadinos ns@smil-control.com
set -e

SCRIPTDIR=$(dirname "$0")
FULL_SCRIPTDIR=$(pwd)



export QT_BASE_PATH=							           	# path to your qt base directory
export QT_VERSION=5.15.19								    # The Qt Version
export CONFIG_DEBUG_RELEASE=release    					    # set if debug or release

##################################################################

if [ -z "QT_BASE_PATH" ]; then
	echo Error: Set the correct paths in QT_BASE_PATH
	exit 1;
fi

export QT_SRC=$QT_BASE_PATH/$QT_VERSION/Src
export DEV_JOBS=$(grep -c "^processor" /proc/cpuinfo)			# determine how many cores can be used
export SHADOW_BUILD_DIR=build-$QT_VERSION	 					# shadow directory name
export QT_INSTALL_DIR=$QT_BASE_PATH/$QT_VERSION/gcc_64			# where Qt  lib will be installed

##################################################################

echo
echo ========== create and enter shadowbuild dir
echo
mkdir -p $SHADOW_BUILD_DIR
cd $SHADOW_BUILD_DIR

echo
echo "== Configuring qtbase =="
echo
$QT_SRC/configure -c++std c++17 -opensource -confirm-license -nomake tests -nomake examples \
    -ssl -openssl-runtime -make libs \
    -prefix $QT_INSTALL_DIR \
    -skip qttranslations -skip qtserialport -skip qtdoc -skip qtpurchasing -skip qtvirtualkeyboard -skip qtspeech \
	-skip qtwebengine

echo
echo "== Building qt =="
echo
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

echo
echo ========== enter new shadowbuild dir
echo

$QT_INSTALL_DIR/bin/qmake $QT_SRC/qtwebengine/qtwebengine.pro -- -no-webengine-jumbo-build -no-feature-webengine-system-ninja -proprietary-codecs

echo
echo "== Building qt =="
echo

export NINJAFLAGS=-j$DEV_JOBS  # limit this if get crashes about memory exceeds

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
