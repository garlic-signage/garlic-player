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
#include "qdialog.h"
#include "../player-common/debug_infos.h"
#include "../player-common/configdialog.h"

MainWindow::MainWindow(TScreen *screen, LibFacade *lib_facade, PlayerConfiguration *pc)
{
    MyScreen               = screen;
    MyLibFacade            = lib_facade;
    MyPlayerConfiguration  = pc;
    MyRegionsList          = new RegionsList(this);
    MyLauncher.reset(new Launcher());
    MyLauncher.data()->toogleLauncher(MyPlayerConfiguration->hasLauncher());

}

MainWindow::~MainWindow()
{
    delete MyRegionsList; // region must be deleted at last, cause media pointer had to be deleted
}

void MainWindow::init()
{
    connect (this, SIGNAL(statusChanged(QQuickView::Status)), this, SLOT(doStatusChanged(QQuickView::Status)));
    connect(MyLibFacade, SIGNAL(startShowMedia(BaseMedia*)), this, SLOT(startShowMedia(BaseMedia*)));
    connect(MyLibFacade, SIGNAL(stopShowMedia(BaseMedia*)), this, SLOT(stopShowMedia(BaseMedia*)));
    connect(MyLibFacade, SIGNAL(resumeShowMedia(BaseMedia*)), this, SLOT(resumeShowMedia(BaseMedia*)));
    connect(MyLibFacade, SIGNAL(pauseShowMedia(BaseMedia*)), this, SLOT(pauseShowMedia(BaseMedia*)));
    connect(MyLibFacade, SIGNAL(readyForPlaying()), this, SLOT(prepareParsing()));

    connect(MyLibFacade, SIGNAL(rebootOS(QString)), this, SLOT(rebootOS(QString)));
    connect(MyLibFacade, SIGNAL(installSoftware(QString)), this, SLOT(installSoftware(QString)));
    connect(MyLibFacade, SIGNAL(screenshot(QString)), this, SLOT(takeScreenShot(QString)));
    connect(engine(), SIGNAL(quit()), QCoreApplication::instance(), SLOT(quit())); // to connect quit signal from QML

    setSource(QUrl(QStringLiteral("qrc:/root_qtm.qml")));
    setMainWindowSize(QSize(980, 540)); // set default
    MyRegionsList->installEventFilter(this);
}

void MainWindow::keyPressEvent(QKeyEvent *ke)
{
//    if (ke->modifiers().testFlag(Qt::NoModifier))
    if (QGuiApplication::queryKeyboardModifiers() == Qt::NoModifier)
    {
        MyLibFacade->transferAccessKey(ke->text().toLower().at(0));
        return;
    }

    if (QGuiApplication::queryKeyboardModifiers() != Qt::ControlModifier)
        return;

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
        case Qt::Key_S:  // Ctrl-C will not work during qwebview
            openConfigDialog();
        break;

        case Qt::Key_Q: // quit app normal
            quitApplication();
        break;
        case Qt::Key_C: // quit app not normal e.g. to simulate a crash
            QApplication::quit();
        break;
    }
    return;
}

bool MainWindow::event(QEvent *event)
{
    event->accept();
    if(event->type() == QEvent::TouchBegin || event->type() == QEvent::MouseButtonPress)
    {
        qint64 delay = QDateTime::currentMSecsSinceEpoch() - last_touch;
        if (delay < 500)
        {
            count_touch++;
        }
        else
        {
            count_touch = 0;
        }

        if (count_touch > 9)
        {
            count_touch = 0;
            openDebugInfos();
        }
    }
    if(event->type() == QEvent::TouchEnd || event->type() == QEvent::MouseButtonRelease)
    {
        last_touch = QDateTime::currentMSecsSinceEpoch();
    }
    return QQuickView::event(event);
}

void MainWindow::openDebugInfos()
{
    DebugInfos MyDebugInfos(MyLibFacade, Q_NULLPTR);
    if (MyDebugInfos.exec() == QDialog::Accepted)
    {
        quitApplication();
    }
}


int MainWindow::openConfigDialog()
{
    int ret = QDialog::Rejected;
    if (MyPlayerConfiguration->hasLauncher())
    {
        return ret;
    }
    ConfigDialog MyConfigDialog(0, MyLibFacade->getConfiguration());
    setCursor(Qt::ArrowCursor);
    if (MyConfigDialog.exec() == QDialog::Accepted)
    {
        MyLibFacade->initParser();
        ret = QDialog::Accepted;
    }
    setCursor(Qt::BlankCursor);

    return ret;
}

void MainWindow::resizeAsNormalFullScreen()
{
    screen_state = FULLSCREEN;
    setCursor(Qt::BlankCursor);
    showFullScreen();
}

void MainWindow::resizeAsBigFullScreen()
{
    screen_state = BIGFULLSCREEN;
    setFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setCursor(Qt::BlankCursor);
    showNormal();
    setPosition(0, 0);
    resize(MyScreen->getWholeSize());
}

void MainWindow::resizeAsWindow()
{
    screen_state = WINDOWED;
    setFlags(Qt::Window);
    setCursor(Qt::ArrowCursor);
    showNormal();
    MyScreen->determineCurrentScreenId(this->geometry().topLeft());
    setPosition(MyScreen->getStartPointFromCurrentScreen());
    resize(QSize(980, 540));
}

void MainWindow::setMainWindowSize(QSize size)
{
    mainwindow_size = size;
}

QSize MainWindow::getMainWindowSize()
{
    return mainwindow_size;
}

void MainWindow::resizeEvent(QResizeEvent * event)
{
    MyRegionsList->resize(event->size());
    QQuickView::resizeEvent(event);
}

// =================== protected slots ====================================

void MainWindow::prepareParsing()
{
    MyRegionsList->remove(); // Must be done first to be clear that no media is loaded or played anymore
    setColor(MyLibFacade->getHead()->getRootBackgroundColor());
    MyRegionsList->create(MyLibFacade, QSize(width(), height()), rootObject(), MyLauncher.data(), engine());

    MyLibFacade->beginSmilPlaying(); // begin playing not before Layout ist build to prevent crash in MainWindow::startShowMedia
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

void MainWindow::doStatusChanged(QQuickView::Status status)
{
    switch (status)
    {
        case QQuickView::Null:
            qDebug(MediaPlayer) << "No QML source set";
            break;
        case QQuickView::Ready:
            MyLibFacade->initParser();
            break;
        case QQuickView::Loading:
            qDebug(MediaPlayer) << "QML loaded/compiled... ";
            break;
        case QQuickView::Error:
            QList<QQmlError> qml_errors = errors();
            QList<QQmlError> ::iterator i;
            for (i = qml_errors.begin(); i != qml_errors.end(); ++i)
            {
                QQmlError err = *i;
                qDebug(MediaPlayer) << err.messageType() << err.description() << err.line();
            }
            break;
    }

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

void MainWindow::sendClosePlayerCorrect()
{
    MyLauncher.data()->sendClosePlayerCorrect();
    qDebug(MediaPlayer) << "send ClosePlayerCorrect";
}

void MainWindow::quitApplication()
{
    MyLibFacade->shutDownParsing();
    sendClosePlayerCorrect();
    QApplication::quit();
}
