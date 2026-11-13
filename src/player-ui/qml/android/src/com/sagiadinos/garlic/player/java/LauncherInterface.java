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
package com.sagiadinos.garlic.player.java;

/**
 * What stands over the player on Android: the garlic-launcher 1.x or the firmware of a Philips display.
 * The C++ part reaches it through the static methods of GarlicActivity.
 */
public interface LauncherInterface
{
    /// "garlic" or "philips"
    String getLauncherName();

    /// Asks for uuid, content URL and version; the answers may come later (Philips: over the SICP socket).
    void fetchDeviceInformation();

    String getContentUrlFromLauncher();

    String getUUIDFromLauncher();

    String getLauncherVersion();

    void setScreenOff();

    void setScreenOn();

    void activateDeepStandBy(String seconds_to_wakeup);

    void rebootOS(String task_id);

    void installSoftware(String apk_path);

    /// The player ends on purpose: the launcher shall not start it again.
    void closedCorrectly();
}
