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
#ifndef LOGGING_CATEGORIES_H
#define LOGGING_CATEGORIES_H

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(Screen)
Q_DECLARE_LOGGING_CATEGORY(Database)
Q_DECLARE_LOGGING_CATEGORY(SmilParser)
Q_DECLARE_LOGGING_CATEGORY(ContentManager)
Q_DECLARE_LOGGING_CATEGORY(MediaPlayer)
Q_DECLARE_LOGGING_CATEGORY(MediaControl)
Q_DECLARE_LOGGING_CATEGORY(System)
Q_DECLARE_LOGGING_CATEGORY(TimeService)

Q_DECLARE_LOGGING_CATEGORY(Develop)
Q_DECLARE_LOGGING_CATEGORY(PlayLog)
Q_DECLARE_LOGGING_CATEGORY(EventLog)
Q_DECLARE_LOGGING_CATEGORY(TaskExecution)

#endif // LOGGING_CATEGORIES_H
