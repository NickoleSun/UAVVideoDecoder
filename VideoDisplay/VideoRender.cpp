#include "VideoRender.h"

class VideoFboRender : public QQuickFramebufferObject::Renderer
{
public:
    VideoFboRender(){

    }

    ~VideoFboRender() override {

    }
public:
    void synchronize(QQuickFramebufferObject *item) override {
        m_i420Ptr = dynamic_cast<VideoRender*>(item)->m_data;
        m_videoH = dynamic_cast<VideoRender*>(item)->m_height;
        m_videoW = dynamic_cast<VideoRender*>(item)->m_width;
        m_drawPosition = dynamic_cast<VideoRender*>(item)->m_drawPosition;
    }

    void render() override{
        if(m_i420Ptr == nullptr || m_videoH == 0 || m_videoW == 0) return;

        m_render.render(m_i420Ptr, m_videoW,m_videoH,m_drawPosition);
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        format.setSamples(1);
        return new QOpenGLFramebufferObject(size,format);
    }

private:
    I420Render m_render;
    unsigned char *m_i420Ptr = nullptr;
    int m_videoW = 0;
    int m_videoH = 0;
    QRect m_drawPosition;
};

VideoRender::VideoRender(QQuickItem *parent) : QQuickFramebufferObject(parent)
{

}

VideoRender::~VideoRender()
{

}

QQuickFramebufferObject::Renderer *VideoRender::createRenderer() const
{
    return new VideoFboRender();
}

void VideoRender::handleNewFrame(unsigned char *t_img, const int &t_w, const int &t_h)
{
    this->m_data = t_img;
    this->m_width = t_w;
    this->m_height = t_h;
    int renderW = static_cast<int>(this->width());
    int renderH = static_cast<int>(this->height());
    int w = t_w;
    int h = t_h;
    int vX,vY,vW,vH;
    bool horizontal = static_cast<float>(renderW) / static_cast<float>(renderH) <
            static_cast<float>(w) / static_cast<float>(h);
    vW = horizontal ? renderW : w * renderH / h;
    vH = h * vW / w;
    vX = horizontal ? 0: renderW / 2 - vW / 2;
    vY = horizontal ? renderH / 2 - vH / 2: 0;
    this->m_drawPosition = QRect(vX,vY,vW,vH);
    this->update();
    Q_EMIT drawPositionChanged();
}

