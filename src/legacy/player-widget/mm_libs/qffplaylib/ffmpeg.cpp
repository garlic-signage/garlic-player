#include "ffmpeg.h"

QtFFmpegPlayer::FFmpeg::FFmpeg(QObject *parent) : QObject(parent)
{
    pFormatContext = avformat_alloc_context();
}

QtFFmpegPlayer::FFmpeg::~FFmpeg()
{
    avcodec_close(pVideoCodecContext);
    avcodec_free_context(&pVideoCodecContext);
    avcodec_close(pAudioCodecContext);
    avcodec_free_context(&pAudioCodecContext);
    avformat_close_input(&pFormatContext);
    avformat_free_context(pFormatContext);
}

bool QtFFmpegPlayer::FFmpeg::load(QString fp)
{
    if (!pFormatContext)
    {
        addErrorText("ERROR could not allocate memory for Format Context");
        return false;
    }

    file_path = fp;
    if (fp.isEmpty())
    {
        addErrorText("filepath is empty");
        return false;
    }

    if (avformat_open_input(&pFormatContext, fp.toStdString().c_str(), NULL, NULL) != 0)
    {
        addErrorText("ERROR could not open the file");
        return false;
    }

    if (!findStreams())
        return false;

    if (!prepareDecode())
        return false;

    return true;
}


QString QtFFmpegPlayer::FFmpeg::getErrorText()
{
    return error_text;
}

void QtFFmpegPlayer::FFmpeg::addErrorText(QString error)
{
    error_text = error;
}

bool QtFFmpegPlayer::FFmpeg::findStreams()
{
    if (avformat_find_stream_info(pFormatContext,  NULL) < 0)
    {
        addErrorText("ERROR could not get the stream info");
        avformat_close_input(&pFormatContext);
        return false;
    }

    video_stream_index    = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    VideoCodecParameters  = pFormatContext->streams[video_stream_index]->codecpar;
    VideoCodec            = avcodec_find_decoder(VideoCodecParameters->codec_id);

    audio_stream_index    = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_stream_index >= 0)
    {
        AudioCodecParameters  = pFormatContext->streams[audio_stream_index]->codecpar;
        AudioCodec            = avcodec_find_decoder(AudioCodecParameters->codec_id);
    }

    if (video_stream_index < 0 && audio_stream_index < 0)
    {
       addErrorText("File " + file_path + " does not contain neither a video nor an audio stream!");
       avformat_close_input(&pFormatContext);
       return false;
    }

     return true;
}

bool QtFFmpegPlayer::FFmpeg::prepareDecode()
{
    // to do put this crap in a method to not dublicate code
    pVideoCodecContext = avcodec_alloc_context3(VideoCodec);
    if (!pVideoCodecContext)
    {
        addErrorText("failed to allocated memory for Video AVCodecContext");
        return false;
    }

    int error = avcodec_parameters_to_context(pVideoCodecContext, VideoCodecParameters);
    // Fill the codec context based on the values from the supplied codec parameters
    if (error < 0)
    {
        addErrorText("failed to copy codec params to video codec context");
        return false;
    }

    error = avcodec_open2(pVideoCodecContext, VideoCodec, NULL);
    // Initialize the AVCodecContext to use the given AVCodec.
    if (error < 0)
    {
        addErrorText("failed to open codec through avcodec_open2");
        return false;
    }

    if (audio_stream_index >= 0)
    {
        pAudioCodecContext = avcodec_alloc_context3(AudioCodec);
        if (!pAudioCodecContext)
        {
            addErrorText("failed to allocated memory for Audio AVCodecContext");
            return false;
        }

        error = avcodec_parameters_to_context(pAudioCodecContext, AudioCodecParameters);
        // Fill the codec context based on the values from the supplied codec parameters
        if (error < 0)
        {
            addErrorText("failed to copy codec params to audio codec context");
            return false;
        }

        error = avcodec_open2(pAudioCodecContext, AudioCodec, NULL);
        // Initialize the AVCodecContext to use the given AVCodec.
        if (error < 0)
        {
            addErrorText("failed to open codec through avcodec_open2");
            return false;
        }
    }

    return true;
}

AVFormatContext *QtFFmpegPlayer::FFmpeg::getFormatContext() const
{
    return pFormatContext;
}

AVCodecContext *QtFFmpegPlayer::FFmpeg::getVideoCodecContext() const
{
    return pVideoCodecContext;
}

AVCodecContext *QtFFmpegPlayer::FFmpeg::getAudioCodecContext() const
{
    return pAudioCodecContext;
}

AVCodecParameters *QtFFmpegPlayer::FFmpeg::getVideoCodecParameters() const
{
    return VideoCodecParameters;
}

AVCodecParameters *QtFFmpegPlayer::FFmpeg::getAudioCodecParameters() const
{
    return AudioCodecParameters;
}

const AVCodec *QtFFmpegPlayer::FFmpeg::getVideoCodec() const
{
    return VideoCodec;
}

const AVCodec *QtFFmpegPlayer::FFmpeg::getAudioCodec() const
{
    return AudioCodec;
}

int QtFFmpegPlayer::FFmpeg::getVideoStreamIndex() const
{
    return video_stream_index;
}

int QtFFmpegPlayer::FFmpeg::getAudioStreamIndex() const
{
    return audio_stream_index;
}
