#ifndef QTAVPLAYERDECODER_H
#define QTAVPLAYERDECODER_H


#include <QObject>
#include <QAudioDeviceInfo>
#include <QList>
#include <QtAVPlayer/qavplayer.h>
#include <QtAVPlayer/qavvideoframe.h>
#include <QtAVPlayer/qavaudiooutput.h>
#include "tools/logging_categories.h"

#include "mediawidget_wrapper.h"

class QtAVPLayerDecoder : public QObject
{
        Q_OBJECT
    public:
        explicit QtAVPLayerDecoder(QObject *parent = nullptr);
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
        QAVPlayer           MediaDecoder;
        QAVAudioOutput      MyAudioRenderer;
        VideoRenderer      *MyVideoRenderer;

    protected slots:
        void onMediaStatusChanged(QAVPlayer::MediaStatus status);
        void outputAudioFrame(const QAVAudioFrame &frame);
        void outputVideoFrame(const QAVVideoFrame &frame);
 //       void displayErrorMessage(QtAV::AVError e);
};

#endif // QTAVPLAYERDECODER_H
