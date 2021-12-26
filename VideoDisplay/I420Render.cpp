#include "I420Render.h"

I420Render::I420Render()
{
    initializeOpenGLFunctions();
    const char *vsrc =
            "attribute vec4 vertexIn; \
             attribute vec4 textureIn; \
             varying vec4 textureOut;  \
             void main(void)           \
             {                         \
                 gl_Position = vertexIn; \
                 textureOut = textureIn; \
             }";

         const char *fsrc =
                 "varying mediump vec4 textureOut;\n"
                 "uniform sampler2D textureY;\n"
                 "uniform sampler2D textureU;\n"
                 "uniform sampler2D textureV;\n"
                 "void main(void)\n"
                 "{\n"
                 "vec3 yuv; \n"
                 "vec3 rgb; \n"
                 "yuv.x = texture2D(textureY, textureOut.st).r - 0.0625; \n"
                 "yuv.y = texture2D(textureU, textureOut.st).r - 0.5; \n"
                 "yuv.z = texture2D(textureV, textureOut.st).r - 0.5; \n"
                 "rgb = mat3( 1,       1,         1, \n"
                             "0,       -0.39465,  2.03211, \n"
                             "1.13983, -0.58060,  0) * yuv; \n"
                 "gl_FragColor = vec4(rgb, 1); \n"
                 "}\n";

    program.addCacheableShaderFromSourceCode(QOpenGLShader::Vertex,vsrc);
    program.addCacheableShaderFromSourceCode(QOpenGLShader::Fragment,fsrc);
    program.link();

    GLfloat points[]{
        -1.0f, 1.0f,
         1.0f, 1.0f,
         1.0f, -1.0f,
        -1.0f, -1.0f,

        0.0f,1.0f,
        1.0f,1.0f,
        1.0f,0.0f,
        0.0f,0.0f,
    };
    vbo.create();
    vbo.bind();
    vbo.allocate(points,sizeof(points));
    GLuint ids[3];
    glGenTextures(3,ids);
    idY = ids[0];
    idU = ids[1];
    idV = ids[2];
}

void I420Render::render(uchar *dataPtr, int w, int h, int renderW, int renderH)
{
    int vX,vY,vW,vH;
    bool horizontal = (float)renderW / (float)renderH < (float)w / (float)h;
    vW = horizontal ? renderW : w * renderH / h;
    vH = h * vW / w;
    vX = horizontal ? 0: renderW / 2 - vW / 2;
    vY = horizontal ? renderH / 2 - vH / 2: 0;
    glViewport(vX,vY,vW,vH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    program.bind();

    vbo.bind();
    program.enableAttributeArray("vertexIn");
    program.enableAttributeArray("textureIn");
    program.setAttributeBuffer("vertexIn",GL_FLOAT, 0, 2, 2*sizeof(GLfloat));
    program.setAttributeBuffer("textureIn",GL_FLOAT,2 * 4 * sizeof(GLfloat),2,2*sizeof(GLfloat));

    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D,idY);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,w,h,0,GL_RED,GL_UNSIGNED_BYTE,dataPtr);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D,idU);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,w >> 1,h >> 1,0,GL_RED,GL_UNSIGNED_BYTE,dataPtr + w*h);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D,idV);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,w >> 1,h >> 1,0,GL_RED,GL_UNSIGNED_BYTE,dataPtr + w*h*5/4);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    program.setUniformValue("textureY",2);
    program.setUniformValue("textureU",1);
    program.setUniformValue("textureV",0);

    glDrawArrays(GL_QUADS,0,4);

    program.disableAttributeArray("vertexIn");
    program.disableAttributeArray("textureIn");
    program.release();

}

