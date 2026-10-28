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
#include <doctest/doctest.h>

#include "configuration/ini_file.hpp"

using garlic::core::configuration::IniFile;

namespace
{
    // garlic-player.ini of a running garlic-lib player (Qt QSettings), token and ids replaced
    constexpr std::string_view qsettings_ini = R"([General]
api_access_token=0123456789ABCDEF0123456789ABCDEF
api_access_token_expire=2026-10-22T00:49:35
cleanupInterval=60000
firmware_update_task_id=000000000000000000000a
index_uri=https://garlic-hub.ddev.site/smil-index
last_played_index_path=/home/user/.cache/garlic-player/index.smil
maxMultiPartSize=10000000
maxRequestSize=16000
maxThreads=4
minThreads=1
player_name=Arch Linux Germany
port=8080
readTimeout=60000
shutdown_player_task_id=000000000000000000000b
standby_mode=no_standby
update_settings_task_id=000000000000000000000c
uuid=00000000-0000-4000-8000-000000000001

[audio]
volume=100%

[display]
brightness=100%

[httpd]
cacheSize=10485760
cacheTime=0
cleanupInterval=60000
encoding=UTF-8
maxAge=0
maxCachedFileSize=65536
maxMultiPartSize=114857600
maxRequestSize=65536
maxThreads=64
minThreads=8
path=/home/user/.cache/garlic-player/
port=8080
readTimeout=60000

[schedule]
reboot_days=Monday Tuesday Wednesday Thursday Friday Saturday Sunday
reboot_time=03:00
)";
}

TEST_CASE("reads a garlic-player.ini written by QSettings")
{
    auto ini = IniFile::parse(qsettings_ini);

    CHECK(ini.value("uuid") == "00000000-0000-4000-8000-000000000001");
    CHECK(ini.value("player_name") == "Arch Linux Germany");
    CHECK(ini.value("index_uri") == "https://garlic-hub.ddev.site/smil-index");
    CHECK(ini.value("audio/volume") == "100%");
    CHECK(ini.value("httpd/port") == "8080");
    CHECK(ini.value("httpd/path") == "/home/user/.cache/garlic-player/");
    CHECK(ini.value("schedule/reboot_days") == "Monday Tuesday Wednesday Thursday Friday Saturday Sunday");
    CHECK_FALSE(ini.value("missing").has_value());
}

TEST_CASE("writing gives exactly the file QSettings wrote")
{
    CHECK(IniFile::parse(qsettings_ini).serialize() == qsettings_ini);
}

TEST_CASE("new keys are sorted into their group")
{
    auto ini = IniFile::parse(qsettings_ini);
    ini.setValue("httpd/bind", "0.0.0.0");
    ini.setValue("time/server", "pool.ntp.org");
    ini.setValue("a_key", "x");

    auto text = ini.serialize();
    CHECK(text.starts_with("[General]\na_key=x\napi_access_token="));
    CHECK(text.find("[httpd]\nbind=0.0.0.0\ncacheSize=") != std::string::npos);
    CHECK(text.ends_with("[time]\nserver=pool.ntp.org\n"));
}

TEST_CASE("values with special characters are quoted and escaped like QSettings")
{
    IniFile ini;
    ini.setValue("comma", "a, b");
    ini.setValue("semicolon", "a;b");
    ini.setValue("equal", "a=b");
    ini.setValue("spaces", " padded ");
    ini.setValue("quote", "say \"hi\"");
    ini.setValue("backslash", "C:\\garlic\\cache");
    ini.setValue("newline", "line1\nline2");
    ini.setValue("at", "@home");
    ini.setValue("empty", "");
    ini.setValue("control", std::string("a\x01" "b"));

    auto text = ini.serialize();
    CHECK(text.find("comma=\"a, b\"\n") != std::string::npos);
    CHECK(text.find("semicolon=\"a;b\"\n") != std::string::npos);
    CHECK(text.find("equal=\"a=b\"\n") != std::string::npos);
    CHECK(text.find("spaces=\" padded \"\n") != std::string::npos);
    CHECK(text.find("quote=say \\\"hi\\\"\n") != std::string::npos);
    CHECK(text.find("backslash=C:\\\\garlic\\\\cache\n") != std::string::npos);
    CHECK(text.find("newline=line1\\nline2\n") != std::string::npos);
    CHECK(text.find("at=@@home\n") != std::string::npos);
    CHECK(text.find("empty=\n") != std::string::npos);
    CHECK(text.find("control=a\\x01\\x62\n") != std::string::npos); // b after \x01 is escaped too

    // and back
    auto back = IniFile::parse(text);
    for (const auto &[key, value] : ini.values())
    {
        CAPTURE(key);
        CHECK(back.value(key) == value);
    }
}

TEST_CASE("reads escapes QSettings writes: \\x for non-ASCII, %XX in keys, comments, CRLF")
{
    auto ini = IniFile::parse("[General]\r\n"
                              "; comment\r\n"
                              "player_name=Gr\\xf6\\xdf\\x65\r\n"
                              "my%20key=value ; trailing comment\r\n"
                              "raw_utf8=Größe\r\n"
                              "\r\n"
                              "[display]\r\n"
                              "sub\\level=deep\r\n");

    CHECK(ini.value("player_name") == "Größe");
    CHECK(ini.value("my key") == "value");
    CHECK(ini.value("raw_utf8") == "Größe");
    CHECK(ini.value("display/sub/level") == "deep");
}

TEST_CASE("keys with special characters and deeper groups are written like QSettings")
{
    IniFile ini;
    ini.setValue("my key", "1");
    ini.setValue("display/sub/level", "2");
    ini.setValue("General/x", "3"); // a real group called General

    auto text = ini.serialize();
    CHECK(text == "[General]\nmy%20key=1\n\n[%General]\nx=3\n\n[display]\nsub\\level=2\n");
    CHECK(IniFile::parse(text).values() == ini.values());
}

TEST_CASE("remove")
{
    auto ini = IniFile::parse(qsettings_ini);
    ini.remove("httpd/port");
    ini.remove("not/there");

    CHECK_FALSE(ini.value("httpd/port").has_value());
}
