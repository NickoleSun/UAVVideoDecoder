#include "VideoEngine.h"
#include "Decoder/Gstreamer/GStreamDecoder.h"
#include "Decoder/FFMpeg/FFMPEGDecoder.h"
#include "Decoder/MC/MCDecoder.h"
#include <unistd.h>
VideoEngine::VideoEngine(QObject *parent) : QObject(parent)
{
    m_decoderList.append(new FFMPEGDecoder);
    m_decoderList.append(new MCDecoder);
    m_decoderList.append(new GStreamDecoder);
    Q_EMIT decoderListChanged();
    changeDecoder(m_decoderList[0]->decodeType());
}
VideoEngine::~VideoEngine()
{
    if(m_decoder != nullptr)
    {
        m_decoder->stop();
        while(m_decoder->isRunning())
        {
            printf("%s\r\n",m_decoder->decodeType().toStdString().c_str());
            m_decoder->stop();
            sleep(1);
            fflush(stdout);
        }

        printf("Kill Decoder\r\n");
        m_decoder->quit();
        delete m_decoder;
    }
}

void VideoEngine::changeDecoder(QString decoderName)
{
    QString currentSource = "";
    if(m_decoder != nullptr)
    {
        currentSource = m_decoder->source();
        if(m_decoder->isRunning())
            m_decoder->pause(true);
    }
    for(int i=0; i< m_decoderList.size(); i++)
    {
        if(m_decoderList[i]->decodeType() == decoderName)
        {
            m_decoder = m_decoderList[i];
            connect(m_decoder,&DecoderInterface::frameDecoded,
                    this,&VideoEngine::renderFrame);
            connect(m_decoder,&DecoderInterface::videoInformationChanged,
                    this,&VideoEngine::videoInformationChanged);
            connect(m_decoder,&DecoderInterface::metaDecoded,
                    this,&VideoEngine::metadataFound);
            connect(m_decoder,&DecoderInterface::locationComputed,
                    this,&VideoEngine::handleLocationComputed);
            if(decoderName.contains("MC"))
                m_decoder->setOffset(0,-4.5f,0);
            else
                m_decoder->setOffset(0.0f,0.0f,0.0f);

            if(currentSource != "") setVideo(currentSource);
            break;
        }
    }
}

void VideoEngine::setVideo(QString video)
{
    if(m_decoder != nullptr)
    {
        m_decoder->setVideo(video);
        m_decoder->startService();
    }
}

void VideoEngine::setRender(VideoRender* render)
{
    m_render = render;
}

void VideoEngine::pause(bool pause)
{
    m_decoder->pause(pause);
}

void VideoEngine::setSpeed(float speed)
{
    m_decoder->setSpeed(speed);
}

void VideoEngine::goToPosition(float percent)
{
    m_decoder->goToPosition(percent);
}

void VideoEngine::setSensorParams(float sx, float sy)
{
    m_decoder->setSensorParams(sx,sy);
    m_decoder->computeTargetLocation(m_xRatio,m_yRatio);
}

void VideoEngine::computeTargetLocation(float xRatio, float yRatio)
{
    m_xRatio = xRatio;
    m_yRatio = yRatio;
    m_decoder->computeTargetLocation(xRatio,yRatio);
}

int VideoEngine::getCurrentTime ()
{
    return m_decoder->getCurrentTime();
}

int VideoEngine::getDuration ()
{
    return m_decoder->getDuration();
}

void VideoEngine::renderFrame(unsigned char* frameData, int width, int height)
{
    if(m_render != nullptr)
    {
        m_frameSize.setWidth(width);
        m_frameSize.setHeight(height);
        m_render->handleNewFrame(frameData,width,height);        
    }
}

void VideoEngine::handleLocationComputed(QPoint point, QGeoCoordinate location)
{
    m_geoPoints.clear();
    appendGeoPoint(new GeoPoint(m_frameSize,point,location));
}
