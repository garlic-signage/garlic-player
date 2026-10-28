#ifndef VIDEOOUT_H
#define VIDEOOUT_H

#include <QLabel>
#include <QResizeEvent>

class VideoOut : public QWidget
{
        Q_OBJECT
    public:
        explicit VideoOut(QWidget *parent = nullptr);
        ~VideoOut();
        void placeDecodedImage(const QImage &image);
        void clear();
    protected:
        void resizeEvent(QResizeEvent *event);
    private:
        QImage  paint_image;
        QLabel *MyLabel = Q_NULLPTR;

};

#endif // VIDEOOUT_H

