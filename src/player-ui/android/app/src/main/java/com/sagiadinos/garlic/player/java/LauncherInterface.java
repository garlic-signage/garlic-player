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
 * GarlicActivity finds it and calls it; without one it does nothing.
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

    /// The screen for hardwareInfo of the system report: model, product id, serial, vendor; null if the launcher does
    /// not know it (garlic-launcher)
    String[] getDisplayHardware();

    /// The device for modelInfo of the system report: model and firmware; null if the launcher does not know it
    /// (garlic-launcher: the core takes what Android tells)
    String[] getDeviceModel();

    /// Hours the screen has run, null if the launcher does not know them; waits for the answer: not in the UI thread
    Integer fetchOperatingHours();

    /// A CPU temperature of the screen in °C, only a fallback where the core reads none; null if the launcher does
    /// not know it; waits for the answer: not in the UI thread
    Integer fetchTemperatureCpuFallback();

    /// Throttling, the same: only a fallback where the core cannot tell it; null unknown; waits, not in the UI thread
    Boolean fetchThrottlingFallback();

    /// Asks for CPU temperature and throttling; the answers may come later (Philips: over the SICP socket).
    void fetchThermal();

    /// °C; null: the launcher does not tell it, the player shows what the core reads itself
    Integer getTemperatureCpu();

    /// null: the launcher does not tell it, the player shows what the core reads itself
    Boolean getThrottling();

    void setScreenOff();

    void setScreenOn();

    void activateDeepStandBy(String seconds_to_wakeup);

    /// The element that started the deep standby ended: the device is awake again
    void deepStandbyEnded();

    /// standby_mode deep at the start: the CMS alone switches the device, schedules of the device are cleared
    void takeOverSchedule();

    void rebootOS(String task_id);

    void installSoftware(String apk_path);

    /// The player ends on purpose: the launcher shall not start it again.
    void closedCorrectly();

    /// Brightness of the configuration, 0…100 (the master volume the player sets itself)
    void setBrightness(int percent);
}
