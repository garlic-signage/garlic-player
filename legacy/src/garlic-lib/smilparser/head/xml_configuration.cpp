/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2021 Nikolaos Sagiadinos <ns@smil-control.com>
    This file is part of the garlic-player source code

    This program is free software: you can redistribute it and/or  modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************************/
#include "xml_configuration.h"

SmilHead::XMLConfiguration::XMLConfiguration(DB::InventoryTable *it, MainConfiguration *config, FreeDiscSpace *fds, QObject *parent) : BaseManager(config, fds, parent)
{
    XMLDownloader        = new Downloader(fds, it, MyConfiguration, this);
    connect(XMLDownloader, SIGNAL(succeed(TNetworkAccess*)), SLOT(doSucceed(TNetworkAccess*)));
}

void SmilHead::XMLConfiguration::processFromUrl(QUrl config_url)
{
    QFile::remove(MyConfiguration->getPaths("cache")+"configuration.xml"); // remove probably old file

    XMLDownloader->processFile(config_url, MyConfiguration->getPaths("cache")+"configuration.xml");

}

void SmilHead::XMLConfiguration::processFromLocalFile(QString file_path)
{
    if (!loadDocument(file_path))
        return;
    parse();
}

bool SmilHead::XMLConfiguration::loadDocument(QString file_path)
{
    QFile       file(file_path);
    QStringList list;
    list << "instanceId" << "display:0"
         << "resourceURI" << file_path
         << "contentLength" << QString::number(file.size())
         << "lastModifiedTime" << QFileInfo(file).lastModified().toString(Qt::ISODate);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCritical(MediaControl) << Logger::getInstance().createEventLogMetaData("CONFIGURATION_OPEN_ERROR", list);
        file.close();
        return false;
    }
    QString error_message = "";
    int error_line, error_column = 0;
    if (!document.setContent(&file, &error_message, &error_line, &error_column))
    {
        list << "errorMessage" << error_message
             << "errorLine" << QString::number(error_line)
             << "errorColumn" << QString::number(error_column);
        qCritical(MediaControl) << Logger::getInstance().createEventLogMetaData("CONFIGURATION_XML_ERROR", list);
        file.close();
        return false;
    }
    file.close();
    qInfo(MediaControl) << Logger::getInstance().createEventLogMetaData("CONFIGURATION_LOADED", list);
    return true;
}


void SmilHead::XMLConfiguration::doSucceed(TNetworkAccess *network)
{
    Q_UNUSED(network);
    QString path = MyConfiguration->getPaths("cache")+"configuration.xml";
    renameDownloadedFile(path);
    processFromLocalFile(path);
}

void SmilHead::XMLConfiguration::parse()
{
    QDomNodeList node_list = document.elementsByTagName("prop");
    QString attr_name      = "";
    QString attr_value     = "";
    for(int i = 0; i < node_list.size(); i++)
    {
        attr_name  = getAttribute(node_list.at(i).toElement(), "name");
        attr_value = getAttribute(node_list.at(i).toElement(), "value");
        if (attr_value == "")
            continue;

        // most are unused and will be proceed by a launcher concept (only android currently)
        // However, let them here, cause maybe a linux launcher could work better together with the player
        if (attr_name == "info.playerName")
        {
            MyConfiguration->setPlayerName(attr_value);
        }
        else if (attr_name == "content.serverUrl")
        {
            MyConfiguration->setIndexUri(attr_value);
        }
        else if (attr_name == "display.brightness" || attr_name == "brightness")
        {
            MyConfiguration->setUserConfigByKey("display/brightness", attr_value);
        }
        else if (attr_name == "hardware.videoOut.0.rotation")
        {
            MyConfiguration->setUserConfigByKey("display/rotation", attr_value);
        }
        else if (attr_name == "audio.soundlevel" || attr_name == "volume" || attr_name == "hardware.audioOut.0.masterSoundLevel")
        {
            MyConfiguration->setUserConfigByKey("audio/volume", attr_value);
        }
        else if (attr_name == "time.tzDescription")
        {
            MyConfiguration->setUserConfigByKey("time/tzDescription", attr_value);
        }
        else if (attr_name == "time.tzCode")
        {
            MyConfiguration->setUserConfigByKey("time/tzCode", attr_value);
        }
        else if (attr_name == "time.autoUpdate.protocol")
        {
            MyConfiguration->setUserConfigByKey("time/protocol", attr_value);
        }
        else if (attr_name == "time.autoUpdate.server")
        {
            MyConfiguration->setUserConfigByKey("time/server", attr_value);
        }
        else if (attr_name == "task.scheduledReboot.days" || attr_name == "schedule.reboot.days")
        {
            MyConfiguration->setRebootDays(attr_value);
        }
        else if (attr_name == "task.scheduledReboot.time" || attr_name == "schedule.reboot.time")
        {
            MyConfiguration->setRebootTime(attr_value);
        }
        else if (attr_name == "standby_mode")
        {
            MyConfiguration->setStandbyMode(attr_value);
        }
    }

    emit finishedConfiguration(); // should lead to reboot;
}


QString SmilHead::XMLConfiguration::getAttribute(QDomElement element, QString attr_name)
{
    QString ret = "";
    if (element.hasAttribute(attr_name))
    {
        ret = element.attribute(attr_name);
    }
    return ret;
}
