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

#ifndef TBASE_H
#define TBASE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtXml>
#include "tools/logger.h"

enum class  SMIL_TAG{seq, par, excl, img, audio, video, text, ref, prefetch, unknown};

/**
 * @brief The abstract TBase class should inherited for all smil elements in body section
 *        Here comes is the init of all common attributes:
 *        id/xml:id, title, class and xml:lang
 *
 */
class TBase : public QObject
{
    Q_OBJECT
public:
    explicit TBase(QObject *parent);
    virtual void          preloadParse(QDomElement element) = 0;

            QString       getID();
            QString       getTitle();
            QString       getLang();
            QString       getClass();
            QDomElement   getRootElement();
    static  QString       parseID(QDomElement element);
            TBase        *getParentContainer();
protected:
            TBase        *parent_container;
            SMIL_TAG      parent_tag;
            // core attributes for every Tag
            QString       version        = "3.0";
            QString       base_profile   = "Tiny";
            QString       id             = "";
            QString       title          = "";
            QString       alt            = "";
            QString       longdesc       = "";
            QString       lang           = "";
            QString       expr           = "";
            QString       a_class        = ""; // cause class is reserved word, a_class means attribut_class
            QDomElement   root_element;
            void          setBaseAttributes();
            QString       getAttributeFromRootElement(const QString attribute_name, const QString default_value);
};

#endif // TBASE_H
