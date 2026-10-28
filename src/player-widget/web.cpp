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
#include "web.hpp"

#include <QWebEnginePage>
#include <QWebEngineSettings>

namespace player
{
    Web::Web(const garlic::core::smil::Media &s, const garlic::core::smil::Region &r, QString src)
        : Media(s, r, std::move(src))
    {
        // the settings of garlic-lib: widgets load local files and remote data, play media without a click
        QWebEngineSettings *settings = browser.settings();
        settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
        settings->setAttribute(QWebEngineSettings::ShowScrollBars, false);
        settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
        settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
        settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);
        settings->setAttribute(QWebEngineSettings::SpatialNavigationEnabled, true);
        settings->setAttribute(QWebEngineSettings::HyperlinkAuditingEnabled, true);
        settings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
        settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
        settings->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
        settings->setAttribute(QWebEngineSettings::TouchIconsEnabled, true);
        settings->setAttribute(QWebEngineSettings::FocusOnNavigationEnabled, true);
        settings->setAttribute(QWebEngineSettings::PrintElementBackgrounds, true);
        settings->setAttribute(QWebEngineSettings::AllowGeolocationOnInsecureOrigins, true);
        settings->setAttribute(QWebEngineSettings::AllowWindowActivationFromJavaScript, true);
        settings->setAttribute(QWebEngineSettings::JavascriptCanPaste, true);
        settings->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, true);

        // camera and microphone for web pages that use them (garlic-lib)
        QWebEnginePage *page = browser.page();
        QObject::connect(page, &QWebEnginePage::permissionRequested, page,
                         [](QWebEnginePermission permission) { permission.grant(); });

        QObject::connect(&browser, &QWebEngineView::loadFinished, &browser,
                         [this](bool ok)
                         {
                             if (ok)
                                 browser.page()->runJavaScript("document.body.style.overflow = 'hidden';");
                         });
    }

    void Web::start()
    {
        browser.load(QUrl::fromUserInput(source, {}, QUrl::AssumeLocalFile));
    }

    void Web::stop()
    {
        browser.stop();
    }
}
