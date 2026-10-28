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
#ifndef INDEX_MODEL_H
#define INDEX_MODEL_H

#include <QObject>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QFileInfo>
#include <QDateTime>
#include "tools/logger.h"

class IndexModel : public QObject
{
        Q_OBJECT
    public:
        explicit IndexModel(QObject *parent=Q_NULLPTR);
        QDomElement     getHead();
        QDomElement     getBody();
        QDomElement     getSmil();
        bool            loadDocument(QString file_path);
        QDomDocument getDocument() const;
        void setDocument(const QDomDocument &value);
    signals:

    protected:
        QDomDocument    document;
        QDomElement     getTag(QString tag_name);
};

#endif // INDEX_MODEL_H
