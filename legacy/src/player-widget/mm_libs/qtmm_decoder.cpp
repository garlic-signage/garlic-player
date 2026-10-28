#ifdef SUPPORT_QTMM
#include "qtmm_decoder.h"

QtMMDecoder::QtMMDecoder(QObject *parent) : QObject(parent)
{
    connect(&MediaDecoder, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)), this, SLOT(onMediaStatusChanged(QMediaPlayer::MediaStatus)));
    connect(&MediaDecoder, SIGNAL(error(QMediaPlayer::Error)), this, SLOT(displayErrorMessage()));
}

void QtMMDecoder::setVideoOutput(MediaWidgetWrapper *renderer)
{
    MediaDecoder.setVideoOutput(renderer->getVideoRenderer());
}

void QtMMDecoder::removeVideoOutput(MediaWidgetWrapper *renderer)
{
    Q_UNUSED(renderer);
    MediaDecoder.setVideoOutput((QVideoWidget *) Q_NULLPTR);
}

bool QtMMDecoder::load(QString file_path)
{
    if (file_path.mid(0, 3) == "///")
        current_media_path = file_path.mid(2, file_path.length()-2);
    else
        current_media_path = file_path;


    MediaDecoder.setMedia(QMediaContent(QUrl::fromLocalFile(current_media_path)));
    bool ret = (MediaDecoder.mediaStatus() != QMediaPlayer::NoMedia && MediaDecoder.mediaStatus() != QMediaPlayer::InvalidMedia);
    return ret;
}

void QtMMDecoder::restart()
{
    MediaDecoder.setPosition(0);
}

void QtMMDecoder::setVolume(QString percent)
{
    int vol = 0;
    if (percent.endsWith('%'))
        vol = percent.midRef(0, percent.length()-1).toInt();
    MediaDecoder.setVolume(vol);
}

void QtMMDecoder::unload()
{
    MediaDecoder.setMedia(QMediaContent());
}

void QtMMDecoder::play()
{
    MediaDecoder.play();
}

void QtMMDecoder::stop()
{
    MediaDecoder.stop();
}

void QtMMDecoder::resume()
{
    play();
}

void QtMMDecoder::pause()
{
    MediaDecoder.pause();
}


void QtMMDecoder::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    switch (status)
    {
        case QMediaPlayer::UnknownMediaStatus:
            qWarning(Develop) << "Unknown media status! Media:" << current_media_path;
            break;
        case QMediaPlayer::NoMedia:
            qWarning(Develop) << "No Media " << current_media_path;
            break;
        case QMediaPlayer::LoadedMedia:
            qDebug() << "Loaded media " << current_media_path;
            break;
        case QMediaPlayer::BufferedMedia:
            qDebug() << "Buffered media " << current_media_path;
            break;
        case QMediaPlayer::BufferingMedia:
            qDebug() << "Buffering media " << current_media_path;
            break;
        case QMediaPlayer::LoadingMedia:
            qDebug() << "Loading media " << current_media_path;
            break;
        case QMediaPlayer::StalledMedia:
            qWarning(Develop) << "Stalled media " << current_media_path;
            break;
        case QMediaPlayer::EndOfMedia:
            qDebug() << "End of media " << current_media_path;
            emit finished();
            break;
        case QMediaPlayer::InvalidMedia:
            qWarning(Develop) << "invalid Media " << current_media_path;
            break;
    }
}

void QtMMDecoder::displayErrorMessage()
{
    qWarning(Develop) << MediaDecoder.errorString();
}


#endif
