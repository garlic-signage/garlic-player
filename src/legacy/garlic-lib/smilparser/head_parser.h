/************7*************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2016 Nikolaos Saghiadinos <ns@smil-control.com>
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

#ifndef HEAD_H
#define HEAD_H

#include <algorithm>
#include <QFile>
#include <QString>
#include <QtXml>
#include <QList>

#include "head/subscription.h"
#include "head/task_scheduler.h"
#include "head/placeholder.h"
#include "files/media_manager.h"
#include "reports/system_report_manager.h"
#include "reports/event_logs_manager.h"
#include "reports/play_logs_manager.h"
#include "reports/inventory_report_manager.h"
#include "smilparser/base.hpp"
/**
 * @brief The Region struct
 * All values are QString cause there could be a percent sign (%) in SMIL
 */
struct Region
{
    QString id         = "";
    QString regionName = "";
    qreal top = 0;
    qreal left = 0;
    qreal width = 1;
    qreal height = 1;
    int z_index = 0;
    QString fit              = "hidden";
    QString mediaAlign       = "topLeft";
    QString soundLevel      = "100%";
    QString backgroundColor  = "transparent";
    QString backgroundImage  = "none";
    QString backgroundRepeat = "repeat";
    bool operator<(const Region& other) const {return z_index < other.z_index;} // use for sorting a list of Regions
};


class HeadParser: public QObject
{
        Q_OBJECT
    public:
        explicit HeadParser(MainConfiguration *config, Files::MediaManager *mm, DB::InventoryTable *it, SmilHead::PlaceHolder *ph, SystemInfos::DiscSpace *ds, QObject *parent = Q_NULLPTR);
         ~HeadParser();
        void                   setDefaultValues();
        void                   parse(QDomElement head, SmilHead::TaskScheduler *MyTasks);
        int                    getRefreshTime(){return refresh;}
        QString                getRootBackgroundColor();
        QString                getTitle();
        QList<Region>         *getLayout();
        void                   setRootLayout(int w, int h);
    protected:
        const     int        MAX_SECONDS_WAIT  = 10;

        SystemInfos::DiscSpace              *MyDiscSpace;
        DB::InventoryTable                  *MyInventoryTable = Q_NULLPTR;
        QScopedPointer<Reporting::SystemReportManager> MySystemReportManager;
        QScopedPointer<Reporting::InventoryReportManager> MyInventoryReportManager;
        QScopedPointer<Reporting::EventLogsManager>    MyEventLogsManager;
        QScopedPointer<Reporting::PlayLogsManager>    MyPlayLogsManager;
        int                    timer_id = 0;
        int                    timer_counter = 0;
        Region                 default_region;
        QString                title;
        bool                   has_backgroundimage = false;
        int                    refresh, width, height;
        QString                backgroundColor = "transparent";
        QString                backgroundImage = "none";
        QString                backgroundRepeat= "repeat";
        QDomElement            head;
        QList<Region>          region_list;
        MainConfiguration     *MyConfiguration;
        SmilHead::PlaceHolder *MyPlaceHolder;
        Files::MediaManager   *MyMediaManager;
        void                   parseMeta(QDomElement element);
        void                   parseMetaData(QDomElement element, SmilHead::TaskScheduler *MyTasks);
        void                   parseLayout(QDomElement layout);
        void                   parseState(QDomElement state);
        void                   parseRootLayout(QDomElement root_layout);
        void                   parseRegions(QDomNodeList childs);
        void                   handleBackgroundImage(QString value);
        qreal                  calculatePercentBasedOnRoot(QString value, qreal root);
        void                   timerEvent(QTimerEvent *event);
    private:
        bool                   isMediaLoadable(QString src);
    signals:
        void                   parsingCompleted();
};

#endif // HEAD_H
