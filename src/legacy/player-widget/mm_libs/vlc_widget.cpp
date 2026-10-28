#ifdef SUPPORT_LIBVLC
#include "vlc_widget.h"

VlcWidget::VlcWidget(QWidget *parent) : QWidget(parent)
{
}

QWidget *VlcWidget::getVideoWidget()
{
    return this;
}

QWidget *VlcWidget::getVideoRenderer()
{
    return getVideoWidget();
}

void VlcWidget::ignoreAspectRatio()
{

}

void VlcWidget::keepAspectRatio()
{

}

void VlcWidget::keepAspectRatioByExpanding()
{

}
#endif
