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
#include "ref_command.h"

MediaParser::TRefCommand::TRefCommand(Files::MediaManager *mm, MainConfiguration *config, SmilHead::PlaceHolder *ph, Expr *expr, QObject *parent)  : BaseMedia(mm, config, ph, expr, parent)
{
    setObjectName("TRefCommand");
    is_media = true;
}

MediaParser::TRefCommand::~TRefCommand()
{
}

void MediaParser::TRefCommand::prepareDurationTimers()
{
    if (!MyExpr->executeQuery(expr) || getLoadablePath().isEmpty())
    {
        skipElement();
        return;
    }

    if (status == _active && !isRestartable())
        return;

    if (startDurTimer() || isEndTimerActive())
    {
        resetInternalRepeatCount();
        emitStartElementSignal(this);
    }
    else
    {
        skipElement();
    }
}

// ====================  protected methods =================================

void MediaParser::TRefCommand::setAttributes()
{
    parseBaseMediaAttributes();
    src        = getAttributeFromRootElement("src", "");
}

void MediaParser::TRefCommand::prepareDurationTimersForRepeat()
{
    startDurTimer();
}
