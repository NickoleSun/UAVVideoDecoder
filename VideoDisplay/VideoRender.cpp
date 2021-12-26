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
        m_renderW = static_cast<int>(item->width());
        m_renderH = static_cast<int>(item->height());
    }

    void render() override{
        if(m_i420Ptr == nullptr || m_videoH == 0 || m_videoW == 0) return;
        m_render.render(m_i420Ptr, m_videoW,m_videoH,m_renderW,m_renderH);
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
    int m_renderW = 0;
    int m_renderH = 0;
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

void VideoRender::handleNewFrame(unsigned char *_img, const int &_w, const int &_h)
{
    this->m_data = _img;
    this->m_width = _w;
    this->m_height = _h;
    this->update();
}

