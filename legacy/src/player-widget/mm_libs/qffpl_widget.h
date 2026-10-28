#ifdef SUPPORT_QFFPLAY

#ifndef QFFPLWIDGET_H
#define QFFPLWIDGET_H

#include <QWidget>
#include "qffplaylib/videoout.h"

class QffplWidget : public QWidget
{
    public:
        QffplWidget();

        QWidget              *getVideoWidget();
        QWidget              *getVideoRenderer();

        void                  ignoreAspectRatio();
        void                  keepAspectRatio();
        void                  keepAspectRatioByExpanding();
    protected:
        VideoOut              VideoWidget;
};

#endif // QFFPLWIDGET_H

#endif
