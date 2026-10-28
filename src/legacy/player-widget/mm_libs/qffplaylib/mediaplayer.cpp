#include "mediaplayer.h"

QtFFmpegPlayer::MediaPlayer::MediaPlayer(QObject *parent)
    : QObject{parent}
{

}

QtFFmpegPlayer::MediaPlayer::~MediaPlayer()
{
}

void QtFFmpegPlayer::MediaPlayer::setRenderer(VideoOut *r)
{
    MyRenderer = r;
}

bool QtFFmpegPlayer::MediaPlayer::load(QString file_path)
{
    if (MyDecoder != Q_NULLPTR)
        return false;

    MyFFmpeg       = new  QtFFmpegPlayer::FFmpeg;
    MyDecoder      = new  QtFFmpegPlayer::Decoder(MyFFmpeg);
    MyDecoder->setRenderer(MyRenderer);
    MyDecodeThread = new QThread;
    MyDecoder->moveToThread(MyDecodeThread);
    connect(MyDecoder, SIGNAL(error(QString)), this, SLOT(setErrorText(QString)));
    connect(MyDecodeThread, &QThread::started, MyDecoder, &Decoder::decode);
    //connect(MyDecoder, &Decoder::finished, MyDecodeThread, &QThread::quit);
    connect(MyDecoder, SIGNAL(finished()), this, SLOT(doFinished()));
    emit mediaStatusChanged(LoadingMedia);

    if (!MyFFmpeg->load(file_path))
    {
        emit mediaStatusChanged(InvalidMedia);
        return false;
    }
    emit mediaStatusChanged(LoadedMedia);
    return true;
}

void QtFFmpegPlayer::MediaPlayer::play()
{
    if (MyDecoder == Q_NULLPTR)
        return;

    MyDecodeThread->start();
    emit mediaStatusChanged(BufferedMedia);
}

void QtFFmpegPlayer::MediaPlayer::stop()
{
    if (MyDecoder == Q_NULLPTR)
        return;

    MyDecoder->stop();
    MyDecodeThread->quit();
    MyDecodeThread->wait();
    setRenderer(Q_NULLPTR);
    delete MyDecodeThread;
    delete MyDecoder;
    delete MyFFmpeg;
    MyDecodeThread = Q_NULLPTR;
    MyDecoder      = Q_NULLPTR;
    MyFFmpeg       = Q_NULLPTR;
 }

void QtFFmpegPlayer::MediaPlayer::pause()
{
    if (MyDecoder == Q_NULLPTR)
        return;
    MyDecoder->pause();
    MyDecodeThread->quit();
}

void QtFFmpegPlayer::MediaPlayer::resume()
{
    if (MyDecoder == Q_NULLPTR)
        return;

    MyDecodeThread->start();
}

void QtFFmpegPlayer::MediaPlayer::seek(quint64 position)
{
    Q_UNUSED(position);
}

void QtFFmpegPlayer::MediaPlayer::setErrorText(QString error)
{
   error_string = error;
}

void QtFFmpegPlayer::MediaPlayer::doFinished()
{
    emit mediaStatusChanged(EndOfMedia);
}

