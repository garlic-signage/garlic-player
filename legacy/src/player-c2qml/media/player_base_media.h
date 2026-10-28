#ifndef BASE_MEDIA_H
#define BASE_MEDIA_H

#include <QScopedPointer>
#include "QtQuick/qquickitem.h"
#include "smilparser/media/base_media.h"
#include "tools/logger.h"
#include "../player-common/launcher.h"

using namespace MediaParser;

class PlayerBaseMedia : public QObject
{
        Q_OBJECT
    public:
        explicit PlayerBaseMedia(QQmlComponent *mc, QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent = nullptr);
        virtual void          loadMedia(BaseMedia *media, Region *reg)   = 0;
        virtual void          restart() = 0;
        virtual void          play()   = 0;
        virtual void          stop()   = 0;
        virtual void          pause()   = 0;
        virtual void          resume()   = 0;
        virtual void          setParentItem(QQuickItem *parent)   = 0;
        BaseMedia            *getSmilMedia();
        QString               getRegionId() const;
        void                  setStartTime();
        QString               createPlayLogXml();
        void                  setRegionId(const QString &value);
        virtual void          changeSize(int w, int h) = 0;
    protected:
        Logger&               MyLogger = Logger::getInstance();
        MainConfiguration    *MyMainConfiguration;
        Launcher             *MyLauncher;
        Region               *region;
        BaseMedia            *SmilMedia;
        QString               start_time = "";
        QString               region_id  = "";

        QQuickItem           *createMediaItem(QQmlComponent *mc, QString str);
        bool                  load(QQuickItem *item);
        bool                  isFileExists(QString path);
};
#endif // BASE_MEDIA_H
