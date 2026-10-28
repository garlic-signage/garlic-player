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
#include "system_report_manager.h"

Reporting::SystemReportManager::SystemReportManager(MainConfiguration *config, SystemInfos::DiscSpace *ds, QObject *parent) : Reporting::BaseReportManager(config, ds, parent)
{
    MyCreateSystemReport.reset(new Reporting::CreateSystemReport(MyConfiguration, this));
    MyCreateSystemReport.data()->setDiscSpace(ds);
}

void Reporting::SystemReportManager::handleSend()
{
    MyCreateSystemReport.data()->process();
    MyWebDav.data()->processPutData(action_url, MyCreateSystemReport.data()->asXMLString().toUtf8());
}

void Reporting::SystemReportManager::doSucceed(TNetworkAccess *uploader)
{
    qInfo(Develop) << "upload succeed" << uploader->getRemoteFileUrl().toString();
}

void Reporting::SystemReportManager::doFailed(TNetworkAccess *uploader)
{
    qWarning(Develop) << "upload failed" << uploader->getRemoteFileUrl().toString();
}

