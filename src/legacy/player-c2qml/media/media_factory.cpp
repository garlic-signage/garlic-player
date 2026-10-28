#include "media_factory.h"
#include "media/image.h"
#include "media/brush.h"
#include "media/web.h"
#include "media/video.h"
#include "media/audio.h"
#include "media/ref_command.h"

MediaFactory::MediaFactory(QQmlComponent *m, QString id, Launcher *l, MainConfiguration *config, QObject *parent) : QObject(parent)
{
    media_component = m;
    region_name     = id;
    MyLauncher      = l;
    MyMainConfiguration = config;

}

PlayerBaseMedia *MediaFactory::create(BaseMedia *media)
{
    QString type   = media->objectName();
    if (type == "TImage")
    {
        MyImage = new Image(media_component, region_name, MyLauncher, MyMainConfiguration, this);
        QQmlEngine::setObjectOwnership(MyImage, QQmlEngine::CppOwnership);
        return MyImage;
    }
    else if (type == "TBrush")
    {
        MyBrush = new Brush(media_component, region_name, MyLauncher, MyMainConfiguration, this);
        QQmlEngine::setObjectOwnership(MyBrush, QQmlEngine::CppOwnership);
        return MyBrush;
    }
    else if (type == "TVideo")
    {
        MyVideo = new Video(media_component, region_name, MyLauncher, MyMainConfiguration, this);
        QQmlEngine::setObjectOwnership(MyVideo, QQmlEngine::CppOwnership);
        return MyVideo;
    }
    else if (type == "TAudio")
    {
        MyAudio = new Audio(media_component, region_name, MyLauncher, MyMainConfiguration, this);
        QQmlEngine::setObjectOwnership(MyAudio, QQmlEngine::CppOwnership);
        return MyAudio;
    }
    else if (type == "TWeb")
    {
        MyWeb = new Web(media_component, region_name, MyLauncher, MyMainConfiguration, this);
        QQmlEngine::setObjectOwnership(MyWeb, QQmlEngine::CppOwnership);
        return MyWeb;
    }
    else if (type == "TWidget")
    {
        MyWeb = new Web(media_component, region_name, MyLauncher, MyMainConfiguration, this);
        QQmlEngine::setObjectOwnership(MyWeb, QQmlEngine::CppOwnership);
        return MyWeb;
    }
    else if (type == "TRefCommand")
    {
        MyRefCommand = new RefCommand(media_component, region_name, MyLauncher, MyMainConfiguration, this);
        QQmlEngine::setObjectOwnership(MyRefCommand, QQmlEngine::CppOwnership);
        return MyRefCommand;
    }
    else
        return Q_NULLPTR;
}
