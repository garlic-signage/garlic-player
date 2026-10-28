#ifndef FFMPEGDECODER_H
#define FFMPEGDECODER_H

#include <QObject>
#include <QLabel>
#include <QCoreApplication>
#include <QTime>
#include "videoout.h"
#include "ffmpeg.h"


namespace QtFFmpegPlayer
{
    class Decoder : public QObject
    {
        enum status {stopped, paused, decoding};
        Q_OBJECT
    public:
        explicit Decoder(FFmpeg *ff, QObject *parent = nullptr);
        ~Decoder();
        void stop();
        void pause();
        void seek(int position);
        void setRenderer(VideoOut *r);
       // bool event(QEvent* ev)  override;

    public slots:
        void decode();
     private:
        FFmpeg   *MyFFmpeg;
        VideoOut *MyVideoOut;
        QPixmap   loaded_image;
        status    decoder_status = stopped;
        QImage    convertFrameToImage(AVFrame *pFrame);

        int decodeVideoPacket(AVPacket *pPacket);
        int decodeAudioPacket(AVPacket *pPacket);
        void fillRenderer(unsigned char *buf, int wrap, int xsize, int ysize, char *filename);
        void calculateFrameRate(AVFrame *frame);
        void delay(int msec);
        int frame_rate = 25;
    signals:
        void finished();
        void error(QString err);
};
}
#endif // FFMPEGDECODER_H
