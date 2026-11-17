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
// CEF wants an NSApplication that implements CefAppProtocol (isHandlingSendEvent, setHandlingSendEvent:), which the
// application class of SDL (SDLApplication) does not. Added to it at run time before SDL_Init(), like ClientApplication
// of cefclient: sendEvent: marks that an event is being handled.
#include "include/cef_application_mac.h"

#include <objc/runtime.h>

#include <string>

namespace
{
    BOOL handling_send_event = NO;
    IMP  original_send_event = nullptr;

    BOOL isHandlingSendEvent(id /*self*/, SEL /*selector*/)
    {
        return handling_send_event;
    }

    void setHandlingSendEvent(id /*self*/, SEL /*selector*/, BOOL handling)
    {
        handling_send_event = handling;
    }

    void sendEvent(id self, SEL selector, NSEvent *event)
    {
        BOOL before         = handling_send_event;
        handling_send_event = YES;
        reinterpret_cast<void (*)(id, SEL, NSEvent *)>(original_send_event)(self, selector, event);
        handling_send_event = before;
    }
}

namespace player
{
    void prepareCefApplication()
    {
        Class application = objc_getClass("SDLApplication");
        if (application == nil || class_conformsToProtocol(application, @protocol(CefAppProtocol)))
            return;
        std::string get_types = std::string(@encode(BOOL)) + "@:";
        std::string set_types = std::string("v@:") + @encode(BOOL);
        class_addMethod(application, @selector(isHandlingSendEvent), reinterpret_cast<IMP>(&isHandlingSendEvent),
                        get_types.c_str());
        class_addMethod(application, @selector(setHandlingSendEvent:), reinterpret_cast<IMP>(&setHandlingSendEvent),
                        set_types.c_str());

        // SDLApplication overrides sendEvent:; if not, the override is added (NSApplication stays untouched)
        SEL    send    = @selector(sendEvent:);
        Method method  = class_getInstanceMethod(application, send);
        original_send_event = method_getImplementation(method);
        if (!class_addMethod(application, send, reinterpret_cast<IMP>(&sendEvent), method_getTypeEncoding(method)))
            method_setImplementation(method, reinterpret_cast<IMP>(&sendEvent));

        class_addProtocol(application, @protocol(CrAppProtocol));
        class_addProtocol(application, @protocol(CrAppControlProtocol));
        class_addProtocol(application, @protocol(CefAppProtocol));
    }
}
