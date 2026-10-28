#ifdef SUPPORT_LIBVLC

#ifndef VLC_DECODER_H
#define VLC_DECODER_H

#include <QObject>
#include <QObject>
#include <QTimer>
#if defined(Q_OS_MAC)
    #include <QMacCocoaViewContainer>
#endif

#ifdef _MSC_VER
    typedef long int ssize_t;
#endif

#include <vlc/vlc.h>
#include "mediawidget_wrapper.h"
#include "tools/logging_categories.h"

class VlcDecoder: public QObject
{
    Q_OBJECT
public:
    VlcDecoder(QObject *parent = nullptr);
   ~VlcDecoder();
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
    QString                  current_media_path;
    libvlc_instance_t       *vlcInstance = NULL;
    libvlc_media_player_t   *vlcPlayer = NULL;
    libvlc_media_t          *vlcMedia = NULL;
    MediaWidgetWrapper      *Renderer;
    QScopedPointer<QTimer>   PositionTimer;
protected slots:
    void updatePosition();
    void displayErrorMessage();
};
#endif

#endif // VLC_DECODER_H
