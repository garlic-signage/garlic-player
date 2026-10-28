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
#include <QString>
#include <QtTest>

#include "smilparser/head_parser.h"

class TestHeadParser : public QObject
{
    Q_OBJECT

public:
    TestHeadParser(){}
private:
    QDomElement getTestSmilFile(QString file_string);

private Q_SLOTS:
    void test_getDefaultValues();
    void test_getHeadValuesFromSimpleSmil();
    void test_getHeadValuesFromComplexSmil();
};

void TestHeadParser::test_getDefaultValues()
{
    QSettings *Settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "SmilControl", "garlic-player-test");
    HeadParser MySmilHead(new MainConfiguration(Settings));
    QCOMPARE(MySmilHead.getRootBackgroundColor(), QString("black"));
    QCOMPARE(MySmilHead.getTitle(), QString("No Title"));
    QList<Region> *region_list = MySmilHead.getLayout();
    QCOMPARE(region_list->count(), int(1));
    QCOMPARE(region_list->at(0).regionName, QString("screen"));
    QCOMPARE(region_list->at(0).width, qreal(1));
    QCOMPARE(region_list->at(0).height, qreal(1));
    QCOMPARE(region_list->at(0).top, qreal(0));
    QCOMPARE(region_list->at(0).left, qreal(0));
    QCOMPARE(region_list->at(0).z_index, int(0));
    QCOMPARE(region_list->at(0).backgroundColor, QString("transparent"));
}

void TestHeadParser::test_getHeadValuesFromSimpleSmil()
{
    QDomElement head = getTestSmilFile(":/head_simple.smil");
    QSettings *Settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "SmilControl", "garlic-player-test");
    HeadParser MySmilHead(new MainConfiguration(Settings));
    MySmilHead.parse(head, new SmilHead::TaskScheduler(new MainConfiguration(Settings), this));
    QCOMPARE(MySmilHead.getTitle(), QString("Simple SMIL header for testing"));
    QCOMPARE(MySmilHead.getRootBackgroundColor(), QString("black"));
    QList<Region> *region_list = MySmilHead.getLayout();
    QCOMPARE(region_list->count(), int(1));

    QCOMPARE(region_list->at(0).regionName, QString("test_screen"));
    QCOMPARE(region_list->at(0).width, qreal(1));
    QCOMPARE(region_list->at(0).height, qreal(1));
    QCOMPARE(region_list->at(0).top, qreal(0));
    QCOMPARE(region_list->at(0).left, qreal(0));
    QCOMPARE(region_list->at(0).z_index, int(1));
    QCOMPARE(region_list->at(0).backgroundColor, QString("#000"));

}

void TestHeadParser::test_getHeadValuesFromComplexSmil()
{
    QDomElement head = getTestSmilFile(":/head_complex.smil");
    QSettings *Settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "SmilControl", "garlic-player-test");
    HeadParser MySmilHead(new MainConfiguration(Settings));
    MySmilHead.parse(head, new SmilHead::TaskScheduler(new MainConfiguration(Settings), this));

    QCOMPARE(MySmilHead.getRefreshTime(), int(2)); // works only in debug enviroment in release value should be 30
    QCOMPARE(MySmilHead.getTitle(), QString("Complex SMIL header for testing"));
    QList<Region> *region_list = MySmilHead.getLayout();
    QCOMPARE(region_list->count(), int(5));

    QCOMPARE(region_list->at(0).regionName, QString("screen1"));
    QCOMPARE(region_list->at(0).width, qreal(1));
    QCOMPARE(region_list->at(0).height, qreal(1));
    QCOMPARE(region_list->at(0).top, qreal(0));
    QCOMPARE(region_list->at(0).left, qreal(0));
    QCOMPARE(region_list->at(0).z_index, int(0));
    QCOMPARE(region_list->at(0).backgroundColor, QString("red"));

    QCOMPARE(region_list->at(1).regionName, QString("screen2"));
    QCOMPARE(region_list->at(1).width, qreal(0.25));
    QCOMPARE(region_list->at(1).height, qreal(0.25));
    QCOMPARE(region_list->at(1).top, qreal(0));
    QCOMPARE(region_list->at(1).left, qreal(0.50));
    QCOMPARE(region_list->at(1).z_index, int(1));
    QCOMPARE(region_list->at(1).backgroundColor, QString("green"));

    QCOMPARE(region_list->at(2).regionName, QString("screen3"));
    QCOMPARE(region_list->at(2).width, qreal(0.50));
    QCOMPARE(region_list->at(2).height, qreal(0.50));
    QCOMPARE(region_list->at(2).top, qreal(0.50));
    QCOMPARE(region_list->at(2).left, qreal(0));
    QCOMPARE(region_list->at(2).z_index, int(1));
    QCOMPARE(region_list->at(2).backgroundColor, QString("blue"));

    QCOMPARE(region_list->at(3).regionName, QString("screen4"));
    QCOMPARE(region_list->at(3).width, qreal(0.25));
    QCOMPARE(region_list->at(3).height, qreal(0.50));
    QCOMPARE(region_list->at(3).top, qreal(0.50));
    QCOMPARE(region_list->at(3).left, qreal(0.50));
    QCOMPARE(region_list->at(3).z_index, int(1));
    QCOMPARE(region_list->at(3).backgroundColor, QString("orange"));

    QCOMPARE(region_list->at(4).regionName, QString("screen5"));
    QCOMPARE(region_list->at(4).width, qreal(0.25));
    QCOMPARE(region_list->at(4).height, qreal(0.50));
    QCOMPARE(region_list->at(4).top, qreal(0.50));
    QCOMPARE(region_list->at(4).left, qreal(0.75));
    QCOMPARE(region_list->at(4).z_index, int(1));
    QCOMPARE(region_list->at(4).backgroundColor, QString("transparent"));
}

QDomElement TestHeadParser::getTestSmilFile(QString file_string)
{
    QFile file(file_string);
    QDomDocument document;
    document.setContent(&file);
    file.close();
    return document.firstChildElement().firstChildElement();
}

QTEST_MAIN(TestHeadParser)

#include "tst_head_parser.moc"
