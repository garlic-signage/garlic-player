#ifdef SUPPORT_QTMM
#include "qtmm_widget.h"

QtMMWidget::QtMMWidget(QWidget *parent) :QWidget(parent)
{
}

QtMMWidget::~QtMMWidget()
{
}

QWidget  *QtMMWidget::getVideoWidget()
{
    return &VideoWidget;
}

QVideoWidget *QtMMWidget::getVideoRenderer()
{
    return &VideoWidget;
}

void QtMMWidget::ignoreAspectRatio()
{
    VideoWidget.setAspectRatioMode(Qt::IgnoreAspectRatio);
}

void QtMMWidget::keepAspectRatio()
{
    VideoWidget.setAspectRatioMode(Qt::KeepAspectRatio);
}

void QtMMWidget::keepAspectRatioByExpanding()
{
    VideoWidget.setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
}

#endif
