#include "play_logs.h"

Reporting::CreatePlayLogs::CreatePlayLogs(MainConfiguration *config, QObject *parent) : Reporting::CreateBase(config, parent)
{

}


void Reporting::CreatePlayLogs::process(QString file_name)
{
    init();
    mergeXml(MyConfiguration->getPaths("logs") + file_name);
}

void Reporting::CreatePlayLogs::mergeXml(QString file_name)
{
    QFile file;
    QDomDocument play_log;
    file.setFileName(file_name);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning(Develop) << "cannot open file" << file_name;
        file.close();
        return;
    }

    QByteArray xml        = "<contentPlayLog>" + file.readAll() + "</contentPlayLog>";
    QString error_message = "";
    int error_line, error_column = 0;
    if (!play_log.setContent(xml, &error_message, &error_line, &error_column))
    {
        qWarning(Develop) << "cannot read xml" << file_name << error_message
                          << "Line: " << QString::number(error_line)
                          << "Column: " << QString::number(error_column);
        file.close();
        return;
    }
    file.close();
    player.appendChild(play_log);
}
