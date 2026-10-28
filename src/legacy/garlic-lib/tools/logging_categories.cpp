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
#include "logging_categories.h"

// for eventlogs
Q_LOGGING_CATEGORY(Screen, "Screen")
Q_LOGGING_CATEGORY(Database, "Database")
Q_LOGGING_CATEGORY(SmilParser, "SmilParser")
Q_LOGGING_CATEGORY(ContentManager, "ContentManager")
Q_LOGGING_CATEGORY(MediaPlayer, "MediaPlayer")
Q_LOGGING_CATEGORY(MediaControl, "MediaControl")
Q_LOGGING_CATEGORY(System, "System")
Q_LOGGING_CATEGORY(TimeService, "TimeService")

// for ordinary Debugmessages
Q_LOGGING_CATEGORY(Develop, "Develop")

// for common play or event messsages
Q_LOGGING_CATEGORY(PlayLog, "PlayLog")
Q_LOGGING_CATEGORY(EventLog, "EventLog")

Q_LOGGING_CATEGORY(TaskExecution, "TaskExecution")
