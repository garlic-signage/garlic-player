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
#include "web.h"
using namespace MediaParser;


#include "QtWebEngineWidgets/qwebengineview.h"

PlayerWeb::PlayerWeb(QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent) : PlayerBaseMedia(r_id, lc, config, parent)
{
    browser   = new QWebEngineView();
    port      = MyMainConfiguration->getUserConfig()->value("httpd/port").toString();
    connect(browser, SIGNAL(loadFinished(bool)), this, SLOT(doLoadFinished(bool)));
    page = browser->page();
    QObject::connect(page, &QWebEnginePage::featurePermissionRequested,
                     [&](const QUrl &securityOrigin, QWebEnginePage::Feature feature)
                     {
                         if (feature == QWebEnginePage::MediaAudioCapture ||
                             feature == QWebEnginePage::MediaVideoCapture ||
                             feature == QWebEnginePage::MediaAudioVideoCapture)
                         {
                             page->setFeaturePermission(securityOrigin, feature, QWebEnginePage::PermissionGrantedByUser);
                         }
                     });
}

PlayerWeb::~PlayerWeb()
{
    // could be obsolete 21 July 2022
    // FIX THIS! delete or deleteLater for browser crashes,
    // when playliste changed and browser was used method init/deinit
    // it crashes even as stack variable or QScopedPointer
    // actual solution works, but unsatisfied
    // Info: delete crashes cause this destructor invoked two times
    // deleteLater crashes someone else deep in assembler


    qDebug() << "delete browser";
    delete  browser;
}

void PlayerWeb::loadMedia(BaseMedia *media, Region *reg)
{
    SmilMedia = media;
    region    = reg;

    // Deactivate caching for testing
    // browser->page()->profile()->setHttpCacheType(QWebEngineProfile::NoCache);
    browser->page()->setBackgroundColor(Qt::transparent);

    browser->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false); // auto play video and audio
    browser->settings()->setAttribute(QWebEngineSettings::ShowScrollBars, false);
    browser->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true); // needed to block SOP Attention can be deprecated look at param functions in main.cpp
    browser->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true); // needed to deblock CORS for local files
    browser->settings()->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);
/*
    // activates everything for debug and testing
    browser->settings()->setAttribute(QWebEngineSettings::SpatialNavigationEnabled, true);
    browser->settings()->setAttribute(QWebEngineSettings::HyperlinkAuditingEnabled, true);
    browser->settings()->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
    browser->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    browser->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
    browser->settings()->setAttribute(QWebEngineSettings::TouchIconsEnabled, true);
    browser->settings()->setAttribute(QWebEngineSettings::FocusOnNavigationEnabled, true);
    browser->settings()->setAttribute(QWebEngineSettings::PrintElementBackgrounds, true);
    browser->settings()->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);
    browser->settings()->setAttribute(QWebEngineSettings::AllowGeolocationOnInsecureOrigins, true);
    browser->settings()->setAttribute(QWebEngineSettings::AllowWindowActivationFromJavaScript, true);
    browser->settings()->setAttribute(QWebEngineSettings::JavascriptCanPaste, true);
    browser->settings()->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, true);
*/

    QString uri = sanitizeUri(SmilMedia->getLoadablePath()) + SmilMedia->getParamsAsQuery();
    browser->load(uri);
}

void PlayerWeb::restart()
{
    stop();
    play();
}

void PlayerWeb::play()
{
    if (SmilMedia->getLogContentId() != "")
        setStartTime();
}

void PlayerWeb::stop()
{
    if (browser == Q_NULLPTR)
        return;

    browser->close();

    if (SmilMedia->getLogContentId() != "")
        qInfo(PlayLog).noquote() << createPlayLogXml();

}

void PlayerWeb::resume()
{
    browser->setVisible(true);
}

void PlayerWeb::pause()
{
    browser->setVisible(false);
    // todo add support for pauseDisplay
}

void PlayerWeb::changeSize(int w, int h)
{
    browser->resize(w-2, h-2);
}

QWidget *PlayerWeb::getView()
{
    return browser;
}

void PlayerWeb::doLoadFinished(bool has_succeeded)
{
    if (has_succeeded)
    {
        QString code = QStringLiteral("document.body.style.overflow = 'hidden';");
        browser->page()->runJavaScript(code);
        //apply zoom_factor
        browser->setZoomFactor(SmilMedia->getZoomFactor());
        //run extra js on page
        QVariantList jsList = SmilMedia->getExtraJSList();
        for (QVariantList::iterator j = jsList.begin(); j != jsList.end(); j++)
        {
            QString jsCode =(*j).toString();
            browser->page()->runJavaScript(jsCode+";");
        }
    }
}

QString PlayerWeb::sanitizeUri(QString uri)
{
    if (uri.isEmpty())
        return "";

    // add file scheme if neccessary
    if (uri.at(0) == "/")
    {
        uri = "file://" + uri;

        // We tried to open wgt via webserver to avoid open local pdf painment, but this causes other problems.
        // QString prefix_to_remove = MyMainConfiguration->getPaths("cache");
        // uri.remove(prefix_to_remove);
        // uri = "http://localhost:" + port + "/cache/" + uri;
    }
    return uri;
}
