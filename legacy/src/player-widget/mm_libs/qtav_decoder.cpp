#ifdef SUPPORT_QTAV
#include "qtav_decoder.h"

QtAVDecoder::QtAVDecoder(QObject *parent) : QObject(parent)
{

#ifdef SUPPORT_RPI
    // ensure that Raspberry uses MMAL encoder
    MediaDecoder.setVideoDecoderPriority(QStringList() << "MMAL" << "FFmpeg");
#endif

    MediaDecoder.audio()->setBufferSamples(512); // prevent stuttering
    connect(&MediaDecoder, SIGNAL(mediaStatusChanged(QtAV::MediaStatus)), this, SLOT(onMediaStatusChanged(QtAV::MediaStatus)));
    connect(&MediaDecoder, SIGNAL(error(QtAV::AVError)), this, SLOT(displayErrorMessage(QtAV::AVError)));
}

QtAVDecoder::~QtAVDecoder()
{
    MediaDecoder.clearVideoRenderers();
    MediaDecoder.audio()->clear();
    MediaDecoder.audio()->close();

}

void QtAVDecoder::setVideoOutput(MediaWidgetWrapper *renderer)
{
    MediaDecoder.setRenderer(renderer->getVideoRenderer());
}

void QtAVDecoder::removeVideoOutput(MediaWidgetWrapper *renderer)
{
    MediaDecoder.removeVideoRenderer(renderer->getVideoRenderer());
}

bool QtAVDecoder::load(QString file_path)
{
    current_media_path = file_path;
    MediaDecoder.setAsyncLoad(false);
    MediaDecoder.setFile(current_media_path);
    return MediaDecoder.load();
}

bool QtAVDecoder::restart()
{
    MediaDecoder.setPosition(0);
}

void QtAVDecoder::setVolume(QString percent)
{
    qreal vol = 0;
    if (percent.endsWith('%'))
        vol = (percent.midRef(0, percent.length()-1).toDouble()) / 100;
    // sometimes result messed up with 0.10000000000000000001, but that should be no problem
    // a more significant problem could be, that e.g. 10% Volume is in QtMultimedia louder than in QtAV.
    MediaDecoder.audio()->setVolume(vol);
}

void QtAVDecoder::unload()
{
    current_media_path = "";
    MediaDecoder.setFile("");
}

void QtAVDecoder::play()
{
    MediaDecoder.play();
}

void QtAVDecoder::stop()
{
    MediaDecoder.stop();
}

void QtAVDecoder::resume()
{
    MediaDecoder.togglePause();
}

void QtAVDecoder::pause()
{
    MediaDecoder.pause();
}

void QtAVDecoder::onMediaStatusChanged(QtAV::MediaStatus status)
{
    switch (status)
    {
        case QtAV::UnknownMediaStatus:
            qWarning(Develop) << "Unknown media status! Media:" << current_media_path;
            break;
        case QtAV::NoMedia:
            qWarning(Develop) << "No Media " << current_media_path;
            break;
        case QtAV::LoadedMedia:
            qDebug() << "Loaded media " << current_media_path;
            break;
        case QtAV::BufferedMedia:
            qDebug() << "Buffered media " << current_media_path;
            break;
        case QtAV::BufferingMedia:
            qDebug() << "Buffering media " << current_media_path;
            break;
        case QtAV::LoadingMedia:
            qDebug() << "Loading media " << current_media_path;
            break;
        case QtAV::StalledMedia:
            qWarning(Develop) << "Stalled media " << current_media_path;
            break;
        case QtAV::EndOfMedia:
            qDebug() << "End of media " << current_media_path;
            emit finished();
            break;
        case QtAV::InvalidMedia:
            qWarning(Develop) << "invalid Media " << current_media_path;
            break;
    }
    return;
}


void QtAVDecoder::displayErrorMessage(QtAV::AVError e)
{
    qWarning(Develop) << e.string();
    return;
}

#endif
