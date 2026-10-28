#ifdef SUPPORT_QTAVPLAYER
#include "qtavplayer_widget.h"

QtAVPlayerWidget::QtAVPlayerWidget(QWidget *parent) : QWidget(parent)
{
   // MyVideoWidget.show();
    MyMediaObject = new MediaObject(&vr, this);
    MyVideoWidget.setMediaObject(MyMediaObject);
}

QtAVPlayerWidget::~QtAVPlayerWidget()
{
    MyVideoWidget.setMediaObject(Q_NULLPTR);
    delete MyMediaObject;
}

QWidget *QtAVPlayerWidget::getVideoWidget()
{
    return &MyVideoWidget;
}

VideoRenderer *QtAVPlayerWidget::getVideoRenderer()
{
    return &vr;
}

void QtAVPlayerWidget::ignoreAspectRatio()
{

}

void QtAVPlayerWidget::keepAspectRatio()
{

}

void QtAVPlayerWidget::keepAspectRatioByExpanding()
{

}
#endif
