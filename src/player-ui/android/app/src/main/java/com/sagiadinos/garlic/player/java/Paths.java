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

import android.content.Context;
import android.os.Environment;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.Arrays;

/**
 * Where the player keeps its files, the places of the qml player (and the legacy player), so an update keeps the ini
 * with the uuid, the cache and the logs:
 * - ini: QSettings (IniFormat, UserScope) of Qt on Android, files/.config/SmilControl/garlic-player.ini of the app
 *   (found so on a device with an earlier player)
 * - cache and logs: /sdcard/garlic-player with the permission for the shared storage (launcher 1.x installs updates
 *   from there), otherwise inside the app
 */
final class Paths
{
    final String ini;
    final String cache;
    final String logs;
    final boolean shared;

    Paths(Context context, boolean storage_permitted)
    {
        File files = context.getFilesDir();
        ini        = new File(files, ".config/SmilControl/garlic-player.ini").getAbsolutePath();
        File root  = storage_permitted ? new File(Environment.getExternalStorageDirectory(), "garlic-player") : files;
        cache      = new File(root, "cache").getAbsolutePath();
        logs       = new File(root, "logs").getAbsolutePath();
        shared     = storage_permitted;
        //noinspection ResultOfMethodCallIgnored
        new File(ini).getParentFile().mkdirs();
    }

    /**
     * The CA certificates of the system as one PEM file for libcurl: Android has no OpenSSL store. The updatable ones
     * of Android 10+ first, otherwise those of the system image (one file per certificate; OpenSSL skips the text
     * before BEGIN CERTIFICATE). Empty if none were found.
     */
    static String caBundle(Context context)
    {
        File bundle = new File(context.getFilesDir(), "ca-bundle.pem");
        for (String folder : new String[]{"/apex/com.android.conscrypt/cacerts", "/system/etc/security/cacerts"})
        {
            File[] files = new File(folder).listFiles();
            if (files == null || files.length == 0)
                continue;
            Arrays.sort(files);
            try (FileOutputStream out = new FileOutputStream(bundle))
            {
                byte[] buffer = new byte[16384];
                for (File file : files)
                {
                    try (InputStream in = new FileInputStream(file)) // java.nio.file only from API 26
                    {
                        for (int read; (read = in.read(buffer)) > 0;)
                            out.write(buffer, 0, read);
                    }
                    out.write('\n');
                }
                return bundle.getAbsolutePath();
            }
            catch (IOException error)
            {
                Log.w("garlic-player", "cannot write the CA bundle: " + error.getMessage());
            }
        }
        return "";
    }
}
