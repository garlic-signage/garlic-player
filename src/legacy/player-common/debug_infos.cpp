#include "debug_infos.h"
#include "ui_debug_infos.h"

DebugInfos::DebugInfos(LibFacade *lib_facade, QWidget *parent) :  QDialog(parent), ui(new Ui::DebugInfos)
{
    setLibFacade(lib_facade);
    ui->setupUi(this);
    timer_id = startTimer(1000);
    setWindowOpacity(0.9);
    MyResourceMonitor = MyLibFacade->getResourceMonitor();
#if !defined  Q_OS_ANDROID
    // cause in Android it shows fullscreen and not as dialog
    setWindowFlags(Qt::WindowStaysOnTopHint);
#endif
}


void DebugInfos::setLibFacade(LibFacade *lib_facade)
{
    MyLibFacade = lib_facade;
}


DebugInfos::~DebugInfos()
{
    delete ui;
    killTimer(timer_id);
}

void DebugInfos::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    outputResourcesUsage();
}

QString DebugInfos::preparePlayedMediaText(BaseMedia *media)
{
    if (media == Q_NULLPTR)
    {
        return "";
    }

    QString key = media->getRegionName();
    QString value = media->getTitle();

    if (value == "")
        value = media->getSrc();

    QMap<QString, QString>::iterator i = played_media.find(key);
    if (i != played_media.end())
        played_media[i.key()] = value;
    else
        played_media.insert(key, value);

    QString output = "";
    for (i = played_media.begin(); i != played_media.end(); i++)
    {
        output += i.key()+": " + i.value()+"\n";
    }
    return output;
}

void DebugInfos::outputResourcesUsage()
{
    MyResourceMonitor->refresh();

    ui->FreeSpace->setText(MyResourceMonitor->getTotalDiscSpace());
    ui->TotalSpace->setText(MyResourceMonitor->getFreeDiscSpace());

    ui->TotalSystemMemory->setText(MyResourceMonitor->getTotalMemorySystem());
    ui->FreeSystemMemory->setText(MyResourceMonitor->getFreeMemorySystem());

    ui->OutputMemoryUse->setText(MyResourceMonitor->getMemoryAppUse());
    ui->MaxMemoryUsed->setText(MyResourceMonitor->getMaxMemoryAppUsed());

    ui->ThreadsNumber->setText(MyResourceMonitor->getThreadsNumber());
    ui->MaxThreadsNumber->setText(MyResourceMonitor->getMaxThreadsNumber());

    QString title = "";
    if (MyLibFacade->getHead() != Q_NULLPTR)
    {
        title = MyLibFacade->getHead()->getTitle();
    }
    ui->PlaylistTitle->setText(title);
    ui->UserAgent->setText(MyLibFacade->getConfiguration()->getUserAgent());
}

void DebugInfos::on_close_dialog_clicked()
{
    QDialog::close();
}

void DebugInfos::on_close_player_clicked()
{
    QDialog::accept();
}
