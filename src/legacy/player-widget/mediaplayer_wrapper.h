#ifndef MEDIADECODERWRAPPER_H
#define MEDIADECODERWRAPPER_H

#if defined SUPPORT_QTAV
    #include "mm_libs/qtav_decoder.h"
#elif defined SUPPORT_QTAVPLAYER
    #include "mm_libs/qtavplayer_decoder.h"
#elif defined SUPPORT_QFFPLAY
    #include "mm_libs/qffpl_decoder.h"
#elif defined SUPPORT_LIBVLC
    #include "mm_libs/vlc_decoder.h"
#else
    #include "mm_libs/qtmm_decoder.h"
#endif

#include <QObject>
#include "mediawidget_wrapper.h"

class MediaPlayerWrapper : public QObject
{
    Q_OBJECT
public:
    explicit MediaPlayerWrapper(QObject *parent = Q_NULLPTR);
    ~MediaPlayerWrapper();
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
    void                ended();

protected:
    QString             current_media_path;
#if defined SUPPORT_QTAV
   QtAVDecoder  MediaDecoder;
#elif defined SUPPORT_QTAVPLAYER
   QtAVPLayerDecoder   MediaDecoder;
#elif defined SUPPORT_QFFPLAY
   QffplDecoder MediaDecoder;
#elif defined SUPPORT_LIBVLC
    VlcDecoder  MediaDecoder;
#else
    QtMMDecoder  MediaDecoder;
#endif

protected slots:


};

#endif // MEDIADECODERWRAPPER_H
