#ifndef MEDIAVIEWWRAPPER_H
#define MEDIAVIEWWRAPPER_H

#include <QWidget>
#include <QLabel>

#if defined SUPPORT_QTAV
    #include "mm_libs/qtav_widget.h"
#elif defined SUPPORT_QTAVPLAYER
    #include "mm_libs/qtavplayer_widget.h"
#elif defined SUPPORT_QFFPLAY
    #include "mm_libs/qffpl_widget.h"
#elif defined SUPPORT_LIBVLC
    #include "mm_libs/vlc_widget.h"
#else
    #include "mm_libs/qtmm_widget.h"
#endif

class MediaWidgetWrapper : public QWidget
{
    Q_OBJECT
public:
    explicit MediaWidgetWrapper(QWidget *parent = Q_NULLPTR);
    ~MediaWidgetWrapper();
#if defined SUPPORT_QTAV
    QtAV::VideoRenderer    *getVideoRenderer(){return VideoWidget.getVideoRenderer();}
#elif defined SUPPORT_QTAVPLAYER
    VideoRenderer         *getVideoRenderer(){return VideoWidget.getVideoRenderer();}
#elif defined SUPPORT_QFFPLAY
    QWidget                *getVideoRenderer(){return VideoWidget.getVideoRenderer();}
#elif defined SUPPORT_LIBVLC
    QWidget                *getVideoRenderer(){return VideoWidget.getVideoRenderer();}
#else
    QVideoWidget         *getVideoRenderer(){return VideoWidget.getVideoRenderer();}
#endif
    QWidget               *getVideoWidget();
    void                  ignoreAspectRatio();
    void                  keepAspectRatio();
    void                  keepAspectRatioByExpanding();
signals:
protected:
#if defined SUPPORT_QTAV
    QtAVWidget VideoWidget;  // a deleteLater leads to a crash on playlistchange!
#elif defined SUPPORT_QTAVPLAYER
    QtAVPlayerWidget     VideoWidget;
#elif defined SUPPORT_QFFPLAY
    QffplWidget         VideoWidget;
#elif defined SUPPORT_LIBVLC
    VlcWidget         VideoWidget;
#else
    QtMMWidget         VideoWidget;
#endif
};

#endif // MEDIAVIEWWRAPPER_H
