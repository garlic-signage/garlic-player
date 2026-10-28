#include <QString>
#include <QtTest>
#include "tools/log_file.h"

class Thread : public QThread
{
    Q_OBJECT

public:
    Thread(LogFile *lf)
    {
        MyLogFile = lf;
    }
    qint64 getFileSize(){return file_size;}
protected:
    LogFile *MyLogFile;
    qint64   file_size = 0;
    void run()
    {
        QString str = "_ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz";
        for (int i = 1; i < 16001; i++)
        {
            QString write = QString::number(i) + str;
            MyLogFile->write(write);
            file_size += write.length()+1; // cause of the endl
        }

    }

};

class TestLogFile : public QObject
{
    Q_OBJECT

public:
    TestLogFile();

private Q_SLOTS:
    void cleanup();
    void testWriteFile();
    void testWriteFileDuringInterupted();
    void testRotateFileLowerThanMaxNUM();
    void testRotateFileGreaterThanMaxNUM();
private:
    QString file_name = "test.log";
    void createLogFiles(int num_files);
    QStringList  readFileContents(QFile *file);
};

TestLogFile::TestLogFile()
{
}

void TestLogFile::cleanup()
{
    QDir dir(".");
    QFile   file;
    // find all a files with this name
   foreach (const QString &match,  dir.entryList(QStringList(file_name+"*"), QDir::Files, QDir::Time | QDir::Reversed))
   {
        file.setFileName(match);
        file.remove();
   }
}

void TestLogFile::testWriteFileDuringInterupted()
{
//    We do not want to lose log entries during a rotate
//    To simulate there is thread-class writing continuesly content into logfile
//    after 200msec a request for a rotate is sended
//    both files should have exact the size of
    LogFile MyLogFile("./"+file_name);
    MyLogFile.setMaxSize(1024*1024*10); // 10MB to have some tome on fast machines
    Thread MyThread(&MyLogFile);
    MyThread.start();

    bool is_rotated = false;
    while (!MyThread.isFinished())
    {
        QTest::qWait(20);
        if (!is_rotated) // rotate only one time
        {
            is_rotated = true;
            MyLogFile.rotateFile();
        }
    }
    QFile file1(file_name);
    QFile file2(file_name+".1");

    QVERIFY(file1.exists());
    QVERIFY(file1.size() > 0);
    QVERIFY(file2.exists());
    QVERIFY(file2.size() > 0);

    QStringList list2 = readFileContents(&file2);
    QStringList list2_begin = list2.at(0).split('_');
    int list2_end_number = list2.at(list2.length()-1).split('_').at(0).toInt();
    QCOMPARE(list2_begin.at(0), QString("1"));

    QStringList list1 = readFileContents(&file1);
    int list1_begin_number = list1.at(0).split('_').at(0).toInt();
    QCOMPARE(list1.at(list1.length()-1).split('_').at(0), QString("16000"));
    QVERIFY(file1.size()+file2.size() <= MyThread.getFileSize()); // some \n can be left so Filesize can be greater but never smaller
    QVERIFY(list1.length()+list2.length() >= 16000); // some \n can be left so length can be greater but never smaller

    QCOMPARE(list2_end_number+1, list1_begin_number);
}

void TestLogFile::testWriteFile()
{
    LogFile MyLogFile("./"+file_name);
    MyLogFile.setMaxSize(1024); // 1 KiB
    QString str_62Byte = "_ABCDEFGHIJKLMNOPQRSTUVWXYZ12345678abcdefghijklmnopqrstuvwxyz";
    int j = 10;
    for (int i=0; i < 16; i++)
    {
        if (j > 19)
            j = 10;
        MyLogFile.write(QString::number(j) + str_62Byte);
        j++;
    }
    QVERIFY(QFile::exists(file_name));
    QVERIFY(QFile::exists(file_name+".1"));
    for (int i=0; i < 32; i++)
    {
        if (j > 19)
            j = 10;
        MyLogFile.write(QString::number(j) + str_62Byte);
        j++;
    }
    QVERIFY(QFile::exists(file_name+".2"));
    QVERIFY(QFile::exists(file_name+".3"));
    QVERIFY(!QFile::exists(file_name+".4"));
    QFile file(file_name);
    QCOMPARE(file.size(), 0);
    file.setFileName(file_name+".1");
    QCOMPARE(file.size(), 1024);
    file.setFileName(file_name+".2");
    QCOMPARE(file.size(), 1024);
    file.setFileName(file_name+".3");
    QCOMPARE(file.size(), 1024);
}


void TestLogFile::testRotateFileLowerThanMaxNUM()
{
    LogFile MyLogFile("./"+file_name);
    QFile   file;
    int num_files = 5;

    createLogFiles(num_files);
    QDir dir(".");
    QStringList list = dir.entryList(QStringList(file_name+"*"), QDir::Files, QDir::Time);
    QCOMPARE(list.size(), num_files+1);

    MyLogFile.rotateFile();
    for (int i = num_files+1; i > -1; i--)
    {
        if (i != 0)
            file.setFileName(file_name+"."+QString::number(i));
        else
            file.setFileName(file_name);

        QVERIFY(file.exists());
        if (file.open(QIODevice::ReadOnly) )
        {
            QByteArray result = "";
            QByteArray expected = "";
            if (i != 0)
            {
                result = file.read(3);
                expected = QByteArray::number(i-1)+"\n";
            }
            else
            {
                result = file.read(3);
                expected = "";
            }
            QVERIFY(result == expected);
            file.close();
        }
    }
    list = dir.entryList(QStringList(file_name+"*"), QDir::Files, QDir::Time);
    QCOMPARE(list.size(), num_files+2);

 }

void TestLogFile::testRotateFileGreaterThanMaxNUM()
{
    LogFile MyLogFile("./"+file_name);
    QFile   file;
    int num_files = 20;
    MyLogFile.setMaxNumber(num_files);
    createLogFiles(num_files); // Should create exact MAX_LOG..!
    QDir dir(".");
    QStringList list = dir.entryList(QStringList(file_name+"*"), QDir::Files, QDir::Time);
    QCOMPARE(list.size(), num_files+1);

    MyLogFile.rotateFile();

    for (int i = num_files+1; i > -1; i--)
    {
        if (i != 0)
            file.setFileName(file_name+"."+QString::number(i));
        else
            file.setFileName(file_name);

        if (i == num_files+1)
            QVERIFY(!file.exists());
        else
        {
            QVERIFY(file.exists());
            if (file.open(QIODevice::ReadOnly) )
            {
                QByteArray result = "";
                QByteArray expected = "";
                if (i != 0)
                {
                    result = file.read(3);
                    expected = QByteArray::number(i-1)+"\n";
                }
                else
                {
                    result = file.read(3);
                    expected = "";
                }
                QVERIFY(result == expected);
                file.close();
            }
        }
    }
    list = dir.entryList(QStringList(file_name+"*"), QDir::Files, QDir::Time);
    QCOMPARE(list.size(), num_files+1);
}


void TestLogFile::createLogFiles(int num_files)
{
    QFile file;
    for (int i = num_files; i > -1; i--)
    {
        if (i > 0)
            file.setFileName(file_name+ "."+ QString::number(i));
        else
           file.setFileName(file_name);

        if (file.open(QIODevice::ReadWrite) )
        {
            QTextStream stream(&file);
            stream << QString::number(i) << endl;
            file.close();
        }
        QTest::qWait(10); // only 1o msec to get sure that files are not created same time
    }
}

QStringList TestLogFile::readFileContents(QFile *file)
{
    QStringList list;
    file->open(QIODevice::ReadOnly| QIODevice::Text);
    QTextStream textStream(file);
    while (true)
    {
        QString line = textStream.readLine();
        if (line.isNull())
            break;
        else
            list.append(line);
    }
    return list;
}



QTEST_MAIN(TestLogFile)

#include "tst_log_file.moc"
