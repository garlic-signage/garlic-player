#!/bin/bash
#
# Build Qt Lib for Android in Linux
# Tutorial in https://garlic-player.com/garlic-player/docs/build-qt-lib/build-qt-android/
#
# Niko Sagiadinos ns@smil-control.com

set -e

SCRIPTDIR=$(dirname "$0")
FULL_SCRIPTDIR=$(pwd)

export QT_BASE_PATH=											# set path to your qt base directory
export QT_VERSION=5.15.10								  		# the Qt Version >= Qt5.15.8 is supported
export CONFIG_DEBUG_RELEASE=release    					    	# set if debug or release
export ANDROID_SDK_ROOT=					 					# Your full basic path to your Android SDK tools do not use ~/
export ANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk/22.1.7171670		# path to Google NDK
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk					# necessary when there is more than one sdk

##################################################################

if [ -z "QT_BASE_PATH" ]; then
	echo Error: Set the correct paths in QT_BASE_PATH
	exit 1;
fi

if [ -z "ANDROID_SDK_ROOT" ]; then
	echo Error: Set the correct paths in ANDROID_SDK_ROOT
	exit 1;
fi

export QT_SRC=$QT_BASE_PATH/$QT_VERSION/Src
export SSL_INCLUDES=$ANDROID_SDK_ROOT/android_openssl/ssl_1.1/include	# Qt maintanancetool place them here
export SSL_LIBS=$ANDROID_SDK_ROOT/android_openssl/ssl_1.1				# have also a look on https://github.com/KDAB/android_openssl
export DEV_JOBS=$(grep -c "^processor" /proc/cpuinfo)					# determine how many cores can be used
export SHADOW_BUILD_DIR=build-$QT_VERSION 								# create directory name
export PATH=$JAVA_HOME/bin:$PATH
export QT_INSTALL_DIR=$QT_BASE_PATH/$QT_VERSION/android					# where qt androd lib should be installed

##################################################################


echo
echo ========== create and enter shadowbuild dir
echo

mkdir -p $SHADOW_BUILD_DIR
cd $SHADOW_BUILD_DIR

echo
echo "== Patch Qt Source =="
echo
#activate some features for the garlic-player like video autostart etc.
WEBVIEW_PATH=$QT_SRC/qtwebview/src/jar/src/org/qtproject/qt5/android/view/
if [[ $(grep -L "webSettings.setMediaPlaybackRequiresUserGesture(false)" $WEBVIEW_PATH/QtAndroidWebViewController.java) ]]; then
	echo
	echo  "== Patch WebView =="
	echo
	cp -af $WEBVIEW_PATH/QtAndroidWebViewController.java $WEBVIEW_PATH/QtAndroidWebViewController.java_bak
	patch $WEBVIEW_PATH/QtAndroidWebViewController.java < $FULL_SCRIPTDIR/patches/webview.patch
fi

echo
echo "== Configuring qtbase =="
echo
$QT_SRC/configure -c++std c++17 -opensource -confirm-license -nomake tests -nomake examples \
    -xplatform android-clang  -release \
    -ssl -openssl-runtime -I $SSL_INCLUDES -L $SSL_LIBS \
    -prefix $QT_INSTALL_DIR \
    -android-sdk $ANDROID_SDK_ROOT \
    -android-ndk $ANDROID_NDK_ROOT \
    -skip qtserialport -skip qtwebengine \
    -no-warnings-are-errors -disable-rpath

echo
echo "== Building qt =="
echo
make -j$DEV_JOBS

if [ $? -eq 0 ] ; then
	echo
	echo "== Installing qt =="
	echo
	make -j$DEV_JOBS install
else
	echo make failed
	exit 1
fi
