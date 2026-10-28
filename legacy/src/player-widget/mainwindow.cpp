/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2016 Nikolaos Saghiadinos <ns@smil-control.com>
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

#include "mainwindow.h"

MainWindow::MainWindow(TScreen *screen, LibFacade *lib_facade)
{
    centralWidget          = new QWidget(this); // had to be there to get fullscreen simulation over multiple monitors
    MyScreen               = screen;
    MyLibFacade            = lib_facade;
    connect(MyLibFacade, SIGNAL(startShowMedia(BaseMedia*)), this, SLOT(startShowMedia(BaseMedia*)));
    connect(MyLibFacade, SIGNAL(stopShowMedia(BaseMedia*)), this, SLOT(stopShowMedia(BaseMedia*)));
    connect(MyLibFacade, SIGNAL(resumeShowMedia(BaseMedia*)), this, SLOT(resumeShowMedia(BaseMedia*)));
    connect(MyLibFacade, SIGNAL(pauseShowMedia(BaseMedia*)), this, SLOT(pauseShowMedia(BaseMedia*)));
    connect(MyLibFacade, SIGNAL(readyForPlaying()), this, SLOT(prepareParsing()));

    connect(MyLibFacade, SIGNAL(rebootOS(QString)), this, SLOT(rebootOS(QString)));
    connect(MyLibFacade, SIGNAL(installSoftware(QString)), this, SLOT(installSoftware(QString)));
    connect(MyLibFacade, SIGNAL(screenshot(QString)), this, SLOT(takeScreenshot(QString)));

    setCursor(Qt::BlankCursor);
    setCentralWidget(centralWidget);
    MyRegionsList = new RegionsList(this);
    MyLauncher.reset(new Launcher());
}

MainWindow::~MainWindow()
{
    MyRegionsList->remove();
    delete MyRegionsList;
    delete centralWidget;
}

void MainWindow::keyPressEvent(QKeyEvent *ke)
{
    if (ke->modifiers().testFlag(Qt::NoModifier))
    {
        MyLibFacade->transferAccessKey(ke->text().toLower().at(0));
        return;
    }
    switch (ke->key())
    {
        case Qt::Key_F:
            if (screen_state != FULLSCREEN)
                resizeAsNormalFullScreen();
            else
                resizeAsWindow();
           break;
        case Qt::Key_B:
            if (screen_state != BIGFULLSCREEN)
                 resizeAsBigFullScreen();
            else
                resizeAsWindow();
            break;
        case Qt::Key_D:
            setCursor(Qt::ArrowCursor);
            openDebugInfos();
            setCursor(Qt::BlankCursor);
            break;
        case Qt::Key_C:
            setCursor(Qt::ArrowCursor);
            openCommandline();
            setCursor(Qt::BlankCursor);
            break;
        case Qt::Key_S:
            setCursor(Qt::ArrowCursor);
            if (openConfigDialog() == QDialog::Accepted)
            {
                MyLibFacade->initParser();
            }
            setCursor(Qt::BlankCursor);
            break;
        case Qt::Key_Q:
             QApplication::quit();
            break;
    }
}

bool MainWindow::event(QEvent *event)
{
    event->accept();
    if(event->type() == QEvent::TouchBegin || event->type() == QEvent::MouseButtonPress)
    {
        first_touch = QDateTime::currentMSecsSinceEpoch();
        qint64 delay = QDateTime::currentMSecsSinceEpoch() - last_touch;
        if (delay < 500)
        {
            count_touch++;
        }
        else
        {
            count_touch = 0;
        }

        if (count_touch > 5)
        {
            count_touch = 0;
            openDebugInfos();
        }
    }
    if(event->type() == QEvent::TouchEnd || event->type() == QEvent::MouseButtonRelease)
    {
        last_touch = QDateTime::currentMSecsSinceEpoch();
        if (last_touch - first_touch > 10000)
            openDebugInfos();
    }
    return QMainWindow::event(event);
}


void MainWindow::openDebugInfos()
{
    DebugInfos MyDebugInfos(MyLibFacade, Q_NULLPTR);
    if (MyDebugInfos.exec() == QDialog::Accepted)
    {
        QApplication::quit();
    }}

void MainWindow::openCommandline()
{
    QProcess process;
    process.start("/bin/bash", QStringList() << "mc");
    process.waitForFinished(-1); // will wait forever until finished
}

int MainWindow::openConfigDialog()
{
    ConfigDialog MyConfigDialog(Q_NULLPTR, MyLibFacade->getConfiguration());
    return MyConfigDialog.exec();
}

void MainWindow::resizeAsNormalFullScreen()
{
    move(MyScreen->getStartPointFromCurrentScreen());
    setCursor(Qt::BlankCursor);
    showFullScreen();
    screen_state = FULLSCREEN;
}

void MainWindow::resizeAsBigFullScreen()
{
    // To work correct a taskbar had to set for overlap or hidden
    move(0,0);
    setWindowFlags(Qt::CustomizeWindowHint  | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setCursor(Qt::BlankCursor);
    showNormal();

    resize(MyScreen->getWholeSize());
    screen_state = BIGFULLSCREEN;
}

void MainWindow::resizeAsWindow()
{
    move(MyScreen->getStartPointFromCurrentScreen());
    setWindowFlags(Qt::Window);
    setCursor(Qt::ArrowCursor);

    // This nonsense workaround prevent that MainWindow showed maximized after full- or bigscreen.
    showNormal();
    setWindowState(Qt::WindowMaximized);
    showNormal();
    resize(QSize(980, 540));

    screen_state = WINDOWED;
}

void MainWindow::setMainWindowSize(QSize size)
{
    mainwindow_size = size;
}

QSize MainWindow::getMainWindowSize()
{
    return mainwindow_size;
}

// =================== protected methods ====================================


void MainWindow::resizeEvent(QResizeEvent * event)
{
    MyRegionsList->resize(event->size());
    QMainWindow::resizeEvent(event);
}

// =================== protected slots ====================================

void MainWindow::prepareParsing()
{
    MyRegionsList->remove(); // Must be done first to be clear that no media is loaded or played anymore

    setStyleSheet("background-color:"+MyLibFacade->getHead()->getRootBackgroundColor()+";");
    MyRegionsList->create(MyLibFacade, MyLauncher.data(), QSize(width(), height()));
    MyLibFacade->beginSmilPlaying(); // begin playing not before Layout ist build to prevent crash in MainWindow::startShowMedia
}

void MainWindow::rebootOS(QString task_id)
{
    MyLauncher.data()->rebootOS(task_id);
}

void MainWindow::installSoftware(QString file_path)
{
    MyLauncher.data()->installSoftware(file_path);
}


void MainWindow::takeScreenshot(QString file_path)
{
    MyScreen->takeScreenshot(file_path);
}

void MainWindow::startShowMedia(BaseMedia *media)
{
    MyRegionsList->startShowMedia(media);
}

void MainWindow::stopShowMedia(BaseMedia *media)
{
    MyRegionsList->stopShowMedia(media);
}

void MainWindow::resumeShowMedia(BaseMedia *media)
{
    MyRegionsList->resumeShowMedia(media);
}

void MainWindow::pauseShowMedia(BaseMedia *media)
{
    MyRegionsList->pauseShowMedia(media);
}
