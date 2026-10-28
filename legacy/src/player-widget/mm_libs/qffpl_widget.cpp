#ifdef SUPPORT_QFFPLAY

#include "qffpl_widget.h"

QffplWidget::QffplWidget()
{

}

QWidget *QffplWidget::getVideoWidget()
{
    return &VideoWidget;
}

QWidget *QffplWidget::getVideoRenderer()
{
    return &VideoWidget;
}

void QffplWidget::ignoreAspectRatio()
{

}

void QffplWidget::keepAspectRatio()
{

}

void QffplWidget::keepAspectRatioByExpanding()
{

}

#endif
