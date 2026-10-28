#include "videoout.h"

VideoOut::VideoOut(QWidget *parent) : QWidget(parent)
{
   MyLabel = new QLabel(this);
   MyLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
   MyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

   QString style_sheet = "background: transparent;";
   setStyleSheet(style_sheet);

}

VideoOut::~VideoOut()
{
    MyLabel->clear();
    delete MyLabel;
}

void VideoOut::placeDecodedImage(const QImage &image)
{
    if (image == QImage())
        return;

    QSize size = MyLabel->size();
    MyLabel->setPixmap(QPixmap::fromImage(image.scaled(size.width(), size.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation),Qt::AutoColor));

}

void VideoOut::clear()
{
    MyLabel->clear();
}

void VideoOut::resizeEvent(QResizeEvent *event)
{
    MyLabel->setGeometry(0, 0, event->size().width(), event->size().height());
}
