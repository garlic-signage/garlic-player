#ifdef SUPPORT_QTMM

#ifndef QTMM_WIDGET_H
#define QTMM_WIDGET_H

#include <QWidget>
#include <QVideoWidget>

class QtMMWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QtMMWidget(QWidget *parent = Q_NULLPTR);
    ~QtMMWidget();
    QWidget              *getVideoWidget();
    QVideoWidget         *getVideoRenderer();

    void                  ignoreAspectRatio();
    void                  keepAspectRatio();
    void                  keepAspectRatioByExpanding();
signals:
protected:
    QVideoWidget         VideoWidget;
public slots:
};
#endif

#endif // QTMM_WIDGET_H
