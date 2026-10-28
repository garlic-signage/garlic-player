#!/bin/bash

set -e

SCRIPTDIR=$(dirname "$0")
source $SCRIPTDIR/envLinux64.sh

echo 
echo =========== collect files ============ 
echo 

export GARLIC_DEPLOY_BASE_PATH=deb/garlic-player
export GARLIC_DEPLOY_PATH=$GARLIC_DEPLOY_BASE_PATH/opt

mkdir -p $GARLIC_DEPLOY_PATH $GARLIC_DEPLOY_BASE_PATH
cp -r $GARLIC_DIR/deployment/deb/garlic-player/* $GARLIC_DEPLOY_BASE_PATH

mkdir -p $GARLIC_DEPLOY_PATH/garlic-player/bin/platforms
cp $QT_PATH_RUNTIME/plugins/platforms/libqeglfs.so $GARLIC_DEPLOY_PATH/garlic-player/bin/platforms/libqeglfs.so
cp $QT_PATH_RUNTIME/plugins/platforms/libqxcb.so $GARLIC_DEPLOY_PATH/garlic-player/bin/platforms/libqxcb.so

mkdir -p $GARLIC_DEPLOY_PATH/garlic-player/bin/plugins/iconengines
cp $QT_PATH_RUNTIME/plugins/iconengines/* $GARLIC_DEPLOY_PATH/garlic-player/bin/plugins/iconengines

mkdir -p $GARLIC_DEPLOY_PATH/garlic-player/bin/plugins/imageformats
cp $QT_PATH_RUNTIME/plugins/imageformats/* $GARLIC_DEPLOY_PATH/garlic-player/bin/plugins/imageformats

mkdir -p $GARLIC_DEPLOY_PATH/garlic-player/bin/plugins/xcbglintegrations
cp $QT_PATH_RUNTIME/plugins/xcbglintegrations/* $GARLIC_DEPLOY_PATH/garlic-player/bin/plugins/xcbglintegrations

mkdir -p $GARLIC_DEPLOY_PATH/garlic-player/bin/qtwebengine_locales
cp $QT_PATH_RUNTIME/translations/qtwebengine_locales/* $GARLIC_DEPLOY_PATH/garlic-player/bin/qtwebengine_locales

mkdir -p $GARLIC_DEPLOY_PATH/garlic-player/bin/resources
cp $QT_PATH_RUNTIME/resources/icudtl.dat $GARLIC_DEPLOY_PATH/garlic-player/bin/resources
cp $QT_PATH_RUNTIME/resources/qtwebengine_resources_100p.pak $GARLIC_DEPLOY_PATH/garlic-player/bin/resources
cp $QT_PATH_RUNTIME/resources/qtwebengine_resources_200p.pak $GARLIC_DEPLOY_PATH/garlic-player/bin/resources
cp $QT_PATH_RUNTIME/resources/qtwebengine_resources.pak $GARLIC_DEPLOY_PATH/garlic-player/bin/resources

cp $QT_PATH_RUNTIME/libexec/QtWebEngineProcess $GARLIC_DEPLOY_PATH/garlic-player/bin
printf "[Paths]\nPrefix=." > $GARLIC_DEPLOY_PATH/garlic-player/bin/qt.conf

mkdir -p $GARLIC_DEPLOY_PATH/garlic-player/bin/lib/fonts
cp $GARLIC_DIR/fonts/* $GARLIC_DEPLOY_PATH/garlic-player/bin/lib/fonts

mkdir -p $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libav* $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -P $QT_PATH_RUNTIME/lib/libicudata.* $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -P $QT_PATH_RUNTIME/lib/libicui18n.* $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -P $QT_PATH_RUNTIME/lib/libicuio.* $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -P $QT_PATH_RUNTIME/lib/libicuuc.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5Core.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5DBus.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5Gui.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5Network.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5OpenGL.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5Positioning.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5PrintSupport.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5Qml.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5Quick.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5QuickWidgets.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5Sql.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5Svg.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5WebChannel.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5WebEngine.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5WebEngineCore.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5WebEngineWidgets.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5WebSockets.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5Widgets.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5XcbQpa.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5Xml.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5XmlPatterns.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQt5X11Extras.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQtAV.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libQtAVWidgets.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libswresample.* $GARLIC_DEPLOY_PATH/garlic-player/lib
cp -P $QT_PATH_RUNTIME/lib/libswscale.* $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -L /usr/lib/x86_64-linux-gnu/libstdc++.so.6 $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -L /usr/lib/x86_64-linux-gnu/libevent-2.0.* $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -L /usr/lib/x86_64-linux-gnu/libminizip.so.1 $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -L /usr/lib/x86_64-linux-gnu/libopus.so.0 $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -L /usr/lib/x86_64-linux-gnu/libsnappy.so.1 $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -L /usr/lib/x86_64-linux-gnu/libwebp.so.5 $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -L /usr/lib/x86_64-linux-gnu/libwebpdemux.so.1 $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -L /usr/lib/x86_64-linux-gnu/libxcb-shm.so.0 $GARLIC_DEPLOY_PATH/garlic-player/lib
#cp -L /usr/lib/x86_64-linux-gnu/libXss.so.1 $GARLIC_DEPLOY_PATH/garlic-player/lib


echo 
echo =========== copy created binary to bin path ============ 
echo 

cp bin/garlic-player $GARLIC_DEPLOY_PATH/garlic-player/bin/
mv $GARLIC_DEPLOY_BASE_PATH/garlic.sh $GARLIC_DEPLOY_PATH/garlic-player
cp -P lib/libgarlic.so* $GARLIC_DEPLOY_PATH/garlic-player/lib


echo 
echo =========== create deb ============ 
echo 

dpkg -b $GARLIC_DEPLOY_BASE_PATH/ garlic-player-linux-x64-$DEPLOY_SUFFIX.deb

echo 
echo =========== create tar.xz ============ 
echo 
tar -cJf garlic-player-linux-x64-$DEPLOY_SUFFIX.tar.xz -C deb/garlic-player/opt .
