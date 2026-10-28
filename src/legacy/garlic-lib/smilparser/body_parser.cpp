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

#include "body_parser.h"
#include "excl.h"

BodyParser::BodyParser(ElementFactory *ef, Files::MediaManager *mm, ElementsContainer *ec, QObject *parent) : QObject(parent)
{
    MyElementFactory      = ef;
    MyMediaManager        = mm;
    MyElementsContainer   = ec;
    MyCurrentPlayingMedia = new CurrentPlayingMedia(MyMediaManager, this);
    stop_all              = false;
}

BodyParser::~BodyParser()
{
}

/**
 * @brief TSmil::beginSmilParsing
 * Here starts the parsing withthe body Tag
 *
 * @param body
 */
void BodyParser::beginPreloading(Expr *expr, TBase *smil, QDomElement body)
{
    MyBody.reset(new TBody(expr, smil));
    connectSlots(MyBody.data());
    MyBody.data()->preloadParse(body);
    MyElementsContainer->distributeTriggers();
}

void BodyParser::preloadElement(TContainer *ParentContainer, QDomElement dom_element)
{
    BaseTimings *MyBaseTimings = MyElementFactory->createBase(dom_element, ParentContainer);
    connectSlots(MyBaseTimings);

    // Important! Slots needs to be connected before parsing!
    // Todo: integrate region with default values for fit, soundlevel, and mediaAlign
    MyBaseTimings->preloadParse(dom_element);

    // media must registered after parse
    if (MyBaseTimings->getBaseType() == "media")
    {
        registerMedia(qobject_cast<BaseMedia *> (MyBaseTimings));
    }
    ParentContainer->appendChild(MyBaseTimings);
    MyElementsContainer->insertSmilElement(MyBaseTimings);
    return;
}

void BodyParser::registerMedia(BaseMedia *MyMedia)
{
    QString type   = MyMedia->objectName();
    if (type == "TImage" || type == "TAudio"  || type == "TVideo"  || type == "TWidget")
    {
        MyMedia->registerInMediaManager();
    }
    else if (type == "TWeb" || type == "TRefCommand")
    {
        MyMedia->registerInMediaManagerAsUncachable();
    }
    // do nothing with other e.g. unknown
}

void BodyParser::startPresentationAfterPreload()
{
    MyBody.data()->activateTriggerTimers(); // the parse journey begin
}

/**
 * end playing
 */
void BodyParser::endPlayingBody()
{
    stop_all = true;

    BaseMedia *MyMedia;
    while (MyCurrentPlayingMedia->count() > 0) // stop all currently playing media
    {
        MyMedia = MyCurrentPlayingMedia->getFirstPlayingObject();
        MyMedia->stopAllTimers();
        emitStopShowMedia(MyMedia);
    }
    return;
}

/**
 * @brief TSmil::startedElement
 * @param element
 *
 * startElement fired by the prepareDurationTimers-methods
 * this means a first begin-trigger was fired already
 *
 * a media must be shown
 * a container collect his children and start their timer
 */
void BodyParser::startElement(BaseTimings *element)
{
    if (!determineContinueBasedOnParent(element)) // neccessary for excl
        return;

    element->start();
    qDebug() << "Start: " + element->getID();
    if (element->getBaseType() == "media")
        emitStartShowMedia(qobject_cast<BaseMedia *> (element));
}

/**
 * @brief TSmil::finishedElement slot get called when emit finishedMedia | finishedContainer
 *        => Active Duration
 * @param parent
 * @param element
 */
void BodyParser::stopElement(BaseTimings *element, bool is_forced)
{
    element->stop(is_forced);  //
    qDebug() << "Stop: " + element->getID();

    if (element->getBaseType() == "media")
        emitStopShowMedia(qobject_cast<BaseMedia *> (element));

    TContainer *ParentContainer = qobject_cast<TContainer *> (element->getParentContainer());
    if (ParentContainer == Q_NULLPTR)
        return;

    // prevent that an priority stopped child starts next child or end of simple/active duration
    if (!is_forced)
         ParentContainer->next(element);
}

void BodyParser::pauseElement(BaseTimings *element)
{
    qDebug() <<  "Pause " + element->getID();
    element->pause();

    if (element->getBaseType() == "media")
        emitPauseShowMedia(qobject_cast<BaseMedia *> (element));
}

void BodyParser::resumeQueuedElement(BaseTimings *element)
{
    if (element == Q_NULLPTR)
        return;

    qDebug() << "Resume: " + element->getID();
    element->resume();
    if (element->getBaseType() == "media")
        emitResumeShowMedia(qobject_cast<BaseMedia *> (element));
}

void BodyParser::triggerAccessKey(QChar key)
{
    BaseTimings *element = MyElementsContainer->findAccessKeysForBegin(key);

    if (element != Q_NULLPTR)
    {
        fireTrigger("begin", element, "accesskey");
        return;
    }

    element = MyElementsContainer->findAccessKeysForEnd(key);
    if (element != Q_NULLPTR)
        fireTrigger("end", element, "accesskey");
}

void BodyParser::triggerNotify(QString key)
{
    BaseTimings *element = MyElementsContainer->findNotifyForBegin(key);

    if (element != Q_NULLPTR)
    {
        fireTrigger("begin", element, "notify");
        return;
    }

    element = MyElementsContainer->findNotifyForEnd(key);
    if (element != Q_NULLPTR)
        fireTrigger("end", element, "notify");
}

void BodyParser::prepareFireTrigger(QString trigger, QString target_id, QString source_id)
{
    BaseTimings  *element = MyElementsContainer->findSmilElementById(target_id);
    fireTrigger(trigger, element, source_id);
}

void BodyParser::repeatMedia(BaseMedia *media)
{
    emit stopShowMedia(media);
    emit startShowMedia(media);
}

void BodyParser::fireTrigger(QString trigger, BaseTimings *element, QString source_id)
{
    if (qobject_cast<TContainer *> (element->getParentContainer())->isActive() != true) // only active elements!
        return;

    if (trigger == "begin")
    {
     //   if (!determineContinueBasedOnParent(element))
     //       return;

        element->startTrigger(source_id);
    }
    else
        element->stopTrigger(source_id);
}

void BodyParser::connectSlots(BaseTimings *element)
{
    connect(element, SIGNAL(startElementSignal(BaseTimings*)), this, SLOT(startElement(BaseTimings*)));
    connect(element, SIGNAL(stopElementSignal(BaseTimings*,bool)), this, SLOT(stopElement(BaseTimings*,bool)));
    connect(element, SIGNAL(resumeElementSignal(BaseTimings*)), this, SLOT(resumeQueuedElement(BaseTimings*)));
    connect(element, SIGNAL(pauseElementSignal(BaseTimings*)), this, SLOT(pauseElement(BaseTimings*)));

    connect(element, SIGNAL(triggerSignal(QString,QString,QString)), this, SLOT(prepareFireTrigger(QString,QString,QString)));

    if (element->getBaseType() == "container")
    {
        connect(element, SIGNAL(preloadElementSignal(TContainer*,QDomElement)), this, SLOT(preloadElement(TContainer*,QDomElement)));
    }
    else
    {
        connect(element, SIGNAL(repeat(BaseMedia*)), this, SLOT(repeatMedia(BaseMedia*)));
    }
}

bool BodyParser::determineContinueBasedOnParent(BaseTimings *element)
{
    // When there is excl as parent container we need to stops or pause lower and activate higher priorityClass.

    TContainer *ParentContainer = qobject_cast<TContainer *> (element->getParentContainer());
    if (ParentContainer != Q_NULLPTR && ParentContainer->objectName() == "TExcl")
    {
        TExcl   *MyExclParent   = qobject_cast<TExcl *> (ParentContainer);
        // check if current active has lower priority and then stop or pause it.
        return MyExclParent->determineContinue(element);
    }

    return true; // always true, when not excl parent
}

void BodyParser::emitStartShowMedia(BaseMedia *media)
{
    if (stop_all) // preventing for starting media during stop process
        return;
    MyCurrentPlayingMedia->insert(media);
    emit startShowMedia(media);
}

void BodyParser::emitStopShowMedia(BaseMedia *media)
{
   if (MyCurrentPlayingMedia->remove(media)) // not sending stops if skipElement
        emit stopShowMedia(media);
}

void BodyParser::emitResumeShowMedia(BaseMedia *media)
{
    if (stop_all) // preventing for starting media during stop process
        return;
    emit resumeShowMedia(media);
}

void BodyParser::emitPauseShowMedia(BaseMedia *media)
{
     emit pauseShowMedia(media);
}
