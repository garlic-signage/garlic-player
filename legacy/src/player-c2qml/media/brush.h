#ifndef BRUSH_H
#define BRUSH_H

#include <QObject>
#include "player_base_media.h"
#include "smilparser/media/brush.h"

class Brush : public PlayerBaseMedia
{
        Q_OBJECT
    public:
        Brush(QQmlComponent *mc, QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent = nullptr);
        ~Brush();
        void          loadMedia(BaseMedia *media, Region *reg);
        void          restart();
        void          play();
        void          stop();
        void          resume();
        void          pause();
        void          setParentItem(QQuickItem *parent);
        void          changeSize(int w, int h);
    protected:
        TBrush        *MyBrush;
        QScopedPointer<QQuickItem, QScopedPointerDeleteLater>  brush_item;
};

#endif // BRUSH_H
