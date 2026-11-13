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
#include <QCoreApplication>
#include <QTest>

#include <memory>

QObject *createTestSupervisorClient();
QObject *createTestBlankScreen();

/// Qt Test runs one class per qExec; all classes of this executable, every failure counts.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    for (auto create : {createTestBlankScreen, createTestSupervisorClient})
    {
        std::unique_ptr<QObject> test(create());
        failures += QTest::qExec(test.get(), argc, argv);
    }
    return failures == 0 ? 0 : 1;
}
