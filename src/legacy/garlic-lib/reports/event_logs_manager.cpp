/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2024 Nikolaos Saghiadinos <ns@smil-control.com>
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
#include "event_logs_manager.h"

Reporting::EventLogsManager::EventLogsManager(MainConfiguration *config, SystemInfos::DiscSpace *ds, QObject *parent) : Reporting::BaseReportManager(config, ds, parent)
{
    MyCreateEventLogs.reset(new Reporting::CreateEventLogs(MyConfiguration, this));
    MyCreateEventLogs.data()->setDiscSpace(ds);
    log_dir.setPath(MyConfiguration->getPaths("logs"));
}


void Reporting::EventLogsManager::handleSend()
{
    send_list = log_dir.entryList(QStringList("event_log*"), QDir::Files, QDir::Time | QDir::Reversed);
    send();
}


QString Reporting::EventLogsManager::checkForRotate(QString log_file)
{
    // when the actual used event_log.xml should sended we need to make a copy/rotate first
    // and send the event_log.xml.1
    // This prevents sending same logs again in next refresh
    if (log_file.mid(log_file.length()-1, 1) == "l")
    {
        Logger::getInstance().rotateLog("event");
        log_file += ".1";
    }
    return log_file;
}

void Reporting::EventLogsManager::send()
{
    if (send_list.size() > 0)
    {
        current_send_file_path = checkForRotate(send_list.at(0));
        MyCreateEventLogs.data()->process(current_send_file_path);
        MyWebDav.data()->processPutData(generateSendUrl(), MyCreateEventLogs.data()->asXMLString().toUtf8());
        send_list.removeFirst();
    }
    else
        current_send_file_path = "";
}

QUrl Reporting::EventLogsManager::generateSendUrl()
{
    return QUrl(action_url+"/event-"+MyConfiguration->createUuid().mid(0,23)+MyConfiguration->getUuid().mid(23)+".xml");
}

void Reporting::EventLogsManager::doSucceed(TNetworkAccess *uploader)
{
    qInfo(Develop) << "upload succeed" << uploader->getRemoteFileUrl().toString();
    QString remove = MyConfiguration->getPaths("logs") + current_send_file_path;
    QFile::remove(remove);
    send();
}

void Reporting::EventLogsManager::doFailed(TNetworkAccess *uploader)
{
    qWarning(Develop) << "upload failed" << uploader->getRemoteFileUrl().toString();
}

