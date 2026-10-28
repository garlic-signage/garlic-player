#ifdef SUPPORT_QTMM
#ifndef QTMM_DECODER_H
#define QTMM_DECODER_H

#include <QObject>
#include <QMediaPlayer>
#include "mediawidget_wrapper.h"
#include "tools/logging_categories.h"

class QtMMDecoder : public QObject
{
    Q_OBJECT
public:
    explicit QtMMDecoder(QObject *parent = nullptr);
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
    QMediaPlayer        MediaDecoder;

protected slots:
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void displayErrorMessage();
};
#endif

#endif // QTMM_DECODER_H
