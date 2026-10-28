#ifndef FFMPEG_H
#define FFMPEG_H

#include <QObject>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
}

namespace QtFFmpegPlayer
{
    class FFmpeg : public QObject
    {
        Q_OBJECT
    public:
        explicit FFmpeg(QObject *parent = nullptr);
        ~FFmpeg();
        bool load (QString fp);
        QString getErrorText();
        void    addErrorText(QString error);

        AVFormatContext   *getFormatContext() const;
        AVCodecContext    *getVideoCodecContext() const;
        AVCodecContext    *getAudioCodecContext() const;
        AVCodecParameters *getVideoCodecParameters() const;
        AVCodecParameters *getAudioCodecParameters() const;
        const AVCodec     *getVideoCodec() const;
        const AVCodec     *getAudioCodec() const;
        int               getVideoStreamIndex() const;
        int               getAudioStreamIndex() const;

    private:
        QString            file_path;
        QString            error_text;
        int                video_stream_index = -1;
        int                audio_stream_index = -1;
        AVFormatContext   *pFormatContext       = Q_NULLPTR;
        AVCodecContext    *pVideoCodecContext   = Q_NULLPTR;
        AVCodecContext    *pAudioCodecContext   = Q_NULLPTR;
        const AVCodec     *VideoCodec           = Q_NULLPTR;
        const AVCodec     *AudioCodec           = Q_NULLPTR;
        AVCodecParameters *VideoCodecParameters = Q_NULLPTR;
        AVCodecParameters *AudioCodecParameters = Q_NULLPTR;

        bool findStreams();
        bool prepareDecode();

    signals:

    };
}
#endif // FFMPEG_H
