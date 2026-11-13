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
#include "android_launcher.hpp"

#include <QDebug>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QJniEnvironment>
#include <QJniObject>
#include <QStandardPaths>
#include <QThread>

#include <atomic>
#include <jni.h>

namespace player::android
{
    namespace
    {
        constexpr auto activity = "com/sagiadinos/garlic/player/java/GarlicActivity";

        std::atomic<Launcher *> current{nullptr}; // for the JNI functions of the receivers

        QString callString(const char *method)
        {
            return QJniObject::callStaticMethod<jstring>(activity, method).toString();
        }

        void callVoid(const char *method)
        {
            QJniObject::callStaticMethod<void>(activity, method);
        }

        void callWithString(const char *method, const QString &argument)
        {
            QJniObject text = QJniObject::fromString(argument);
            QJniObject::callStaticMethod<void>(activity, method, "(Ljava/lang/String;)V", text.object<jstring>());
        }
    }

    std::unique_ptr<Launcher> Launcher::detect()
    {
        QString name = callString("launcherName");
        if (name.isEmpty())
            return nullptr;

        std::unique_ptr<Launcher> launcher(new Launcher);
        launcher->launcher_name = name;
        // garlic-launcher answers at once; Philips over the SICP socket, which may not be connected yet: ask again.
        // The legacy player waited up to 5 s, too.
        QDeadlineTimer deadline(5000);
        do
        {
            callVoid("fetchDeviceInformation");
            launcher->launcher_uuid = callString("launcherUuid");
            if (!launcher->launcher_uuid.isEmpty())
                break;
            QThread::msleep(250);
        } while (!deadline.hasExpired());
        launcher->content_url      = callString("launcherContentUrl");
        launcher->launcher_version = callString("launcherVersion");
        qInfo().noquote() << "launcher:" << name << launcher->launcher_version << "uuid:" << launcher->launcher_uuid;

        current = launcher.get();
        return launcher;
    }

    Launcher::~Launcher()
    {
        Launcher *self = this;
        current.compare_exchange_strong(self, nullptr);
    }

    void Launcher::reboot(const std::string &task_id)
    {
        qInfo().noquote() << "launcher: reboot" << QString::fromStdString(task_id);
        callWithString("reboot", QString::fromStdString(task_id));
    }

    void Launcher::installSoftware(const std::string &path)
    {
        qInfo().noquote() << "launcher: install" << QString::fromStdString(path);
        callWithString("installSoftware", QString::fromStdString(path));
    }

    void Launcher::screenOff()
    {
        callVoid("screenOff");
    }

    void Launcher::screenOn()
    {
        callVoid("screenOn");
    }

    /// launcher 1.x takes the seconds as string
    void Launcher::deepStandby(std::chrono::seconds wake_up)
    {
        callWithString("deepStandby", QString::number(wake_up.count()));
    }

    void Launcher::closedCorrectly()
    {
        callVoid("closedCorrectly");
    }

    void Launcher::received(bool configuration, const QString &path)
    {
        Launcher *launcher = current;
        if (launcher == nullptr) // a broadcast without launcher, e.g. from adb: as before, the player takes it
        {
            qWarning().noquote() << "broadcast without launcher:" << path;
            return;
        }
        QMetaObject::invokeMethod(launcher,
                                  [launcher, configuration, path]
                                  {
                                      if (configuration)
                                          Q_EMIT launcher->configurationReceived(path);
                                      else
                                          Q_EMIT launcher->indexReceived(path);
                                  },
                                  Qt::QueuedConnection);
    }

    Storage storage()
    {
        // GarlicActivity asks for the permission when it starts; the dialog may still be open
        QDeadlineTimer deadline(20000);
        bool           permitted = false;
        while (!(permitted = QJniObject::callStaticMethod<jboolean>(activity, "storagePermitted")) && !deadline.hasExpired())
            QThread::msleep(250);

        Storage result;
        if (permitted)
        {
            QString root      = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/garlic-player";
            result.cache_dir  = root + "/cache";
            result.log_dir    = root + "/logs";
            result.shared     = true;
            return result;
        }
        qWarning() << "no permission for the shared storage: cache and logs inside the app, the launcher cannot "
                      "install updates from there";
        QString root     = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        result.cache_dir = root + "/cache";
        result.log_dir   = root + "/logs";
        return result;
    }

    std::string caBundle(const QString &dir)
    {
        QStringList folders{"/apex/com.android.conscrypt/cacerts", "/system/etc/security/cacerts"};
        for (const QString &folder : folders)
        {
            QDir certificates(folder);
            QStringList files = certificates.entryList(QDir::Files, QDir::Name);
            if (files.isEmpty())
                continue;
            QByteArray pem;
            for (const QString &name : files)
            {
                QFile file(certificates.filePath(name));
                if (file.open(QIODevice::ReadOnly))
                    pem += file.readAll() + '\n'; // the text before BEGIN CERTIFICATE is skipped by OpenSSL
            }
            QDir().mkpath(dir);
            QFile bundle(dir + "/ca-bundle.pem");
            if (!bundle.open(QIODevice::WriteOnly | QIODevice::Truncate) || bundle.write(pem) != pem.size())
            {
                qWarning().noquote() << "CA bundle not written:" << bundle.fileName();
                return {};
            }
            return bundle.fileName().toStdString();
        }
        qWarning() << "no CA certificates of the system found, HTTPS will fail";
        return {};
    }
}

// ------------------------------------------------------------------ JNI: the native methods of the receivers

namespace
{
    QString toQString(JNIEnv *env, jstring text)
    {
        if (text == nullptr)
            return {};
        const char *chars = env->GetStringUTFChars(text, nullptr);
        QString     value = QString::fromUtf8(chars);
        env->ReleaseStringUTFChars(text, chars);
        return value;
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_sagiadinos_garlic_player_java_ConfigReceiver_onConfigPath(JNIEnv *env, jclass,
                                                                                                    jstring path)
{
    player::android::Launcher::received(true, toQString(env, path));
}

extern "C" JNIEXPORT void JNICALL Java_com_sagiadinos_garlic_player_java_SmilIndexReceiver_onSmilIndexPath(JNIEnv *env, jclass,
                                                                                                          jstring path)
{
    player::android::Launcher::received(false, toQString(env, path));
}
