#include "mediaplayer_wrapper.h"

MediaPlayerWrapper::MediaPlayerWrapper(QObject *parent) : QObject(parent)
{
    connect(&MediaDecoder, SIGNAL(finished()), this, SLOT(ended()));
}

MediaPlayerWrapper::~MediaPlayerWrapper()
{
}

void MediaPlayerWrapper::setVideoOutput(MediaWidgetWrapper *Renderer)
{
   MediaDecoder.setVideoOutput(Renderer);
}

void MediaPlayerWrapper::removeVideoOutput(MediaWidgetWrapper *renderer)
{
   MediaDecoder.removeVideoOutput(renderer);
}

bool MediaPlayerWrapper::load(QString file_path)
{
    return MediaDecoder.load(file_path);
}

void MediaPlayerWrapper::restart()
{
    MediaDecoder.restart();
}

void MediaPlayerWrapper::setVolume(QString percent)
{
    MediaDecoder.setVolume(percent);
}

void MediaPlayerWrapper::unload()
{
    MediaDecoder.unload();
}

void MediaPlayerWrapper::play()
{
    MediaDecoder.play();
}

void MediaPlayerWrapper::stop()
{
    MediaDecoder.stop();
}

void MediaPlayerWrapper::resume()
{
    MediaDecoder.resume();
}

void MediaPlayerWrapper::pause()
{
    MediaDecoder.pause();
}


void MediaPlayerWrapper::ended()
{
    emit finished();
}
