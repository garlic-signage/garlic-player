#ifndef AUDIO_H
#define AUDIO_H

#include <QTimer>
#include "player_base_media.h"

class Audio : public PlayerBaseMedia
{
        Q_OBJECT
    public:
        Audio(QQmlComponent *mc, QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent = nullptr);
        ~Audio();
        void                        loadMedia(BaseMedia *media, Region *reg);
        void                        loadInternal();
        void                        restart();
        void                        play();
        void                        stop();
        void                        resume();
        void                        pause();
        void                        setParentItem(QQuickItem *parent);
        void                        changeSize(int w, int h);
    protected:
        static const int KILLTIMER_INTERVALL = 3000; // 3 seconds because some devices needs eternities to start videos
        QScopedPointer<QQuickItem, QScopedPointerDeleteLater>  media_item;
        qreal                       determineVolume(QString percent);
        QQmlComponent              *media_component;
        QQuickItem                 *parent_item;
        QTimer                     *KillTimer;
        QString                     qml;
        int                         last_position = -1; // sometimes first getCurrentPosition can be 0.
        int                         getCurrentPosition();
    public slots:
        void                        doStopped();
        void                        checkForKill();
};

#endif // AUDIO_H
