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

#include "base_media.h"

MediaParser::BaseMedia::BaseMedia(Files::MediaManager *mm, MainConfiguration *config, SmilHead::PlaceHolder *ph, Expr *expr, QObject *parent) : BaseTimings(expr, parent)
{
    MyMainConfiguration = config;
    MyMediaManager      = mm;
    MyPlaceHolder       = ph;

}

void MediaParser::BaseMedia::setRegion(Region r)
{
    MyRegion = r;
}

void MediaParser::BaseMedia::preloadParse(QDomElement element)
{
    root_element   = element;
    setAttributes();     // special for every media type
    parseBaseParameters(); // in this class
}


QString MediaParser::BaseMedia::getLoadablePath()
{
    // local media should be played always regardless of the cache mode
    if (MyMediaManager->checkCacheStatus(src) == MEDIA_IS_LOCAL)
    {
        return MyMediaManager->requestLoadablePath(src);
    }

    if (getCacheControl() == CACHE_CONTROL_ONLY_IF_CACHED)
    {
        if (MyMediaManager->checkCacheStatus(src) == MEDIA_CACHED)
        {
            return MyMediaManager->requestLoadablePath(src);
        }
        else
        {
            return ""; // only cachable media can be loaded
        }
    }
    else
    {
        if (MyMediaManager->checkCacheStatus(src) == MEDIA_UNCACHABLE)
        {
            return src;
        }
        else
        {
            return MyMediaManager->requestLoadablePath(src);
        }

    }
}

void MediaParser::BaseMedia::start()
{
    status      = _active;
    has_started = true;
}

void MediaParser::BaseMedia::repeat()
{
    prepareDurationTimersForRepeat() ;
}

void MediaParser::BaseMedia::stop(bool is_forced)
{
    Q_UNUSED(is_forced);
    selectWhichTimerShouldStop();
}

void MediaParser::BaseMedia::pause()
{
    status = _paused;
    pauseAllTimers();
}

void MediaParser::BaseMedia::resume()
{
    status = _active;
    resumeAllTimers();
}

void MediaParser::BaseMedia::interruptByRestart()
{
    status = _stopped;
    // check for repeat
}

QString MediaParser::BaseMedia::getRegionName()
{
    return region_name;
}

void MediaParser::BaseMedia::registerInMediaManager()
{
    if (MyPlaceHolder->isPlaceHolder(src))
        src = MyPlaceHolder->findPathByPlaceHolder(src);

    MyMediaManager->registerFile(src);
}

void MediaParser::BaseMedia::registerInMediaManagerAsUncachable()
{
    if (MyPlaceHolder->isPlaceHolder(src))
        src = MyPlaceHolder->findPathByPlaceHolder(src);

    MyMediaManager->registerAsUncachable(src);
}


QString MediaParser::BaseMedia::getParamsAsQuery() const
{
    QString ret;
    if (params_as_query.length() > 0)
        ret = "?" + params_as_query.join("&");
    return ret;
}


void MediaParser::BaseMedia::parseBaseMediaAttributes()
{
    parseTimingAttributes();

    region_name = getAttributeFromRootElement("region", "");
    fit    = getAttributeFromRootElement("fit", "");
    media_align    = getAttributeFromRootElement("mediaAlign", "");
    // maybe obsolete 2021-04-21
   //  type   = getAttributeFromRootElement("type");
}

void MediaParser::BaseMedia::emitfinishedElement() // called from finishedActiveDuration() BaseTimings
{
    emitStopElementSignal(this, false);
}

void MediaParser::BaseMedia::parseBaseParameters()
{
    if (!root_element.hasChildNodes())
     return;

    QDomNodeList childs = root_element.childNodes();
    for (int i = 0; i < childs.length(); i++)
    {
        setAdditionalParameters(childs.item(i).toElement());
    }
}

void MediaParser::BaseMedia::emitPause()
{
    emitPauseElementSignal(this);
}

void MediaParser::BaseMedia::emitResume()
{
    emitResumeElementSignal(this);
}

void MediaParser::BaseMedia::setAdditionalParameters(QDomElement param)
{
    if (param.tagName() != "param" || !param.hasAttribute("name") || !param.hasAttribute("value"))
        return;

    if (param.attribute("name") == "cacheControl")
    {
        cache_control = determineCacheControl(param.attribute("value"));
    }
    else if (param.attribute("name") == "filename")
    {
        filename = param.attribute("value");
    }
    else if (param.attribute("name") == "logContentId")
    {
        log_content_id = param.attribute("value");
    }
}

int MediaParser::BaseMedia::determineCacheControl(QString value)
{
    if (value.toUpper() == "ONLYIFCACHED")
    {
        return CACHE_CONTROL_ONLY_IF_CACHED;
    }
    return CACHE_CONTROL_USE_CACHE;
}

void MediaParser::BaseMedia::decodePageExtraSettings(QString pageExtraSettings){
    try
    {
        if(!pageExtraSettings.trimmed().isEmpty()){
            QByteArray text = QByteArray::fromBase64(pageExtraSettings.toUtf8());
            auto json_doc=QJsonDocument::fromJson(text);

            if(json_doc.isNull()){
                qDebug()<< src + " - Failed to create JSON doc.";
                return;

            }
            if(!json_doc.isObject()){
                qDebug()<< src+" - JSON is not an object.";
                return;
            }

            QJsonObject json_obj=json_doc.object();

            if(json_obj.isEmpty()){
                qDebug()<<src+" - JSON object is empty.";
                return;
            }

            QVariantMap root_map = json_obj.toVariantMap();
            extra_js = root_map["extra_js"].toList();
            zoom_factor = QString::number(json_obj.value("scale_factor").toDouble(), 'f', 2).toDouble();

          }
        } catch (...) {
            qDebug() << "extraParams decoding failed for "+src;
        }

}


