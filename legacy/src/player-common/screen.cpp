#include "screen.h"

TScreen::TScreen(QObject *parent) : QObject(parent)
{
     current_screen = QGuiApplication::primaryScreen();
     calculateWholeDisplayGeometrie();
}

QPoint TScreen::getWholeStartPoint()
{
    return QPoint(whole_display_geometry.x(), whole_display_geometry.y());
}

QSize TScreen::getWholeSize()
{
    return QSize(whole_display_geometry.width(), whole_display_geometry.height());
}

void TScreen::takeScreenshot(QString file_path)
{
    QRect geom         = current_screen->geometry();
    QPixmap screenshot = current_screen->grabWindow(0, geom.x(), geom.y(), geom.width(), geom.height());
    screenshot.save(file_path, "JPG", 60);
}

void TScreen::selectCurrentScreen(int screen_id)
{
    if (screen_id < 0)
        current_screen = listScreens.first();
    else if (screen_id > listScreens.size())
        current_screen = listScreens.last();
    else
        current_screen = listScreens.at(screen_id);
}

QPoint TScreen::getStartPointFromCurrentScreen()
{
    return current_screen->geometry().topLeft();
}

QRect TScreen::getQRectFromScreen()
{
    return current_screen->geometry();
}



QSize TScreen::getSizeFromCurrentScreen()
{
    return getQRectFromScreen().size();
}

void TScreen::determineCurrentScreenId(const QPoint point)
{
    if (QGuiApplication::screenAt(point) != Q_NULLPTR)
    {
        current_screen = QGuiApplication::screenAt(point);
    }
}

void TScreen::calculateWholeDisplayGeometrie()
{
    listScreens = QGuiApplication::screens();
    QList<QScreen *>::iterator j;
    for (j = listScreens.begin(); j != listScreens.end(); j++)
    {
        QScreen *screen = *j;
        qInfo(Screen) << "Monitor: " << screen->model() << screen->name() << " Geometry:" << screen->availableGeometry();
        whole_display_geometry = whole_display_geometry.united(screen->availableGeometry()); //union
    }

    qInfo(Screen) << " DisplayGeometrie x:" << whole_display_geometry.x() << "y:" << whole_display_geometry.y() << "width:" << whole_display_geometry.width() << "height:" << whole_display_geometry.height();
}
