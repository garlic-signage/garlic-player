#ifndef REFCOMMAND_H
#define REFCOMMAND_H

#include "player_base_media.h"

class RefCommand  : public PlayerBaseMedia
{
    Q_OBJECT
public:
    explicit RefCommand(QQmlComponent *mc, QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent = nullptr);
    ~RefCommand();

    void           loadMedia(BaseMedia *media, Region *reg);
    void           restart();
    void           play();
    void           stop();
    void           resume();
    void           pause();
    void           setParentItem(QQuickItem *parent);
    void           changeSize(int w, int h);
protected:
    QScopedPointer<QQuickItem, QScopedPointerDeleteLater>  ref_item;

};

#endif // REFCOMMAND_H
