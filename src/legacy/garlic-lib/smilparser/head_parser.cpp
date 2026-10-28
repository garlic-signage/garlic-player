/*************************************************************************************
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

#include "head_parser.h"

HeadParser::HeadParser(MainConfiguration *config, Files::MediaManager *mm, DB::InventoryTable *it, SmilHead::PlaceHolder *ph, SystemInfos::DiscSpace *ds, QObject *parent) : QObject(parent)
{
    MyConfiguration  = config;
    MyDiscSpace      = ds;
    MyMediaManager   = mm;
    MyInventoryTable = it;
    MyPlaceHolder    = ph;
    setDefaultValues();
}

HeadParser::~HeadParser()
{
    setDefaultValues();
}

void HeadParser::setDefaultValues()
{
    refresh                        = 0; // means no looking for new index
    title                          = "No Title";
    width                          = 1366;
    height                         = 768;
    backgroundColor                = "black";
    backgroundImage                = "none";
    backgroundRepeat               = "repeat";
    region_list.clear();
    default_region.id              = "screen";
    default_region.regionName      = "";
    default_region.top             = 0;
    default_region.left            = 0;
    default_region.width           = 1;
    default_region.height          = 1;
    default_region.z_index         = 0;
    default_region.backgroundColor = "transparent";
    default_region.backgroundImage = "none";
    default_region.backgroundRepeat= "repeat";
    region_list.append(default_region);            void process();
}

QString HeadParser::getTitle()
{
    return title;
}

QString HeadParser::getRootBackgroundColor()
{
    return backgroundColor;
}

QList<Region> *HeadParser::getLayout()
{
    return &region_list;
}

void HeadParser::parse(QDomElement head, SmilHead::TaskScheduler *MyTasks)
{
    setDefaultValues();
    if (head.hasChildNodes())
    {
        QDomNodeList childs = head.childNodes();
        QDomElement element;
        for (int i = 0; i < childs.length(); i++)
        {
            element = childs.item(i).toElement();
            if (element.tagName() == "meta")
                parseMeta(element);
            else if (element.tagName() == "metadata")
                parseMetaData(element, MyTasks);
            else if (element.tagName() == "layout")
                parseLayout(element);
            else if (element.tagName() == "state")
                parseState(element);
        }
    }
}

void HeadParser::setRootLayout(int w, int h)
{
    width           = w;
    height          = h;
    return;
}


void HeadParser::parseMeta(QDomElement element)
{
    if (element.hasAttribute("name") && element.attribute("name") == "title" && element.hasAttribute("content"))
        title = element.attribute("content");
    else if (element.hasAttribute("http-equiv") && element.attribute("http-equiv") == "Refresh" && element.hasAttribute("content"))
    {
        refresh = element.attribute("content").toInt();
        #ifdef QT_NO_DEBUG
            if (refresh < 61) // make sure that impaciently  user do not hammering CMS or Server
                refresh = 60;
        #endif
    }
}

void HeadParser::parseMetaData(QDomElement element, SmilHead::TaskScheduler *MyTasks)
{
    QDomNodeList node_list = element.elementsByTagName("subscription");

    // QObject's have not a copy constructor so subscriptions had to be stored in a subscription_list
    // otherwise we get only the last parsed subscription send and mem leaks
    SubScription *subscription  = new SubScription(this);

    // FixMe: Violation SRP Find a way to replace this with a GoF-Pattern, e.g. factory!
    // Remind Task scheduler
    for(int i = 0; i < node_list.size(); i++)
    {
        subscription->parse(node_list.at(i).toElement());
        if (subscription->getType() == "SystemReport")
        {
            MySystemReportManager.reset(new Reporting::SystemReportManager(MyConfiguration, MyDiscSpace));
            MySystemReportManager.data()->init(subscription->getAction(), subscription->getRefreshInterval());
        }
        else if (subscription->getType() == "InventoryReport" && MyInventoryTable != Q_NULLPTR)
        {
            MyInventoryReportManager.reset(new Reporting::InventoryReportManager(MyConfiguration, MyInventoryTable, MyDiscSpace));
            MyInventoryReportManager.data()->init(subscription->getAction(), subscription->getRefreshInterval());
        }
        else if (subscription->getType() == "PlaylogCollection")
        {
            MyPlayLogsManager.reset(new Reporting::PlayLogsManager(MyConfiguration, MyDiscSpace));
            MyPlayLogsManager.data()->init(subscription->getAction(), subscription->getRefreshInterval());
        }
        else if (subscription->getType() == "EventlogCollection")
        {
            MyEventLogsManager.reset(new Reporting::EventLogsManager(MyConfiguration, MyDiscSpace));
            MyEventLogsManager.data()->init(subscription->getAction(), subscription->getRefreshInterval());
        }
        else if (subscription->getType() == "TaskSchedule")
        {
            MyTasks->processFromUrl(QUrl(subscription->getAction()));
        }
    }
}

void HeadParser::parseLayout(QDomElement layout)
{
    // root-layout must be found first, cause regions based on it
    QDomNodeList nodelist = layout.elementsByTagName("root-layout");
    if (nodelist.length() == 1)
       parseRootLayout(nodelist.item(0).toElement());
    nodelist = layout.elementsByTagName("region");
    if (nodelist.length() > 0)
        parseRegions(nodelist);

    // wait for download backgroundimages
    if (has_backgroundimage)
    {
        timer_id = startTimer(1000);
        timer_counter = 0;
    }
    else
    {
        emit parsingCompleted();
    }
}

void HeadParser::parseState(QDomElement state)
{
    if (!state.hasChildNodes())
        return;

    QDomNodeList childs = state.childNodes();
    for (int i = 0; i < childs.length(); i++)
    {
        if (childs.item(i).toElement().tagName() == "data")
        {
            MyPlaceHolder->parsePlaceholder(childs.item(i).toElement());
        }
    }
}

void HeadParser::parseRootLayout(QDomElement root_layout)
{
    if (root_layout.hasAttribute("width"))
        width  = root_layout.attribute("width").toInt();
    if (root_layout.hasAttribute("height"))
        height = root_layout.attribute("height").toInt();
    if (root_layout.hasAttribute("backgroundColor"))
        backgroundColor = root_layout.attribute("backgroundColor");
    if (root_layout.hasAttribute("backgroundImage"))
        backgroundImage = root_layout.attribute("backgroundImage");
    if (root_layout.hasAttribute("backgroundRepeat"))
        backgroundRepeat = root_layout.attribute("backgroundRepeat");
}

void HeadParser::parseRegions(QDomNodeList childs)
{
    QDomElement element;
    region_list.clear(); // clear from default values
    for (int i = 0; i < childs.length(); i++)
    {
        Region region = default_region; // init with default values
        element = childs.item(i).toElement();
        if (element.tagName() != "region")
            continue;

        region.id = TBase::parseID(element);

        if (element.hasAttribute("regionName"))
            region.regionName      = element.attribute("regionName");
        if (element.hasAttribute("top"))
            region.top             = calculatePercentBasedOnRoot(element.attribute("top"), height);
        if (element.hasAttribute("left"))
            region.left            = calculatePercentBasedOnRoot(element.attribute("left"), width);
        if (element.hasAttribute("width"))
            region.width           = calculatePercentBasedOnRoot(element.attribute("width"), width);
        if (element.hasAttribute("height"))
            region.height          = calculatePercentBasedOnRoot(element.attribute("height"), height);
        if (element.hasAttribute("z-index"))
            region.z_index         = element.attribute("z-index").toInt();
        if (element.hasAttribute("fit"))
            region.fit         = element.attribute("fit");
        if (element.hasAttribute("mediaAlign"))
            region.mediaAlign         = element.attribute("mediaAlign");
        if (element.hasAttribute("soundLevel"))
            region.soundLevel         = element.attribute("soundLevel");
        if (element.hasAttribute("backgroundColor"))
            region.backgroundColor = element.attribute("backgroundColor");
        if (element.hasAttribute("backgroundImage"))
        {
            region.backgroundImage = element.attribute("backgroundImage");
            handleBackgroundImage(region.backgroundImage);
        }
        if (element.hasAttribute("backgroundRepeat"))
            region.backgroundRepeat = element.attribute("backgroundRepeat");
        region_list.append(region);
        std::sort(region_list.begin(), region_list.end()); // sort z-indexes ascending to place Widgets correct in mainwindow

    }
    qDebug() << childs.length() << "regions found in SMIL layout-tag and " << region_list.length() << " regions were initialized ";
}

void HeadParser::handleBackgroundImage(QString value)
{
    if (value.toLower() != "none" && value.toLower() != "inherited")
    {
        MyMediaManager->registerFile(value);
        has_backgroundimage = true;
    }
}



qreal HeadParser::calculatePercentBasedOnRoot(QString value, qreal root)
{
    int length  = value.length();
    int px      = 0;
    if (value[length-1] == QChar('%'))
        return qreal(value.mid(0, length-1).toFloat())/100; // to have a easy calculatable value for resize zones
    else if (value[length-1] == QChar('x'))
        px = value.mid(0, length-2).toInt();
    else
        px = value.toInt();
    qreal ret;
    if (root > 0)
      ret = (100*px) / root;
    else
      ret = 0;
    return ret/100; // to have a easy calculatable value for resize zones
}

/**
 * @brief HeadParser::timerEvent
 * checks if backgroudimages are loadable
 * @param event
 */
void HeadParser::timerEvent(QTimerEvent *event)
{

    timer_counter++;
    if (timer_counter > MAX_SECONDS_WAIT) // do not wait more than 10s
    {
        killTimer(event->timerId());
        emit parsingCompleted();
        return;
    }

    if (backgroundImage != "none" && !isMediaLoadable(backgroundImage))
    {
       return;
    }

    for (int i = 0; i < region_list.length(); i++)
    {
        if (region_list.at(i).backgroundImage != "none" && !isMediaLoadable(region_list.at(i).backgroundImage))
        {
            return;
        }
    }

    // if we reach this point it means we have all backgroundimages available
    killTimer(event->timerId());
    emit parsingCompleted();
}




bool HeadParser::isMediaLoadable(QString src)
{
    if (MyMediaManager->checkCacheStatus(src) == MEDIA_IS_LOCAL || MyMediaManager->checkCacheStatus(src) == MEDIA_CACHED)
    {
        return true;
    }
    return false;
}



