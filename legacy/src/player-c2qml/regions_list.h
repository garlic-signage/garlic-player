#ifndef REGIONSLIST_H
#define REGIONSLIST_H

#include <QQuickItem>

#include <QObject>
#include "region.h"

class RegionsList : public QObject
{
    Q_OBJECT
public:
    explicit RegionsList(QObject *parent = nullptr);
    ~RegionsList();
    void                      create(LibFacade *MyLibFacade, QSize rs, QQuickItem *qv, Launcher *lc, QQmlEngine *e);
    void                      remove();
    void                      resize(QSize rs);
    QStringList               select(QString region_name);
    void                      startShowMedia(BaseMedia *media);
    void                      stopShowMedia(BaseMedia *media);
    void                      resumeShowMedia(BaseMedia *media);
    void                      pauseShowMedia(BaseMedia *media);


private:
    QSize                       root_size;
    QMap<QString, TRegion *>    regions_list;
    QMultiMap<QString, QString> regions_assign_list;

signals:

};

#endif // REGIONSLIST_H
