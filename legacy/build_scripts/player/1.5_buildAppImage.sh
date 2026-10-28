#!/bin/bash

set -e

SCRIPTDIR=$(dirname "$0")
source $SCRIPTDIR/envLinux64.sh

##################################################################

if [ -z "QT_BASE_PATH" ]; then
	echo Error: Set the correct paths in QT_BASE_PATH
	exit 1;
fi

export QT_HOME=$QT_BASE_PATH/$QT_VERSION/gcc_64
export PATH=$QT_HOME/bin:$PATH
export GARLIC_DEPLOY_BASE_PATH=./AppDir/usr

##################################################################

echo
echo =========== create AppDir structure ============
echo
mkdir -p $GARLIC_DEPLOY_BASE_PATH
mkdir -p $GARLIC_DEPLOY_BASE_PATH/bin
mkdir -p $GARLIC_DEPLOY_BASE_PATH/lib
cp -r $GARLIC_DIR/deployment/deb/garlic-player/usr/* $GARLIC_DEPLOY_BASE_PATH

echo
echo =========== copy created binaries to paths ============
echo

cp -P bin/garlic-player $GARLIC_DEPLOY_BASE_PATH/bin/
cp -P lib/libgarlic.so* $GARLIC_DEPLOY_BASE_PATH/lib

# linuxdeployqt cannot find some lib
# look at https://github.com/probonopd/linuxdeployqt/issues/35
cp -P /usr/lib/x86_64-linux-gnu/nss/* $GARLIC_DEPLOY_BASE_PATH/lib

echo
echo =========== create structure ============
echo
~/BuildEssentials/linuxdeployqt-continuous-x86_64.AppImage $GARLIC_DEPLOY_BASE_PATH/share/applications/garlic-player.desktop \
 -no-translations \
 -bundle-non-qt-libs \
 -appimage


echo 
echo =========== rename file ============
echo 

mv Garlic_Player-x86_64.AppImage garlic-player-linux-x64-$DEPLOY_SUFFIX.AppImage
