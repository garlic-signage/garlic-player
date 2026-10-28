#include "launcher.h"
#include <QDebug>

Launcher::Launcher(QObject *parent) : QObject(parent)
{
}

void Launcher::rebootOS(QString task_id)
{
    if (!has_launcher)
        return;

#if defined  Q_OS_ANDROID
    QAndroidJniObject java_task_id = QAndroidJniObject::fromString(task_id);
    QAndroidJniObject::callStaticMethod<void>("com/sagiadinos/garlic/player/java/GarlicActivity",
                                              "rebootOS",
                                              "(Ljava/lang/String;)V",
                                              java_task_id.object<jstring>()
                                               );
#endif
}

void Launcher::installSoftware(QString file_path)
{
    if (!has_launcher)
        return;
#if defined  Q_OS_ANDROID
   QAndroidJniObject java_file_path = QAndroidJniObject::fromString(file_path);
   QAndroidJniObject::callStaticMethod<void>("com/sagiadinos/garlic/player/java/GarlicActivity",
                                              "installSoftware",
                                              "(Ljava/lang/String;)V",
                                              java_file_path.object<jstring>());
#endif
}

void Launcher::toggleScreenActivity(bool is_on)
{
    if (!has_launcher)
        return;

#if defined  Q_OS_ANDROID
    if (is_on)
    {
        QAndroidJniObject::callStaticMethod<void>("com/sagiadinos/garlic/player/java/GarlicActivity", "setScreenOn");
    }
    else
    {
        QAndroidJniObject::callStaticMethod<void>("com/sagiadinos/garlic/player/java/GarlicActivity", "setScreenOff");
    }
#elif defined Q_OS_LINUX
    QProcess process;
    QString program = "xset";
    QStringList arguments;
    if (is_on)
    {
        arguments << "dpms " << "force" << "on";
    }
    else
    {
        arguments << "dpms " << "force" << "off";
    }
    process.start(program, arguments);
#elif defined Q_OS_OSX
#elif defined Q_OS_WIN32
#endif

//    is_screen_on = is_on;
}

void Launcher::activateDeepStandby(QString wakeup)
{

    qDebug() << wakeup;
    if (!has_launcher)
        return;

#if defined  Q_OS_ANDROID
    QAndroidJniObject seconds_to_wakeup = QAndroidJniObject::fromString(wakeup);
    QAndroidJniObject::callStaticMethod<void>("com/sagiadinos/garlic/player/java/GarlicActivity",
                                              "activateDeepStandBy",
                                              "(Ljava/lang/String;)V",
                                              seconds_to_wakeup.object<jstring>());

#elif defined Q_OS_LINUX
#elif defined Q_OS_OSX
#elif defined Q_OS_WIN32
#endif

}


void Launcher::sendClosePlayerCorrect()
{
    if (!has_launcher)
        return;

#if defined  Q_OS_ANDROID
    QAndroidJniObject::callStaticMethod<void>("com/sagiadinos/garlic/player/java/GarlicActivity", "closePlayerCorrect");
#endif
}

void Launcher::toogleLauncher(bool has)
{
    has_launcher = has;
}
