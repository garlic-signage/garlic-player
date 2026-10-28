#ifndef SCREEN_H
#define SCREEN_H

#include <QObject>
#include <QMainWindow>
#include <QDesktopWidget>
#include <QGuiApplication>
#include <QScreen>
#include "tools/logging_categories.h"

class TScreen : public QObject
{
    Q_OBJECT
public:
    explicit TScreen(QObject *parent = Q_NULLPTR);
    QPoint   getWholeStartPoint();
    QSize    getWholeSize();
    void     takeScreenshot(QString file_path);
    void     selectCurrentScreen(int screen_id);
    QPoint   getStartPointFromCurrentScreen();
    QSize    getSizeFromCurrentScreen();
    void     determineCurrentScreenId(QPoint point);
    QRect    getQRectFromScreen();
public slots:

protected:
    QList<QScreen *> listScreens;
    QRect           whole_display_geometry;
    QScreen        *current_screen = Q_NULLPTR;
    void            calculateWholeDisplayGeometrie();
};

#endif // SCREEN_H
