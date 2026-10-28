/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2024 Nikolaos Saghiadinos <ns@smil-control.com>
    This file is part of the garlic-player source code

    This program is free software: you can redistribute it and/or  modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************************/
#ifndef LOGFILE_H
#define LOGFILE_H

#include <QDir>
#include <QFileInfo>
#include <QScopedPointer>
#include <QTextStream>

class LogFile : public QObject
{
        Q_OBJECT
    public:
        explicit LogFile(QString file_path, QObject *parent = nullptr);
        void     setMaxSize(qint64 size);
        void     setMaxNumber(qint64 number);
        void     write(QString data);
        void     rotateFile();
    protected:
        const qint64 MAX_LOG_FILE_SIZE    = 1048576; // 1 MiB
        const qint64 MAX_LOG_FILE_NUMBER  = 30;
        QString                     buffer;
        bool                        is_rotated = false;
        QTextStream                 out;
        QScopedPointer<QFile>       log_file;
        QScopedPointer<QFileInfo>   log_file_info;
        qint64                      max_size   = MAX_LOG_FILE_SIZE;
        qint64                      max_number = MAX_LOG_FILE_NUMBER;
        bool                        isSizeExeeds();
};

#endif // LOGFILE_H
