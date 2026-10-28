#ifdef SUPPORT_LIBVLC

#ifndef VLC_WIDGET_H
#define VLC_WIDGET_H

#include <QWidget>

class VlcWidget : public QWidget
{
        Q_OBJECT
    public:
        explicit VlcWidget(QWidget *parent = nullptr);
        QWidget              *getVideoWidget();
        QWidget              *getVideoRenderer();

        void                  ignoreAspectRatio();
        void                  keepAspectRatio();
        void                  keepAspectRatioByExpanding();

    signals:
    protected:
    public slots:
};
#endif

#endif // VLC_WIDGET_H
