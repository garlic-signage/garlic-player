#include "video.h"
#include "smilparser/media/video.h"

Video::Video(QQmlComponent *mc, QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent) : Audio(mc, r_id, lc, config, parent)
{
    // create signal as shitty QML has no signal for finished (EndOfFile)
    qml = "import QtQuick 2.12\n                             \
           import QtMultimedia 5.12\n                        \
                    Video {                                  \
                        id: "+getRegionId()+"_video;         \
                        orientation: 0;                      \
                        autoLoad: true;                      \
                        autoPlay: false;                      \
                        anchors.fill: parent; \
                       property string smil_fit: \"nativeResolution\";\
                        property int w: 0; \
                        property int h: 0; \
                        signal qmlEndOfFile()\n               \
                        onStopped: { if (status == MediaPlayer.EndOfMedia) {qmlEndOfFile()}\n} \
                        onPlaying: { \n \
                            switch (smil_fit) {\n \
                                 case \"fill\": fill(); break;\
                                 case \"meet\": meet(); break;\
                                 case \"meetbest\": meetbest(); break;\
                                 case \"slice\": slice(); break;\
                                 default: nativeResolution(); \
                            }\n \
                        }\n \
                        function fill() {\n \
                            anchors.fill = parent; \
                            anchors.centerIn = undefined; \
                            fillMode = VideoOutput.Stretch; \
                        }\n \
                        function meet() {\n \
                            anchors.fill = parent; \
                            anchors.centerIn = undefined; \
                            fillMode = VideoOutput.PreserveAspectFit; \
                        }\n \
                        function slice() {\n \
                            anchors.fill = parent; \
                            anchors.centerIn = undefined; \
                            fillMode = VideoOutput.PreserveAspectCrop; \
                        }\n \
                        function nativeResolution() {\n \
                            anchors.fill = undefined; \
                            anchors.centerIn = parent; \
                            width  = metaData.resolution.width; \
                            height = metaData.resolution.height; \
                        }\n \
                        function meetbest() {\n \
                           if (metaData.resolution.width >= w || metaData.resolution.height >= h) { \
                                meet(); \
                            }\n \
                            else {\n \
                                nativeResolution(); \
                             }\n \
                        }\n \
                  }\n";

}

Video::~Video()
{
}

void Video::loadInternal()
{
    if (load(media_item.data()))
    {
        // to set Volume we need to cast
        MediaParser::TVideo *MyVideo = qobject_cast<MediaParser::TVideo *> (SmilMedia);
        float vol = determineVolume(MyVideo->getSoundLevel());
        media_item.data()->setProperty("volume", vol);
    }
}

void Video::changeSize(int w, int h)
{
    QString smil_fit = SmilMedia->getFit().toLower();
    media_item.data()->setProperty("w", w);
    media_item.data()->setProperty("h", h);
    media_item.data()->setProperty("smil_fit", smil_fit);

    // we requiere this here for two reasons:
    // 1. After an windows resize we need to invoke the correct funktion especially for meetbest
    // 2. The onPlayed trigger could start without correct video resolutions

    if (smil_fit == "fill")
    {
        QMetaObject::invokeMethod(media_item.data(), "fill");
    }
    else if (smil_fit == "meet")
    {
        QMetaObject::invokeMethod(media_item.data(), "meet");
    }
    else if (smil_fit == "meetbest")
    {
       QMetaObject::invokeMethod(media_item.data(), "meetbest");
    }
    else if (smil_fit == "slice")
    {
        QMetaObject::invokeMethod(media_item.data(), "slice");
    }
    else
    {
        QMetaObject::invokeMethod(media_item.data(), "nativeResolution");
    }

}
