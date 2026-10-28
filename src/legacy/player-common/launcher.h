#ifndef LAUCHERCOMMUNICATION_H
#define LAUCHERCOMMUNICATION_H

#include <QObject>
#if defined  Q_OS_ANDROID
#include <QtAndroidExtras>
#elif defined Q_OS_LINUX
    #include <QProcess>
#endif

class Launcher : public QObject
{
        Q_OBJECT
    public:
        explicit Launcher(QObject *parent = nullptr);

        void rebootOS(QString task_id);
        void installSoftware(QString file_path);
        void toggleScreenActivity(bool is_on);
        void activateDeepStandby(QString wakeup);
        void sendClosePlayerCorrect();
        void toogleLauncher(bool has);
    private:
        bool has_launcher = false;

};

#endif // LAUCHERCOMMUNICATION_H
