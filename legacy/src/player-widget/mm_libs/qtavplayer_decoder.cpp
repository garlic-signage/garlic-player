#ifdef SUPPORT_QTAVPLAYER
#include "qtavplayer_decoder.h"

QtAVPLayerDecoder::QtAVPLayerDecoder(QObject *parent) : QObject(parent)
{
    connect(&MediaDecoder, SIGNAL(mediaStatusChanged(QAVPlayer::MediaStatus)), this, SLOT(onMediaStatusChanged(QAVPlayer::MediaStatus)));
}

void QtAVPLayerDecoder::setVideoOutput(MediaWidgetWrapper *renderer)
{
    MyVideoRenderer = renderer->getVideoRenderer();
}

void QtAVPLayerDecoder::removeVideoOutput(MediaWidgetWrapper *renderer)
{

}

bool QtAVPLayerDecoder::load(QString file_path)
{
    MediaDecoder.setSource(file_path);

    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    QList<QAudioDeviceInfo> list_devices = info.availableDevices(QAudio::AudioOutput);
    if (list_devices.length() > 0)
        QObject::connect(&MediaDecoder, SIGNAL(audioFrame(const QAVAudioFrame)), this, SLOT(outputAudioFrame(const QAVAudioFrame)));

    QObject::connect(&MediaDecoder, SIGNAL(videoFrame(const QAVVideoFrame)), this, SLOT(outputVideoFrame(const QAVVideoFrame)));

    return true;
}

void QtAVPLayerDecoder::restart()
{
    MediaDecoder.seek(0);

}

void QtAVPLayerDecoder::setVolume(QString percent)
{
    qreal vol = 0;
    if (percent.endsWith('%'))
        vol = (percent.midRef(0, percent.length()-1).toDouble()) / 100;
    // sometimes result messed up with 0.10000000000000000001, but that should be no problem
    // a more significant problem could be, that e.g. 10% Volume is in QtMultimedia louder than in QtAV.
    MyAudioRenderer.setVolume(vol);
}

void QtAVPLayerDecoder::unload()
{

}

void QtAVPLayerDecoder::play()
{
    MediaDecoder.play();

}

void QtAVPLayerDecoder::stop()
{
    MediaDecoder.stop();
}

void QtAVPLayerDecoder::resume()
{
    MediaDecoder.play();
}

void QtAVPLayerDecoder::pause()
{
    MediaDecoder.pause();
}

void QtAVPLayerDecoder::onMediaStatusChanged(QAVPlayer::MediaStatus status)
{
    switch (status)
    {
        case QAVPlayer::NoMedia:
            qWarning(Develop) << "No Media " << current_media_path;
            break;
        case QAVPlayer::LoadedMedia:
            qDebug() << "Loaded media " << current_media_path;
            break;
        case QAVPlayer::EndOfMedia:
            qDebug() << "End of media " << current_media_path;
            emit finished();
            break;
        case QAVPlayer::InvalidMedia:
            qWarning(Develop) << "invalid Media " << current_media_path;
            break;
    }
}

void QtAVPLayerDecoder::outputAudioFrame(const QAVAudioFrame &frame)
{
    MyAudioRenderer.play(frame);
}

void QtAVPLayerDecoder::outputVideoFrame(const QAVVideoFrame &frame)
{
    if (MyVideoRenderer->m_surface == nullptr)
        return;

    QVideoFrame videoFrame = frame.convertTo(AV_PIX_FMT_RGB32);
    if (!MyVideoRenderer->m_surface->isActive() || MyVideoRenderer->m_surface->surfaceFormat().frameSize() != videoFrame.size()) {
        QVideoSurfaceFormat f(videoFrame.size(), videoFrame.pixelFormat(), videoFrame.handleType());
        MyVideoRenderer->m_surface->start(f);
    }

    if (MyVideoRenderer->m_surface->isActive())
        MyVideoRenderer->m_surface->present(videoFrame);

}
#endif
