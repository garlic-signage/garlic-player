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
#include "web_view.hpp"

/// A build without a web engine (no CEF, and the native views of the other platforms are not done yet).
namespace player
{
    std::unique_ptr<WebView> createWebView(const MediaContext & /*context*/)
    {
        return nullptr;
    }

    int runWebHelperProcess(int /*argc*/, char * /*argv*/[])
    {
        return -1;
    }

    void pumpWebViews() {}

    bool webViewsRunning()
    {
        return false;
    }

    void clearWebCache(const std::function<void()> &done)
    {
        done();
    }

    void shutdownWebViews() {}
}
