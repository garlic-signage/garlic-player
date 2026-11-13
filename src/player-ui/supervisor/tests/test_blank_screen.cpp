/*
 garlic-player: SMIL Player for Digital Signage
 Copyright (C) 2026 Nikolaos Sagiadinos <garlic@saghiadinos.de>
 This file is part of the GarlicSignage source code

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License, version 3,
 as published by the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "blank_screen.hpp"

#include <QStringList>
#include <QTest>

using namespace std::chrono_literals;

namespace
{
    struct FakeScreen final : player::ScreenControl
    {
        QStringList calls;
        void        screenOff() override { calls << "off"; }
        void        screenOn() override { calls << "on"; }
        void        deepStandby(std::chrono::seconds wake_up) override { calls << "deep " + QString::number(wake_up.count()); }
    };

    std::optional<std::chrono::seconds> thirty()
    {
        return 30s;
    }

    std::optional<std::chrono::seconds> none()
    {
        return std::nullopt;
    }
}

class TestBlankScreen final : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void partiallySwitchesOffUntilTheEnd()
        {
            FakeScreen          screen;
            player::BlankScreen blank;
            blank.setControl(&screen);
            blank.begin(1, "partially", thirty);
            blank.end(1);
            blank.end(1); // once only
            QCOMPARE(screen.calls, QStringList({"off", "on"}));
        }

        void deepSleepsUntilThePlannedEnd()
        {
            FakeScreen          screen;
            player::BlankScreen blank;
            blank.setControl(&screen);
            blank.begin(1, "deep", thirty);
            blank.end(1); // the supervisor wakes the device
            QCOMPARE(screen.calls, QStringList({"deep 30"}));
        }

        void deepWithoutPlannedEndSwitchesOff()
        {
            FakeScreen          screen;
            player::BlankScreen blank;
            blank.setControl(&screen);
            blank.begin(1, "deep", none);
            blank.end(1);
            QCOMPARE(screen.calls, QStringList({"off", "on"}));
        }

        void noStandbyAndUnknownModesDoNothing()
        {
            FakeScreen          screen;
            player::BlankScreen blank;
            blank.setControl(&screen);
            blank.begin(1, "no_standby", thirty);
            blank.begin(2, "", thirty);
            blank.end(1);
            blank.end(2);
            QVERIFY(screen.calls.isEmpty());
        }

        void withoutControlTheScreenStaysOn()
        {
            player::BlankScreen blank;
            blank.begin(1, "partially", thirty);
            blank.end(1);
        }
};

QObject *createTestBlankScreen()
{
    return new TestBlankScreen;
}

#include "test_blank_screen.moc"
