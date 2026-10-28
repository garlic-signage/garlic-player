#ifndef QTAVPLAYERWIDGET_H
#define QTAVPLAYERWIDGET_H

#include <QObject>
#include <QVideoWidget>

#include <QAbstractVideoSurface>
#include <QVideoSurfaceFormat>
#include <QMediaService>
#include <QMediaObject>
#include <QVideoRendererControl>

class VideoRenderer : public QVideoRendererControl
{
public:
    QAbstractVideoSurface *surface() const override
    {
        return m_surface;
    }

    void setSurface(QAbstractVideoSurface *surface) override
    {
        m_surface = surface;
    }

    QAbstractVideoSurface *m_surface = nullptr;
};

class MediaService : public QMediaService
{
public:
    MediaService(VideoRenderer *vr, QObject* parent = nullptr)
        : QMediaService(parent)
        , m_renderer(vr)
    {
    }

    QMediaControl* requestControl(const char *name) override
    {
        if (qstrcmp(name, QVideoRendererControl_iid) == 0)
            return m_renderer;

        return nullptr;
    }

    void releaseControl(QMediaControl *) override
    {
    }

    VideoRenderer *m_renderer = nullptr;
};
class MediaObject : public QMediaObject
{
public:
    explicit MediaObject(VideoRenderer *vr, QObject* parent = nullptr)
        : QMediaObject(parent, new MediaService(vr, parent))
    {
    }
};

class VideoWidget : public QVideoWidget
{
public:
    bool setMediaObject(QMediaObject *object) override
    {
        return QVideoWidget::setMediaObject(object);
    }
};

class QtAVPlayerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QtAVPlayerWidget(QWidget *parent = nullptr);
    ~QtAVPlayerWidget();
    QWidget              *getVideoWidget();
    VideoRenderer        *getVideoRenderer();

    void                  ignoreAspectRatio();
    void                  keepAspectRatio();
    void                  keepAspectRatioByExpanding();
signals:
protected:
    MediaObject          *MyMediaObject;
    VideoWidget          MyVideoWidget;
    VideoRenderer         vr;

};

#endif // QTAVPLAYERWIDGET_H
