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

#ifndef SMILPARSER_H
#define SMILPARSER_H

#include "body.h"
#include "current_playing_media.h"
#include "smilparser/element_factory.h"
#include "container.h"
#include "base_media.h"
#include "elements_container.h"

using namespace MediaParser;

/**
 * @brief   The TSmil class is the interface between container, media and
 *          the communication with the graphical output in mainwindow
 *          It is emits a signal which informs about media
 *          has to be played.
 *
 */
class BodyParser : public QObject
{
        Q_OBJECT
    public:
        explicit BodyParser(ElementFactory *ef, Files::MediaManager *mm, ElementsContainer *ec, QObject *parent = Q_NULLPTR);
        ~BodyParser();
        void                               beginPreloading(Expr *expr, TBase *smil, QDomElement body);
        void                               startPresentationAfterPreload();
        void                               endPlayingBody();
        void                               triggerAccessKey(QChar key);
        void                               triggerNotify(QString key);

    protected:
        QDomElement                        parser;
        QString                            index_path;
        QScopedPointer<TBody>              MyBody;
        Files::MediaManager               *MyMediaManager;
        ElementFactory                    *MyElementFactory;
        bool                               stop_all = false;
        ElementsContainer                 *MyElementsContainer;
        CurrentPlayingMedia               *MyCurrentPlayingMedia;
        void                               emitStartShowMedia(BaseMedia *media);
        void                               emitStopShowMedia(BaseMedia *media);
        void                               emitResumeShowMedia(BaseMedia *media);
        void                               emitPauseShowMedia(BaseMedia *media);
        void                               connectSlots(BaseTimings *element);
        bool                               determineContinueBasedOnParent(BaseTimings *element);
        void                               fireTrigger(QString trigger, BaseTimings *element, QString source_id);
    protected slots:
        void                               preloadElement(TContainer *ParentContainer, QDomElement dom_element);

        void                               registerMedia(BaseMedia *MyMedia);

        void                               startElement(BaseTimings *element);
        void                               stopElement(BaseTimings *element, bool is_forced);
        void                               resumeQueuedElement(BaseTimings *element);
        void                               pauseElement(BaseTimings*element);
        void                               prepareFireTrigger(QString trigger, QString target_id, QString source_id);
        void                               repeatMedia(BaseMedia *media);
    signals:
        void                               startShowMedia(BaseMedia *media);
        void                               stopShowMedia(BaseMedia *media);
        void                               pauseShowMedia(BaseMedia *media);
        void                               resumeShowMedia(BaseMedia *media);
};

#endif // SMILPARSER_H
