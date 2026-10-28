#ifdef SUPPORT_QFFPLAY
#include "qffpl_decoder.h"

QffplDecoder::QffplDecoder(QObject *parent)
    : QObject{parent}
{
//    MediaDecoder.data()->audio()->setBufferSamples(512); // prevent stuttering

    connect(&MediaDecoder, SIGNAL(mediaStatusChanged(QtFFmpegPlayer::MediaStatus)), this, SLOT(onMediaStatusChanged(QtFFmpegPlayer::MediaStatus)));
    //connect(MediaDecoder.data(), SIGNAL(error(QtAV::AVError)), this, SLOT(displayErrorMessage(QtAV::AVError)));

}

QffplDecoder::~QffplDecoder()
{
    disconnect(&MediaDecoder, SIGNAL(mediaStatusChanged(QtFFmpegPlayer::MediaStatus)), this, SLOT(onMediaStatusChanged(QtFFmpegPlayer::MediaStatus)));
}

void QffplDecoder::setVideoOutput(MediaWidgetWrapper *renderer)
{
    MediaDecoder.setRenderer((VideoOut *)renderer->getVideoRenderer());
}

void QffplDecoder::removeVideoOutput(MediaWidgetWrapper *renderer)
{
    Q_UNUSED(renderer);
    MediaDecoder.setRenderer(Q_NULLPTR);
}

bool QffplDecoder::load(QString file_path)
{
    return MediaDecoder.load(file_path);
}

void QffplDecoder::restart()
{
    MediaDecoder.seek(0);
}

void QffplDecoder::setVolume(QString percent)
{

}

void QffplDecoder::unload()
{

}

void QffplDecoder::play()
{
    MediaDecoder.play();
}

void QffplDecoder::stop()
{
    MediaDecoder.stop();
}

void QffplDecoder::resume()
{
    MediaDecoder.resume();
}

void QffplDecoder::pause()
{
    MediaDecoder.pause();
}

void QffplDecoder::onMediaStatusChanged(QtFFmpegPlayer::MediaStatus status)
{
    switch (status)
    {
        case QtFFmpegPlayer::UnknownMediaStatus:
            qWarning(Develop) << "Unknown media status! Media:" << current_media_path;
            break;
        case QtFFmpegPlayer::NoMedia:
            qWarning(Develop) << "No Media " << current_media_path;
            break;
        case QtFFmpegPlayer::LoadedMedia:
            qDebug() << "Loaded media " << current_media_path;
            break;
        case QtFFmpegPlayer::BufferedMedia:
            qDebug() << "Buffered media " << current_media_path;
            break;
        case QtFFmpegPlayer::BufferingMedia:
            qDebug() << "Buffering media " << current_media_path;
            break;
        case QtFFmpegPlayer::LoadingMedia:
            qDebug() << "Loading media " << current_media_path;
            break;
        case QtFFmpegPlayer::StalledMedia:
            qWarning(Develop) << "Stalled media " << current_media_path;
            break;
        case QtFFmpegPlayer::EndOfMedia:
            qDebug() << "End of media " << current_media_path;
            emit finished();
            break;
        case QtFFmpegPlayer::InvalidMedia:
            qWarning(Develop) << "invalid Media " << current_media_path;
            break;
    }
    return;
}

#endif
