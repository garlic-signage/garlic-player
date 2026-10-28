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
#include "tst_main_configuration.hpp"
#include "tools/main_configuration.hpp"

using ::testing::Exactly;
using ::testing::Return;
using ::testing::AnyNumber;

MATCHER_P(IsQVariantOfType, type, "")
{
    return arg.type() == type;
}

void TestMainConfiguration::test_init()
{
    EXPECT_CALL(mockSettings, value(QString("uuid"))).Times(Exactly(1)).WillOnce(Return("the-uuid"));
    EXPECT_CALL(mockSettings, value(QString("player_name"))).Times(Exactly(1)).WillOnce(Return("playername"));

    MainConfiguration *MyConfig = new MainConfiguration(&mockSettings);
    MyConfig->init();

    QCOMPARE(MyConfig->getUuid(), "the-uuid");
    QCOMPARE(MyConfig->getPlayerName(), "playername");
}


void TestMainConfiguration::test_determineUserAgent()
{
    EXPECT_CALL(mockSettings, value(QString("uuid"))).Times(Exactly(1)).WillOnce(Return("the-uuid-is-this"));
    EXPECT_CALL(mockSettings, value(QString("player_name"))).Times(Exactly(1)).WillOnce(Return("the playername"));

    MainConfiguration *MyConfig = new MainConfiguration(&mockSettings);
    MyConfig->init();

    MyConfig->determineUserAgent();

#if defined Q_OS_LINUX
    QVERIFY(MyConfig->getUserAgent().contains("GAPI/1.0 (UUID:the-uuid-is-this; NAME:the playername) garlic-linux"));
#elif defined Q_OS_OSX
    QVERIFY(MyConfig->getUserAgent().contains("GAPI/1.0 (UUID:the-uuid-is-this; NAME:the playername) garlic-macOS"));
#elif defined Q_OS_WIN32
    QVERIFY(MyConfig->getUserAgent().contains("GAPI/1.0 (UUID:the-uuid-is-this; NAME:the playername) garlic-windows"));
#else
    QVERIFY(MyConfig->getUserAgent().contains("GAPI/1.0 (UUID:the-uuid-is-this; NAME:the playername) garlic-unknown"));
#endif
}

void TestMainConfiguration::test_determineBasePath()
{
    MainConfiguration *MyConfig = new MainConfiguration(&mockSettings);
    MyConfig->determineBasePath("/absolute/path/to/bin");
    QCOMPARE(MyConfig->getBasePath(), "/absolute/path/to/");

    MyConfig->determineBasePath("/absolute/path/");
    QCOMPARE(MyConfig->getBasePath(), "/absolute/path/");

    MyConfig->determineBasePath("/absolute/path");
    QCOMPARE(MyConfig->getBasePath(), "/absolute/path/");
}

void TestMainConfiguration::test_validateContentUrl()
{
    MainConfiguration *MyConfig = new MainConfiguration(&mockSettings);

    QVERIFY(MyConfig->validateContentUrl("") == false);
    QCOMPARE(MyConfig->getErrorText(), "A content-url is neccessary\n");

    QVERIFY(MyConfig->validateContentUrl("hurz.com") == false);
    QCOMPARE(MyConfig->getErrorText(), "Relative path to SMIL-Index is not allowed! Use /path/to/index, http://domain.tld, https://domain.tld, or file://path/to/index\n");

    QVERIFY(MyConfig->validateContentUrl("relative/path/to") == false);
    QCOMPARE(MyConfig->getErrorText(), "Relative path to SMIL-Index is not allowed! Use /path/to/index, http://domain.tld, https://domain.tld, or file://path/to/index\n");

    QVERIFY(MyConfig->validateContentUrl("https:/hurz.com") == false);
    QCOMPARE(MyConfig->getErrorText(), "Url Scheme is no valid! Use https://domain.tld\n");

    // positives
    QVERIFY(MyConfig->validateContentUrl("/path/to/hurz"));
    QCOMPARE(MyConfig->getValidatedContentUrl(), "/path/to/hurz");

    QVERIFY(MyConfig->validateContentUrl("file://path/to/hurz"));
    QCOMPARE(MyConfig->getValidatedContentUrl(), "//path/to/hurz");

    QVERIFY(MyConfig->validateContentUrl("HtTps://hurz.com"));
    QCOMPARE(MyConfig->getValidatedContentUrl(), "https://hurz.com");

    QVERIFY(MyConfig->validateContentUrl("heidewitzka://hurz.com"));
    QCOMPARE(MyConfig->getValidatedContentUrl(), "heidewitzka://hurz.com");
}


void TestMainConfiguration::test_determineIndexUriWithParameter()
{

    EXPECT_CALL(mockSettings, setValue(QString("index_uri"), IsQVariantOfType(QVariant::String))).Times(AnyNumber());

    MainConfiguration *MyConfig = new MainConfiguration(&mockSettings);

    MyConfig->determineIndexUri("http://domain.tld/path_to/index.smil");
    QCOMPARE(MyConfig->getIndexUri(), QString("http://domain.tld/path_to/index.smil"));
    QCOMPARE(MyConfig->getIndexPath(), QString("http://domain.tld/path_to/"));

    MyConfig->determineIndexUri("ftp://ftpdomain.tld/path_to/index.smil");
    QCOMPARE(MyConfig->getIndexUri(), QString("ftp://ftpdomain.tld/path_to/index.smil"));
    QCOMPARE(MyConfig->getIndexPath(), QString("ftp://ftpdomain.tld/path_to/"));

    MyConfig->setBasePath("/BasePath/");
    MyConfig->determineIndexUri("/path_to/index.smil");
    QCOMPARE(MyConfig->getIndexUri(), QString("/path_to/index.smil"));
    QCOMPARE(MyConfig->getIndexPath(), QString("/path_to/"));
}

void TestMainConfiguration::test_determineIndexUriNoParam()
{
    EXPECT_CALL(mockSettings, value(QString("index_uri"))).Times(Exactly(2)).WillRepeatedly(Return("https://smil-index-via-ini-file.tld/a_path/second/path/index.smil"));

    MainConfiguration *MyConfig = new MainConfiguration(&mockSettings);

    MyConfig->determineIndexUri("");
    QCOMPARE(MyConfig->getIndexUri(), QString("https://smil-index-via-ini-file.tld/a_path/second/path/index.smil"));
    QCOMPARE(MyConfig->getIndexPath(), QString("https://smil-index-via-ini-file.tld/a_path/second/path/"));

}

void TestMainConfiguration::test_determineIndexUriEmpty()
{
    EXPECT_CALL(mockSettings, value(QString("index_uri"))).Times(Exactly(1)).WillOnce(Return(""));

    MainConfiguration *MyConfig = new MainConfiguration(&mockSettings);

    MyConfig->determineIndexUri("");
    QCOMPARE(MyConfig->getIndexUri(), QString(""));
    QCOMPARE(MyConfig->getIndexPath(), QString("/"));

}

static TestMainConfiguration TEST_MAINCONFIGURATION;
