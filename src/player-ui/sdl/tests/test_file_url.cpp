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
#include "file_url.hpp"

#include <doctest/doctest.h>

using player::fileUrl;

TEST_CASE("fileUrl: a URL with a scheme stays as it is")
{
    CHECK(fileUrl("http://example.com/index.html?a=1#top") == "http://example.com/index.html?a=1#top");
    CHECK(fileUrl("https://example.com/") == "https://example.com/");
    CHECK(fileUrl("file:///var/cache/a.html") == "file:///var/cache/a.html");
    CHECK(fileUrl("about:blank") == "about:blank");
    CHECK(fileUrl("data:text/html,<p>x</p>") == "data:text/html,<p>x</p>");
}

TEST_CASE("fileUrl: a local path becomes a file:// URL, percent-encoded")
{
    CHECK(fileUrl("/var/cache/index.html") == "file:///var/cache/index.html");
    CHECK(fileUrl("/var/cache/a-b_c.d~e.html") == "file:///var/cache/a-b_c.d~e.html");
    CHECK(fileUrl("/var/my cache/ä.html") == "file:///var/my%20cache/%C3%A4.html");
    CHECK(fileUrl("/var/cache/a&b=c.html") == "file:///var/cache/a%26b%3Dc.html");
}

TEST_CASE("fileUrl: a Windows path with a drive letter is a path, not a scheme")
{
    CHECK(fileUrl("C:/cache/index.html").starts_with("file://"));
}

TEST_CASE("fileUrl: a query stays unencoded")
{
    CHECK(fileUrl("/var/cache/index.html?a=1&b=2") == "file:///var/cache/index.html?a=1&b=2");
}

TEST_CASE("fileUrl: a fragment stays unencoded (PDF viewer)")
{
    CHECK(fileUrl("/var/cache/a.pdf#toolbar=0&navpanes=0&view=Fit") ==
          "file:///var/cache/a.pdf#toolbar=0&navpanes=0&view=Fit");
    CHECK(fileUrl("/var/my cache/a.pdf#page=2") == "file:///var/my%20cache/a.pdf#page=2");
}

TEST_CASE("fileUrl: query and fragment together, split at the first of them")
{
    CHECK(fileUrl("/var/cache/index.html?a=1#top") == "file:///var/cache/index.html?a=1#top");
}
