#include "event_logs.h"

Reporting::CreateEventLogs::CreateEventLogs(MainConfiguration *config, QObject *parent) : Reporting::CreateBase(config, parent)
{

}

void Reporting::CreateEventLogs::process(QString file_name)
{
    init();
    mergeXml(MyConfiguration->getPaths("logs") + file_name);
}

void Reporting::CreateEventLogs::mergeXml(QString file_name)
{
    QFile file;
    QDomDocument event_log;
    file.setFileName(file_name);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning(Develop) << "cannot open file" << file_name;
        file.close();
        return;
    }

    QByteArray xml        = "<playerEventLog>" + file.readAll() + "</playerEventLog>";
    QString error_message = "";
    int error_line, error_column = 0;
    if (!event_log.setContent(xml, &error_message, &error_line, &error_column))
    {
        qWarning(Develop) << "cannot read xml" << file_name << error_message
                          << "Line: " << QString::number(error_line)
                          << "Column: " << QString::number(error_column);
        file.close();
        return;
    }
    file.close();
    player.appendChild(event_log);
}
