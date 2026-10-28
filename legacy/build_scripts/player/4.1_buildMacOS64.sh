#!/bin/bash

set -e
SCRIPTDIR=$(dirname "$0")
source $SCRIPTDIR/env.sh

export QT_PATH_RUNTIME=$QT_BASE_PATH/$QT_VERSION/clang_64
export SHADOW_BUILD_DIR="clang_64-$SHADOW_BUILD_DIR"

echo 
echo ========== enter shadowbuild dir 
echo 
mkdir -p $SHADOW_BUILD_DIR
cd $SHADOW_BUILD_DIR

echo 
echo ========== prepare build 
echo 
$QT_PATH_RUNTIME/bin/qmake $GARLIC_DIR/src/complete.pro "CONFIG+=$CONFIG_DEBUG_RELEASE"

echo 
echo ========== build with $DEV_JOBS dev jobs
echo 
make -j $DEV_JOBS --silent

# check if binary was created 
if [ ! -f bin/garlic-player.app/Contents/MacOS/garlic-player ]; then
	echo Binary was not created!
    exit 1;
fi

echo 
echo ========== deploy
echo

$QT_PATH_RUNTIME/bin/macdeployqt bin/garlic-player.app -appstore-compliant -always-overwrite

echo 
echo ========== correct errors of shitty macdeployqt
echo

cp -R $QT_PATH_RUNTIME/lib/QtAVWidgets.framework bin/garlic-player.app/Contents/Frameworks
rm -rf bin/garlic-player.app/Contents/Frameworks/QtAVWidgets.framework/Versions/Current/Headers
rm -rf bin/garlic-player.app/Contents/Frameworks/QtAVWidgets.framework/Versions/1/Headers
rm -rf bin/garlic-player.app/Contents/Frameworks/QtAVWidgets.framework/Headers

#QtAVWidgets.framework
install_name_tool -id @executable_path/../Frameworks/QtAVWidgets.framework/Versions/1/QtAVWidgets bin/garlic-player.app/Contents/Frameworks/QtAVWidgets.framework/Versions/1/QtAVWidgets
install_name_tool -change /usr/local/opt/ffmpeg/lib/libswresample.3.dylib @executable_path/../Frameworks/libswresample.3.dylib bin/garlic-player.app/Contents/Frameworks/QtAVWidgets.framework/Versions/1/QtAVWidgets
install_name_tool -change /usr/local/opt/ffmpeg/lib/libavresample.4.dylib @executable_path/../Frameworks/libavresample.4.dylib bin/garlic-player.app/Contents/Frameworks/QtAVWidgets.framework/Versions/1/QtAVWidgets
install_name_tool -change /usr/local/opt/ffmpeg/lib/libavdevice.58.dylib @executable_path/../Frameworks/libavdevice.58.dylib bin/garlic-player.app/Contents/Frameworks/QtAVWidgets.framework/Versions/1/QtAVWidgets
install_name_tool -change /usr/local/opt/ffmpeg/lib/libavfilter.7.dylib @executable_path/../Frameworks/libavfilter.7.dylib bin/garlic-player.app/Contents/Frameworks/QtAVWidgets.framework/Versions/1/QtAVWidgets
install_name_tool -change /usr/local/opt/ffmpeg/lib/libavcodec.58.dylib @executable_path/../Frameworks/libavcodec.58.dylib bin/garlic-player.app/Contents/Frameworks/QtAVWidgets.framework/Versions/1/QtAVWidgets
install_name_tool -change /usr/local/opt/ffmpeg/lib/libavformat.58.dylib @executable_path/../Frameworks/libavformat.58.dylib bin/garlic-player.app/Contents/Frameworks/QtAVWidgets.framework/Versions/1/QtAVWidgets
install_name_tool -change /usr/local/opt/ffmpeg/lib/libswscale.5.dylib @executable_path/../Frameworks/libswscale.5.dylib bin/garlic-player.app/Contents/Frameworks/QtAVWidgets.framework/Versions/1/QtAVWidgets
install_name_tool -change /usr/local/opt/ffmpeg/lib/libavutil.56.dylib @executable_path/../Frameworks/libavutil.56.dylib bin/garlic-player.app/Contents/Frameworks/QtAVWidgets.framework/Versions/1/QtAVWidgets

#libavcodec.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libswresample.3.dylib @executable_path/../Frameworks/libswresample.3.dylib bin/garlic-player.app/Contents/Frameworks/libavcodec.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavutil.56.dylib @executable_path/../Frameworks/libavutil.56.dylib bin/garlic-player.app/Contents/Frameworks/libavcodec.58.dylib

#libavdevice.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavfilter.7.dylib @executable_path/../Frameworks/libavfilter.7.dylib bin/garlic-player.app/Contents/Frameworks/libavdevice.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libswscale.5.dylib @executable_path/../Frameworks/libswscale.5.dylib bin/garlic-player.app/Contents/Frameworks/libavdevice.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libpostproc.55.dylib @executable_path/../Frameworks/libpostproc.55.dylib bin/garlic-player.app/Contents/Frameworks/libavdevice.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavformat.58.dylib @executable_path/../Frameworks/libavformat.58.dylib bin/garlic-player.app/Contents/Frameworks/libavdevice.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavcodec.58.dylib @executable_path/../Frameworks/libavcodec.58.dylib bin/garlic-player.app/Contents/Frameworks/libavdevice.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libswresample.3.dylib @executable_path/../Frameworks/libswresample.3.dylib bin/garlic-player.app/Contents/Frameworks/libavdevice.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavresample.4.dylib @executable_path/../Frameworks/libavresample.4.dylib bin/garlic-player.app/Contents/Frameworks/libavdevice.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavutil.56.dylib @executable_path/../Frameworks/libavutil.56.dylib bin/garlic-player.app/Contents/Frameworks/libavdevice.58.dylib

#libavfilter.7.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libswscale.5.dylib @executable_path/../Frameworks/libswscale.5.dylib bin/garlic-player.app/Contents/Frameworks/libavfilter.7.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libpostproc.55.dylib @executable_path/../Frameworks/libpostproc.55.dylib bin/garlic-player.app/Contents/Frameworks/libavfilter.7.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavformat.58.dylib @executable_path/../Frameworks/libavformat.58.dylib bin/garlic-player.app/Contents/Frameworks/libavfilter.7.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavcodec.58.dylib @executable_path/../Frameworks/libavcodec.58.dylib bin/garlic-player.app/Contents/Frameworks/libavfilter.7.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libswresample.3.dylib @executable_path/../Frameworks/libswresample.3.dylib bin/garlic-player.app/Contents/Frameworks/libavfilter.7.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavresample.4.dylib @executable_path/../Frameworks/libavresample.4.dylib bin/garlic-player.app/Contents/Frameworks/libavfilter.7.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavutil.56.dylib @executable_path/../Frameworks/libavutil.56.dylib bin/garlic-player.app/Contents/Frameworks/libavfilter.7.dylib

#libavformat.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavcodec.58.dylib @executable_path/../Frameworks/libavcodec.58.dylib bin/garlic-player.app/Contents/Frameworks/libavformat.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libswresample.3.dylib @executable_path/../Frameworks/libswresample.3.dylib bin/garlic-player.app/Contents/Frameworks/libavformat.58.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavutil.56.dylib @executable_path/../Frameworks/libavutil.56.dylib bin/garlic-player.app/Contents/Frameworks/libavformat.58.dylib

#libavresample.4.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavutil.56.dylib @executable_path/../Frameworks/libavutil.56.dylib bin/garlic-player.app/Contents/Frameworks/libavresample.4.dylib

#libhogweed.6.dylib
install_name_tool -change /usr/local/Cellar/nettle/3.7/lib/libnettle.8.dylib @executable_path/../Frameworks/libnettle.8.dylib bin/garlic-player.app/Contents/Frameworks/libhogweed.6.dylib

#libpostproc.55.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavutil.56.dylib @executable_path/../Frameworks/libavutil.56.dylib bin/garlic-player.app/Contents/Frameworks/libpostproc.55.dylib

#libswscale.5.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavutil.56.dylib @executable_path/../Frameworks/libavutil.56.dylib bin/garlic-player.app/Contents/Frameworks/libswscale.5.dylib

#libswresample.3.dylib
install_name_tool -change /usr/local/Cellar/ffmpeg/4.3.1_7/lib/libavutil.56.dylib @executable_path/../Frameworks/libavutil.56.dylib bin/garlic-player.app/Contents/Frameworks/libswresample.3.dylib

echo 
echo ========== create dmg file
echo

create-dmg --hdiutil-quiet --sandbox-safe garlic-player-$DEPLOY_SUFFIX.dmg bin

exit 0;
