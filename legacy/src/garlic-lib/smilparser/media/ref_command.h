/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2021 Nikolaos Sagiadinos <ns@smil-control.com>
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
#ifndef TREFCOMMAND_H
#define TREFCOMMAND_H

#include "base_media.h"

// Todo inherit from TWeb, cause that is the same functionality

namespace MediaParser
{
    class TRefCommand : public BaseMedia
    {
        Q_OBJECT
    public:
        explicit TRefCommand(Files::MediaManager *mm, MainConfiguration *config, SmilHead::PlaceHolder *ph, Expr *expr, QObject *parent);
        ~TRefCommand();
    public slots:
        void     prepareDurationTimers();
    protected:
        void     setAttributes();
        void     prepareDurationTimersForRepeat();

    };
    }
#endif // TREFCOMMAND_H
