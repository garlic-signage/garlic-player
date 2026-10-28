#ifndef WEB_H
#define WEB_H

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineProfile>

#include "player_base_media.h"

class PlayerWeb : public PlayerBaseMedia
{
    Q_OBJECT
    public:
        PlayerWeb(QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent = nullptr);
        ~PlayerWeb();
        void            loadMedia(BaseMedia *media, Region *reg);
        void            restart();
        void            play();
        void            stop();
        void            resume();
        void            pause();
        void            changeSize(int w, int h);
        QWidget        *getView();
    protected:
        QWebEngineView *browser;
    protected slots:
        void             doLoadFinished(bool has_succeeded);
    private:
        QWebEnginePage *page;
        QString          sanitizeUri(QString uri);
        QString          port;
};

#endif // WEB_H
