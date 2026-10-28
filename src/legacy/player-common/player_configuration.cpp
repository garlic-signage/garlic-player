#include "player_configuration.h"

PlayerConfiguration::PlayerConfiguration(MainConfiguration *mc, QObject *parent) : QObject(parent)
{
    MyMainConfiguration = mc;
}

void PlayerConfiguration::determineInitConfigValues()
{
    determineUuid();
    MyMainConfiguration->determinePlayerName();
    if (!launcher_version.isEmpty())
    {
        // L is needed temporary by SmilControl-CMS to determine correct build number for
        // releasing screen time functionality Nov 2021
        MyMainConfiguration->setAdditionalVersion("L" + launcher_version);
        qInfo(System) << "Launcher version: " << launcher_version;
    }
    determineSmilIndexUri();
    MyMainConfiguration->determineUserAgent();

    QApplication::setApplicationName(MyMainConfiguration->getAppName());
    QApplication::setApplicationVersion(MyMainConfiguration->getVersion());
    QApplication::setApplicationDisplayName(MyMainConfiguration->getAppName());

    QDir dir(".");
    MyMainConfiguration->determineBasePath(dir.absolutePath());
}

void PlayerConfiguration::setHasLauncher(bool value)
{
    has_launcher = value;
}

void PlayerConfiguration::setUuidFromLauncher(QString value)
{
    launcher_uuid = value;
}

void PlayerConfiguration::setVersionFromLauncher(QString value)
{
    launcher_version = value;
}

void PlayerConfiguration::printVersionInformation()
{
    if (!launcher_version.isEmpty())
    {
        qInfo() << "Launcher version: " << launcher_version;
    }
    qInfo() << MyMainConfiguration->getAppName()+ ": " + MyMainConfiguration->getVersion() << " Operating System:" << MyMainConfiguration->getOS();
}

QString PlayerConfiguration::determineDefaultContentUrlName()
{
    QString ret = "SmilControl";
    QString tmp = "";

#ifdef DEFAULT_CONTENT_URL_NAME
    tmp = STRINGIFY(DEFAULT_CONTENT_URL_NAME);
    if (tmp != "")
        ret = tmp;
#endif

    return ret;
}

QString PlayerConfiguration::determineDefaultContentUrl()
{
    QString protocol = "https";
    QString url      = "indexes.smil-control.com";
    QString tmp      = "";

#ifdef DEFAULT_CONTENT_URL_NAME
    tmp =  STRINGIFY(DEFAULT_CONTENT_URL_PROTOCOL);
    if (tmp != "")
        protocol = tmp;
#endif

#ifdef DEFAULT_CONTENT_URL_NAME
    tmp = "";
    tmp =  STRINGIFY(DEFAULT_CONTENT_URL);
    if (tmp != "")
        url = tmp;
#endif

    return protocol + "://" + url;
}


void PlayerConfiguration::setSmilIndexUriFromLauncher(QString value)
{
    launcher_smil_index_uri = value;
    if (value != MyMainConfiguration->getIndexUri())
    {
        MyMainConfiguration->setIndexUri(value);
    }
}

void PlayerConfiguration::determineUuid()
{
    // if launcher has an uuid use it
    if (has_launcher && !launcher_uuid.isEmpty() && launcher_uuid != MyMainConfiguration->getUuid())
    {
        MyMainConfiguration->setUuid(launcher_uuid);
        return;
    }

    // if launcher has not an uuid and player has one use from player
    if (!MyMainConfiguration->getUuid().isEmpty() && !MyMainConfiguration->getUuid().isEmpty())
    {
        MyMainConfiguration->setUuid(MyMainConfiguration->getUuid());
        return;
    }

    // create UUID in Player
    MyMainConfiguration->setUuid(MyMainConfiguration->createUuid());
}

void PlayerConfiguration::determineSmilIndexUri()
{
    if (has_launcher && launcher_smil_index_uri != MyMainConfiguration->getIndexUri())
    {
        MyMainConfiguration->setIndexUri(launcher_smil_index_uri);
    }
}

void PlayerConfiguration::determinePlayerName()
{

}

