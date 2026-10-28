#ifdef SUPPORT_QTAV

#include "qtav_widget.h"
QtAVWidget::QtAVWidget(QWidget *parent) :QWidget(parent)
{
    VideoWidget = new QtAV::VideoOutput(this);
}

QtAVWidget::~QtAVWidget()
{
    delete VideoWidget;
}

QWidget *QtAVWidget::getVideoWidget()
{
    return VideoWidget->widget();
}

QtAV::VideoRenderer *QtAVWidget::getVideoRenderer()
{
    return VideoWidget;
}

void QtAVWidget::ignoreAspectRatio()
{
    VideoWidget->setOutAspectRatioMode(QtAV::VideoRenderer::RendererAspectRatio);
}

void QtAVWidget::keepAspectRatio()
{
    VideoWidget->setOutAspectRatioMode(QtAV::VideoRenderer::VideoAspectRatio);
}

void QtAVWidget::keepAspectRatioByExpanding()
{
    VideoWidget->setOutAspectRatioMode(QtAV::VideoRenderer::VideoAspectRatio);
}
#endif
