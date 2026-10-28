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
#include "wrapper_settings.hpp"

WrapperSettings::WrapperSettings()
{
    MySettings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "SmilControl", "garlic-player");
}

QString WrapperSettings::value(const QString &key) const
{
    return MySettings->value(key).toString();
}

void WrapperSettings::setValue(const QString &key, const QVariant &value)
{
    MySettings->setValue(key, value);
}

QSettings *WrapperSettings::getOriginal() const
{
    return MySettings;
}
