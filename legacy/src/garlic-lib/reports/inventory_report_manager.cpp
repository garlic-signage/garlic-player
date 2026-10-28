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
#include "inventory_report_manager.h"

Reporting::InventoryReportManager::InventoryReportManager(MainConfiguration *config, DB::InventoryTable *inv_table, SystemInfos::DiscSpace *ds, QObject *parent) : Reporting::BaseReportManager(config, ds, parent)
{
    MyCreateInventoryReport.reset(new Reporting::CreateInventoryReport(MyConfiguration, this));
    MyCreateInventoryReport.data()->setDiscSpace(ds);
    MyInventoryTable = inv_table;

}


void Reporting::InventoryReportManager::handleSend()
{
    MyCreateInventoryReport.data()->process(MyInventoryTable->findAll());
    MyWebDav.data()->processPutData(action_url, MyCreateInventoryReport.data()->asXMLString().toUtf8());
}

void Reporting::InventoryReportManager::doSucceed(TNetworkAccess *uploader)
{
    qInfo(Develop) << "upload succeed" << uploader->getRemoteFileUrl().toString();
}

void Reporting::InventoryReportManager::doFailed(TNetworkAccess *uploader)
{
    qWarning(Develop) << "upload failed" << uploader->getRemoteFileUrl().toString();
}
