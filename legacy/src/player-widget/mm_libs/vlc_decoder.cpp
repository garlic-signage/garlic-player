#ifdef SUPPORT_LIBVLC

#include "vlc_decoder.h"

#ifdef QT_DEBUG
void logCallback(void *data, int level, const libvlc_log_t *ctx, const char *fmt, va_list args)
{
   Q_UNUSED(ctx);
   Q_UNUSED(data);

/*    char *result;
    if (vasprintf(&result, fmt, args) < 0)
    {
        return;
    }

    QString message(result);
    free(result);

    switch (level)
    {
        case LIBVLC_ERROR:
            qCritical(message.toUtf8().data(), NULL);
            break;
        case LIBVLC_WARNING:
            qWarning(message.toUtf8().data(), NULL);
            break;
        case LIBVLC_NOTICE:
            qInfo(message.toUtf8().data(), NULL);
            break;
        case LIBVLC_DEBUG:
        default:
            qDebug(message.toUtf8().data(), NULL);
            break;
    }
*/
}
#endif

VlcDecoder::VlcDecoder(QObject *parent) : QObject(parent)
{

#if defined(Q_OS_MACX)
    const char *arguments[] = {""};
#elif defined(Q_OS_UNIX)
    const char *arguments[] = {""}; // seg fault in bullseye {"--avcodec-hw=any"}; // set hw acceleration especially for raspberry
#else
    const char *arguments[] = {""};
#endif

    vlcInstance = libvlc_new(sizeof(arguments) / sizeof(arguments[0]), arguments);
     if (vlcInstance)
    {
#ifdef QT_DEBUG
//        libvlc_log_set(vlcInstance, logCallback, NULL); // Todo: Fix this
#endif
        vlcPlayer    = libvlc_media_player_new(vlcInstance);
    }
    else
        qCritical() << "VLC-Qt Error: libvlc failed to load!";
}

VlcDecoder::~VlcDecoder()
{
    libvlc_media_player_release(vlcPlayer);
    libvlc_release(vlcInstance);
}

void VlcDecoder::setVideoOutput(MediaWidgetWrapper *renderer)
{
    Renderer = renderer;

#if defined(Q_OS_MAC)

    NSView *video = reinterpret_cast<NSView*>(Renderer->getVideoWidget()->winId());
   // QMacCocoaViewContainer *container = new QMacCocoaViewContainer(video, Renderer->getVideoWidget());

    libvlc_media_player_set_nsobject(vlcPlayer, video);

#elif defined(Q_OS_UNIX)
    libvlc_media_player_set_xwindow(vlcPlayer, Renderer->getVideoRenderer()->winId());
#elif defined(Q_OS_WIN)
    libvlc_media_player_set_hwnd(vlcPlayer, (HWND) Renderer->getVideoWidget()->winId());
#endif

}

void VlcDecoder::removeVideoOutput(MediaWidgetWrapper *renderer)
{
    Q_UNUSED(renderer);
}

bool VlcDecoder::load(QString file_path)
{
    current_media_path = file_path;

#if defined(Q_OS_UNIX)
    vlcMedia = libvlc_media_new_path(vlcInstance, current_media_path.toStdString().c_str());
#elif defined(Q_OS_WIN)

    std::string bspath = current_media_path.toStdString();
    std::replace(bspath.begin(), bspath.end(), '/', '\\');
    vlcMedia = libvlc_media_new_path(vlcInstance, bspath.c_str());
#endif
    if (vlcMedia == NULL)
    {
        qCritical() << "VLC-Qt Error: vlcMedia failed to load!";
        return false;
    }

    libvlc_media_player_set_media(vlcPlayer, vlcMedia);

    PositionTimer.reset(new QTimer(this));
    connect(PositionTimer.data(), SIGNAL(timeout()), this, SLOT(updatePosition()));

    return true;
}

void VlcDecoder::restart()
{
    libvlc_media_player_set_time(vlcPlayer, 0);
}

void VlcDecoder::setVolume(QString percent)
{
    if (vlcPlayer == NULL ||vlcMedia == NULL)
        return;

    int vol = 0;
    if (percent.endsWith('%'))
        vol = percent.midRef(0, percent.length()-1).toInt();
    libvlc_audio_set_volume(vlcPlayer,vol);
}

void VlcDecoder::unload()
{
    if (vlcPlayer == NULL)
        return;
    libvlc_media_player_set_media(vlcPlayer, NULL);

    if (vlcMedia == NULL)
        return;
    libvlc_media_release(vlcMedia);
}

void VlcDecoder::play()
{
    if (vlcPlayer == NULL ||vlcMedia == NULL)
        return;

    libvlc_media_player_play(vlcPlayer);

    /* A timer to update the sliders */
    PositionTimer.data()->start(100);

}

void VlcDecoder::stop()
{
    if (vlcPlayer == NULL ||vlcMedia == NULL)
        return;

    libvlc_media_player_stop(vlcPlayer);
    PositionTimer.data()->stop();
}


void VlcDecoder::resume()
{
    play();
}

void VlcDecoder::pause()
{
    if (vlcPlayer == NULL)
        return;

    libvlc_media_player_pause(vlcPlayer);
    PositionTimer.data()->stop();
}

void VlcDecoder::updatePosition()
{
    if (vlcPlayer == NULL)
        return;

    libvlc_state_t status = libvlc_media_player_get_state(vlcPlayer);

    switch (status)
    {
        case libvlc_Opening:
            break;
        case libvlc_Buffering:
            break;
        case libvlc_Playing:
            break;
        case libvlc_Paused:
            break;
        case libvlc_Stopped:
            PositionTimer.data()->stop();
            break;
        case libvlc_Ended:
            PositionTimer.data()->stop();
            emit finished();
            break;
        case libvlc_Error:
            qWarning(Develop) << "Error" << current_media_path;
            break;

        case libvlc_NothingSpecial:
        default:
            break;
    }
}

void VlcDecoder::displayErrorMessage()
{

}
#endif
