#ifndef I420RENDER_H
#define I420RENDER_H
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QtGui/qvector3d.h>
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qopenglshaderprogram.h>
#include <QtGui/qopenglfunctions.h>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLBuffer>
#include <QVector>
#include <QRect>
#include <QPainter>
#include <QPaintEngine>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <math.h>
class I420Render : public QOpenGLFunctions
{
public:
    I420Render();
    void render(uchar*i420ptr, int w, int h, QRect drawPosition);

private:
    QImage m_image;
    QOpenGLShaderProgram program;
    GLuint idY,idUV, idU, idV;
    QOpenGLBuffer vbo;
};
#endif // I420Render_H
