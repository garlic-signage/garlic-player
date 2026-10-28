#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <QObject>
#include <QThread>
#include "decoder.h"


namespace QtFFmpegPlayer
{
    enum MediaStatus
    {
        UnknownMediaStatus,
        NoMedia,
        LoadingMedia, // when source is set
        LoadedMedia, // if auto load and source is set. player is stopped state
        StalledMedia, // insufficient buffering or other interruptions (timeout, user interrupt)
        BufferingMedia, // NOT IMPLEMENTED
        BufferedMedia, // when playing //NOT IMPLEMENTED
        EndOfMedia, // Playback has reached the end of the current media. The player is in the StoppedState.
        InvalidMedia // what if loop > 0 or stopPosition() is not mediaStopPosition()?
    };

    class MediaPlayer : public QObject
    {
            Q_OBJECT
        public:

            explicit MediaPlayer(QObject *parent = nullptr);
            ~MediaPlayer();
            void setRenderer(VideoOut *r);
            bool load(QString file_path);
            void play();
            void stop();
            void pause();
            void resume();
            void seek(quint64 position);

        private:
            QString        error_string;
            MediaStatus    MyMediaStatus = NoMedia;
            QtFFmpegPlayer::Decoder *MyDecoder      = Q_NULLPTR;
            QtFFmpegPlayer::FFmpeg  *MyFFmpeg       = Q_NULLPTR;
            QThread                 *MyDecodeThread = Q_NULLPTR;
            VideoOut                *MyRenderer = Q_NULLPTR;
        private slots:
            void setErrorText(QString error);
            void doFinished();
        signals:
            void mediaStatusChanged(QtFFmpegPlayer::MediaStatus status);
     };
}

#endif // MEDIAPLAYER_H
