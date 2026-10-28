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
#include "base_report_manager.h"

Reporting::BaseReportManager::BaseReportManager(MainConfiguration *config, SystemInfos::DiscSpace *ds, QObject *parent) : QObject(parent)
{
    MyConfiguration      = config;
    MyDiscSpace          = ds;
    MyWebDav.reset(new WebDav(MyConfiguration));
    connect(MyWebDav.data(), SIGNAL(succeed(TNetworkAccess *)), SLOT(doSucceed(TNetworkAccess *)));
    connect(MyWebDav.data(), SIGNAL(failed(TNetworkAccess *)), SLOT(doFailed(TNetworkAccess *)));
}

Reporting::BaseReportManager::~BaseReportManager()
{
    if (timer_id != 0)
        killTimer(timer_id);
}

void Reporting::BaseReportManager::init(QString action, int refresh)
{
    action_url = action;
    handleSend();
    timer_id = startTimer(refresh * 1000);
}

void Reporting::BaseReportManager::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == timer_id)
        handleSend();
}
