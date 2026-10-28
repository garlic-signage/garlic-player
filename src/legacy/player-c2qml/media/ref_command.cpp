#include "ref_command.h"

RefCommand::RefCommand(QQmlComponent *mc, QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent) : PlayerBaseMedia(mc, r_id, lc, config, parent)
{
    setRegionId(r_id);
    MyLauncher = lc;

    QString str("import QtQuick 2.12\n \
                 Item{\n \
                        id: "+getRegionId()+"_web;\n \
                        anchors.fill: parent;\n \
                   }\n"
    );
    ref_item.reset(createMediaItem(mc, str));

}

RefCommand::~RefCommand()
{
}

void RefCommand::loadMedia(BaseMedia *media, Region *reg)
{
    SmilMedia = media;
    region    = reg;
}

void RefCommand::restart()
{
    stop();
    play();
}

void RefCommand::play()
{
    QString source = SmilMedia->getLoadablePath();
    if (source.toLower() == "adapi:blankscreen")
    {
        if (MyMainConfiguration->getStandbyMode() == MyMainConfiguration->STANDBY_MODE_PARTIALLY)
            MyLauncher->toggleScreenActivity(false);
        else if (MyMainConfiguration->getStandbyMode() == MyMainConfiguration->STANDBY_MODE_DEEP)
            MyLauncher->activateDeepStandby(QString::number(SmilMedia->determineNextEndTime()));

        if (SmilMedia->getLogContentId() != "")
            setStartTime();
    }
}


void RefCommand::stop()
{
    // check if this is work when values are changuing meanwhile
    if (MyMainConfiguration->getStandbyMode() == MyMainConfiguration->STANDBY_MODE_PARTIALLY)
        MyLauncher->toggleScreenActivity(true);

    if (!SmilMedia->getLogContentId().isEmpty())
        qInfo(PlayLog).noquote() << createPlayLogXml();
}

void RefCommand::resume()
{

}

void RefCommand::pause()
{

}

void RefCommand::changeSize(int w, int h)
{
    Q_UNUSED(w);Q_UNUSED(h)
}
void RefCommand::setParentItem(QQuickItem *parent)
{
    Q_UNUSED(parent);
}


