#ifndef MEDIA_FACTORY_H
#define MEDIA_FACTORY_H

#include "QtQml/qqmlcomponent.h"
#include "base_media.h"
#include "media/player_base_media.h"
#include <QQmlEngine>
#include <QObject>

class MediaFactory : public QObject
{
        Q_OBJECT
    public:
        explicit MediaFactory(QQmlComponent *m, QString id, Launcher *l, MainConfiguration *config, QObject *parent = nullptr);
        PlayerBaseMedia *create(BaseMedia *media);

    private:
        PlayerBaseMedia *MyVideo, *MyImage, *MyBrush, *MyAudio, *MyWeb, *MyRefCommand;
        MainConfiguration *MyMainConfiguration;
        QQmlComponent *media_component;
        QString region_name;
        Launcher *MyLauncher;

};

#endif // MEDIA_FACTORY_H
