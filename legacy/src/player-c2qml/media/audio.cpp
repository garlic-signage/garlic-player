#include "audio.h"
#include "smilparser/media/audio.h"

Audio::Audio(QQmlComponent *mc, QString r_id, Launcher *lc, MainConfiguration *config,  QObject *parent) : PlayerBaseMedia(mc, r_id, lc, config, parent)
{
    media_component = mc;
    KillTimer = new QTimer(this);
    connect(KillTimer, SIGNAL(timeout()), this, SLOT(checkForKill()));
    KillTimer->setTimerType(Qt::VeryCoarseTimer);

    // Audio QML cannot be created
    // Bug was reported at 26 Nov 2017 https://bugreports.qt.io/browse/QTBUG-64763
    // and updated on 2022-07-16

    qml = "import QtQuick 2.12\n                             \
           import QtMultimedia 5.12\n                        \
                   Video {                                   \
                        id: "+getRegionId()+"_audio;         \
                        autoLoad: true;                      \
                        autoPlay: false;                      \
                        signal qmlEndOfFile()\n \
                        onStopped: { if (status == MediaPlayer.EndOfMedia) {qmlEndOfFile()}\n} \
                     }\n";

}

Audio::~Audio()
{
    KillTimer->stop();
    delete KillTimer;
}

void Audio::loadMedia(BaseMedia *media, Region *reg)
{
    SmilMedia = media;
    region    = reg;
    loadInternal();
}

void Audio::loadInternal()
{
    media_item.reset(createMediaItem(media_component, qml));
    if (!media_item.isNull())
        connect(media_item.data(), SIGNAL(qmlEndOfFile()), this, SLOT(doStopped()));

    if (load(media_item.data()))
    {
        // to set Volume we need to cast
        MediaParser::TAudio *MyAudio = qobject_cast<MediaParser::TAudio *> (SmilMedia);
        float vol = determineVolume(MyAudio->getSoundLevel());
        media_item.data()->setProperty("volume", vol);
    }
}

void Audio::restart()
{
    if (!SmilMedia->getLogContentId().isEmpty())
        qInfo(PlayLog).noquote() << createPlayLogXml();

    // seek did not work
    // set seekable in QML as true crashes
    QMetaObject::invokeMethod(media_item.data(), "stop");
    QMetaObject::invokeMethod(media_item.data(), "play");

    if (SmilMedia->getLogContentId() != "")
        setStartTime();
}

void Audio::play()
{
    // todo add support for pauseDisplay
   QMetaObject::invokeMethod(media_item.data(), "play");
   KillTimer->start(KILLTIMER_INTERVALL);
   if (SmilMedia->getLogContentId() != "")
       setStartTime();

   media_item.data()->setVisible(true);
}

void Audio::stop()
{
 //
    if (!SmilMedia->getLogContentId().isEmpty())
        qInfo(PlayLog).noquote() << createPlayLogXml();

    KillTimer->stop();
    QMetaObject::invokeMethod(media_item.data(), "stop");
}

void Audio::resume()
{
    loadInternal();
    media_item.data()->setParentItem(parent_item);
    play(); // KillTimer starts here

    // look at pause explanation
//    QMetaObject::invokeMethod(media_item.data(), "play");

}

void Audio::pause()
{
    // numerous Android players cannot handle two videos at the same time. Especially Philips PPDS-screens do this.
    // This result in crashes or undefined reactions. Philips OS for example kills the current video so hard,
    // that the system has no chance to send an EndOfFile
    // This breaks the playlist. It will stop to continue and display the background screen
    // To prevent this we had to stop and deinitialize the video component in case of a pause and reload it when resume
    // Furthermore, there is a kill timer checking every 2s the position and determine if the video/audio is progresses
    // if not it send a stoppped signal, so that the playlist can continue.

    // QMetaObject::invokeMethod(media_item.data(), "pause");
    // todo add support for pauseDisplay
    // video_item.data()->setVisible(false);

    KillTimer->stop();
    if (!SmilMedia->getLogContentId().isEmpty())
        qInfo(PlayLog).noquote() << createPlayLogXml();
    QMetaObject::invokeMethod(media_item.data(), "stop");
    media_item.reset();
}

void Audio::setParentItem(QQuickItem *parent)
{
    parent_item = parent;
    media_item.data()->setParentItem(parent);
}

void Audio::changeSize(int w, int h)
{
    Q_UNUSED(w);Q_UNUSED(h)
}

qreal Audio::determineVolume(QString percent)
{
    qreal vol = 0;
    if (percent.endsWith('%'))
        vol = percent.midRef(0, percent.length()-1).toFloat();

    return vol / (float) 100;
}

int Audio::getCurrentPosition()
{
    return media_item.data()->property("position").toInt();
}

void Audio::doStopped()
{
    KillTimer->stop();
    if (media_item.data()->property("error").toInt() != 0)
    {
        QStringList list;
         list  << "resourceURI" << SmilMedia->getSrc()
              << "error"<< media_item.data()->property("error").toString()
              << "errorMessage" << media_item.data()->property("errorString").toString();
        qCritical(MediaPlayer) << MyLogger.createEventLogMetaData("MEDIA_PLAYBACK_ERROR", list);
    }
    SmilMedia->finishIntrinsicDuration();
}

void Audio::checkForKill()
{
   int current = getCurrentPosition();
   qDebug() << "current pos: " + QString::number(current) + " last pos: " + QString::number(last_position);
   if (current == last_position)
   {
       qDebug() << "finish by Killtimer";
       doStopped();
   }

    last_position = current;
}

