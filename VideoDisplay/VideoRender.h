#ifndef VIDEORENDER_H
#define VIDEORENDER_H

#include <QQuickFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include "I420Render.h"

class VideoRender : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QRect drawPosition READ drawPosition NOTIFY drawPositionChanged)
public:
    explicit VideoRender(QQuickItem* parent = nullptr);
    ~VideoRender() override;
    Renderer *createRenderer() const override;
    QRect drawPosition() { return m_drawPosition; }
    Q_SIGNALS:
    void drawPositionChanged();
public Q_SLOTS:
    void handleNewFrame(unsigned char *t_img, const int &t_w, const int &t_h);

public:
    QString m_videoSource = "";
    unsigned char *m_data = nullptr;
    int m_width = 0;
    int m_height = 0;
    QRect m_drawPosition;
};

#endif // VIDEORENDER_H
