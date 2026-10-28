#include "decoder.h"

#define MINIAUDIO_IMPLEMENTATION
//#include "miniaudio.h"

QtFFmpegPlayer::Decoder::Decoder(FFmpeg *ff, QObject *parent) : QObject(parent)
{
    MyFFmpeg = ff;
}

QtFFmpegPlayer::Decoder::~Decoder()
{

}


void QtFFmpegPlayer::Decoder::decode()
{
    // fill the Packet with data from the Stream
    decoder_status = decoding;
    AVPacket pPacket;
    while (decoder_status == decoding)
    {
        if (av_read_frame(MyFFmpeg->getFormatContext(), &pPacket))
        {
            emit error("Failed to read frame");
            break;
        }
        if (pPacket.stream_index == MyFFmpeg->getVideoStreamIndex())
        {
            if (decodeVideoPacket(&pPacket) < 0)
            {
                emit error("Failed to decode video packet");
                break;
            }
            // Calculate the Frame rate to determine the wait until next frame can be decoded
            delay(1000/frame_rate);
        }
     /*   if (pPacket.stream_index == MyFFmpeg->getAudioStreamIndex())
        {
            if (decodeAudioPacket(&pPacket) < 0)
            {
                emit error("Failed to decode audio packet");
                break;
            }
        }
      */
        av_packet_unref(&pPacket);
    }

    if (decoder_status == decoding)
        emit finished();
}

void QtFFmpegPlayer::Decoder::stop()
{
    if (decoder_status == decoding)
        decoder_status = stopped;
}

void QtFFmpegPlayer::Decoder::pause()
{
    if (decoder_status == decoding)
        decoder_status = paused;
}

void QtFFmpegPlayer::Decoder::seek(int position)
{

}

void QtFFmpegPlayer::Decoder::setRenderer(VideoOut *r)
{
    MyVideoOut = r;
}

int QtFFmpegPlayer::Decoder::decodeVideoPacket(AVPacket *pPacket)
{
    // Supply raw packet data as input to a decoder
    int response = avcodec_send_packet(MyFFmpeg->getVideoCodecContext(), pPacket);
    if (response < 0 && response != AVERROR(EAGAIN))
    {
        emit error("Failed to send packet");
        return response;
    }

    while (response >= 0)
    {
       AVFrame *pFrame = av_frame_alloc();
        // Return decoded output data (into a frame) from a decoder
        response = avcodec_receive_frame(MyFFmpeg->getVideoCodecContext(), pFrame);
        if (response < 0)
        {
            av_frame_unref(pFrame);
            emit error("Failed to receive frame");
            break;
        }
        else if (response < 0)
        {
            av_frame_unref(pFrame);
            av_frame_free(&pFrame);
            return response;
        }

        MyVideoOut->placeDecodedImage(convertFrameToImage(pFrame));
        // MyVideoOut->placeDecodedImage(QImage());
        calculateFrameRate(pFrame);

        av_frame_unref(pFrame);
        av_frame_free(&pFrame);
    }
    return 0;
}
/*
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    AVAudioFifo *fifo = reinterpret_cast<AVAudioFifo *>(pDevice->pUserData);
    av_audio_fifo_read(fifo, &pOutput, frameCount);

    (void)pInput;
}
*/

int QtFFmpegPlayer::Decoder::decodeAudioPacket(AVPacket *pPacket)
{
/*    // Supply raw packet data as input to a decoder
    int response = avcodec_send_packet(MyFFmpeg->getAudioCodecContext(), pPacket);
    if (response < 0 && response != AVERROR(EAGAIN))
    {
        emit error("Failed to send packet");
        return response;
    }

    AVStream *stream      = MyFFmpeg->getFormatContext()->streams[MyFFmpeg->getAudioStreamIndex()];
    SwrContext* resampler = swr_alloc_set_opts(Q_NULLPTR,
                                  stream->codecpar->channel_layout,
                                  AV_SAMPLE_FMT_FLT,
                                  stream->codecpar->sample_rate,
                                  stream->codecpar->channel_layout,
                                  (AVSampleFormat)stream->codecpar->format,
                                  stream->codecpar->sample_rate, 0, Q_NULLPTR);

    AVAudioFifo *fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLT, stream->codecpar->channel_layout, 1);

    AVFrame *pFrame = av_frame_alloc();

    while (response >= 0)
    {
        // Return decoded output data (into a frame) from a decoder
        response = avcodec_receive_frame(MyFFmpeg->getAudioCodecContext(), pFrame);
        if (response < 0)
        {
            av_frame_unref(pFrame);
            emit error("Failed to receive frame");
            break;
        }
        else if (response < 0)
        {
            av_frame_unref(pFrame);
            av_frame_free(&pFrame);
            return response;
        }

        AVFrame *resampled_frame = av_frame_alloc();
        resampled_frame->sample_rate    = pFrame->sample_rate;
        resampled_frame->ch_layout      = pFrame->ch_layout;
      //  resampled_frame->channels       = pFrame->channels;
        resampled_frame->format         = AV_SAMPLE_FMT_FLT;

        swr_convert_frame(resampler, resampled_frame, pFrame);
        av_frame_unref(pFrame);

        av_audio_fifo_write(fifo, (void**)resampled_frame->data, resampled_frame->nb_samples);
        av_frame_free(&resampled_frame);
    }

    ma_device_config deviceConfig;
    ma_device device;

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = stream->codecpar->channels;
    deviceConfig.sampleRate        = stream->codecpar->sample_rate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = fifo;

    av_frame_free(&pFrame);
    swr_free(&resampler);

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS)
    {
        printf("Failed to open playback device.\n");
        return -3;
    }

    if (ma_device_start(&device) != MA_SUCCESS)
    {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        return -4;
    }

    while (av_audio_fifo_size(fifo));

    ma_device_uninit(&device);
    av_audio_fifo_free(fifo);
    */
    return 0;

}

void QtFFmpegPlayer::Decoder::calculateFrameRate(AVFrame *frame)
{

   // This will not work good with webm, because you need to parse the whole file first.
   // https://stackoverflow.com/a/32538549
   AVRational fr = av_guess_frame_rate(MyFFmpeg->getFormatContext(), MyFFmpeg->getFormatContext()->streams[MyFFmpeg->getVideoStreamIndex()], frame);

/*   if (fr.num > 0)
       frame_rate = fr.num / fr.den;
    else*/
       frame_rate = 30;
}

QImage QtFFmpegPlayer::Decoder::convertFrameToImage(AVFrame *pFrame)
{
    AVPixelFormat pixFormat;
    bool changeColorspaceDetails = false;
    switch (MyFFmpeg->getVideoCodecContext()->pix_fmt)
      {
        case AV_PIX_FMT_YUVJ420P:
          pixFormat = AV_PIX_FMT_YUV420P;
          changeColorspaceDetails = true;
          break;
        case AV_PIX_FMT_YUVJ422P:
          pixFormat = AV_PIX_FMT_YUV422P;
          changeColorspaceDetails = true;
          break;
        case AV_PIX_FMT_YUVJ444P:
          pixFormat = AV_PIX_FMT_YUV444P;
          changeColorspaceDetails = true;
          break;
        case AV_PIX_FMT_YUVJ440P:
          pixFormat = AV_PIX_FMT_YUV440P;
          changeColorspaceDetails = true;
          break;
        default:
          pixFormat = MyFFmpeg->getVideoCodecContext()->pix_fmt;
      }

    SwsContext* img_convert_ctx = sws_getContext(pFrame->width, pFrame->height, pixFormat,
                                     pFrame->width, pFrame->height,
                                     AV_PIX_FMT_RGB24,
                                     SWS_BICUBIC, NULL, NULL, NULL);

    if(!img_convert_ctx)
    {
        emit error("Failed to create sws context");
        return QImage();
    }

    if (changeColorspaceDetails)
    {
        // change the range of input data by first reading the current color space and then setting it's range as yuvj.
        int dummy[4];
        int srcRange, dstRange;
        int brightness, contrast, saturation;
        sws_getColorspaceDetails(img_convert_ctx, (int**)&dummy, &srcRange, (int**)&dummy, &dstRange, &brightness, &contrast, &saturation);
        const int* coefs = sws_getCoefficients(SWS_CS_DEFAULT);
        srcRange = 1; // this marks that values are according to yuvj
        sws_setColorspaceDetails(img_convert_ctx, coefs, srcRange, coefs, dstRange, brightness, contrast, saturation);
      //  delete coefs;
    }

    // prepare line sizes structure as sws_scale expects
    int rgb_linesizes[8] = {0};
    rgb_linesizes[0]     = 3*pFrame->width;

    // prepare char buffer in array, as sws_scale expects
    unsigned char* rgbData[8];
    int imgBytesSyze = 3*pFrame->height*pFrame->width;

    // as explained above, we need to alloc extra 64 bytes
  //  rgbData[0] = new unsigned char[imgBytesSyze+64];
    rgbData[0] = (unsigned char *)malloc(imgBytesSyze+64);

    if(!rgbData[0])
    {
        emit error("Error allocating buffer for frame conversion");
        free(rgbData[0]);
        sws_freeContext(img_convert_ctx);
        return QImage();
    }

    if(sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0, pFrame->height, rgbData, rgb_linesizes) != pFrame->height)
    {
        emit error("Error changing frame color range");
        free(rgbData[0]);
        sws_freeContext(img_convert_ctx);
        return QImage();
    }

    // then create QImage and copy converted frame data into it
    QImage image(pFrame->width, pFrame->height, QImage::Format_RGB888);
    for(int y = 0; y < pFrame->height; y++)
    {
        memcpy(image.scanLine(y), rgbData[0] + y*3*pFrame->width, 3*pFrame->width);
    }

    free(rgbData[0]);
    sws_freeContext(img_convert_ctx);

    return image;
 //   return QImage();
}


void QtFFmpegPlayer::Decoder::delay(int msec)
{
    QTime dieTime = QTime::currentTime().addMSecs(msec);
    while(QTime::currentTime() < dieTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}
