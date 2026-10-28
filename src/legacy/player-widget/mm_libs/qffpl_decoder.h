#ifdef SUPPORT_QFFPLAY

#ifndef QFFPLDECODER_H
#define QFFPLDECODER_H

#include <QObject>
#include "qffplaylib/mediaplayer.h"
#include "mediawidget_wrapper.h"
#include "tools/logging_categories.h"

class QffplDecoder : public QObject
{
        Q_OBJECT
    public:
        explicit QffplDecoder(QObject *parent = nullptr);
        ~QffplDecoder();
        void                setVideoOutput(MediaWidgetWrapper *renderer);
        void                removeVideoOutput(MediaWidgetWrapper *renderer);
        bool                load(QString file_path);
        void                restart();
        void                setVolume(QString percent);
        void                unload();

    signals:
        void                finished();
    public slots:
        void                play();
        void                stop();
        void                resume();
        void                pause();

    protected:
        QString             current_media_path;
        QtFFmpegPlayer::MediaPlayer  MediaDecoder;

    protected slots:
        void onMediaStatusChanged(QtFFmpegPlayer::MediaStatus status);
       //x void displayErrorMessage(QtFFmpegPlayer::AVError e);
    signals:

};

#endif // QFFPLDECODER_H

#endif
