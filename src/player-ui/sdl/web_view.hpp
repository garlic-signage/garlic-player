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
#ifndef PLAYER_UI_SDL_WEB_VIEW_HPP
#define PLAYER_UI_SDL_WEB_VIEW_HPP

#include "media.hpp"

#include <functional>
#include <memory>
#include <string>

namespace player
{
    /**
     * What shows a web page or widget, one per medium. Per platform: CEF off-screen on Linux (cef_web_view.cpp, draws
     * into a texture, so it lies in its region like an image); later the native views (WebView2 on Windows,
     * WKWebView on macOS and iOS, Android WebView), which lie over the window. Without one (web_view.cpp)
     * createWebView() returns nullptr and web pages fail, so the core goes on.
     */
    class WebView
    {
        public:
            virtual ~WebView() = default;

            /// url: http(s) or file:// with the query of a widget
            virtual void load(const std::string &url) = 0;
            virtual void stop()                       = 0;

            /// Draws the page into area (pixels of the renderer), sized to it
            virtual void draw(SDL_Renderer &renderer, const SDL_FRect &area) = 0;

            /// Mouse, wheel, key and text events; x and y of mouse events relative to the area
            virtual void input(const SDL_Event &event, float x, float y) = 0;
    };

    /// The web view of this platform; nullptr if this build has none or it cannot start.
    std::unique_ptr<WebView> createWebView(const MediaContext &context);

    // What the web engine needs from the program, the same calls for every platform (nothing without one):

    /// First thing in main(): a helper process of the engine runs here and returns its exit code; -1 for the player
    int runWebHelperProcess(int argc, char *argv[]);

    /// Before SDL_Init(): what the engine needs of the video driver (CEF: EGL also on X11)
    void prepareWebViews();

    /// Every turn of the main loop: the work the engine scheduled
    void pumpWebViews();

    /// While true, the main loop turns at least every 33 ms for pumpWebViews()
    bool webViewsRunning();

    /// ClearCaches with web: the cache of the engine goes; done() afterwards, in the main thread
    void clearWebCache(const std::function<void()> &done);

    /// Before the end of main(), after the last web view: closes the engine
    void shutdownWebViews();
}

#endif // PLAYER_UI_SDL_WEB_VIEW_HPP
