#include "VideoEngine.h"
#include "Decoder/Gstreamer/GSTStreamDecoder.h"
#include "Decoder/FFMpeg/FFMPEGDecoder.h"
#include "Decoder/MC/MCDecoder.h"
#include <unistd.h>
VideoEngine::VideoEngine(QObject *parent) : QObject(parent)
{
    m_decoder = new MCDecoder;
    connect(m_decoder,&DecoderInterface::frameDecoded,
            this,&VideoEngine::renderFrame);
    connect(m_decoder,&DecoderInterface::videoInformationChanged,
            this,&VideoEngine::videoInformationChanged);    
    connect(m_decoder,&DecoderInterface::metaDecoded,
            this,&VideoEngine::metadataFound);
}
VideoEngine::~VideoEngine()
{
    m_decoder->stop();
    while(m_decoder->isRunning())
    {
        printf(".");
        sleep(1);
        fflush(stdout);
    }

    printf("Kill Decoder\r\n");
    m_decoder->quit();
    delete m_decoder;
}

void VideoEngine::changeDecoder(QString decoderName)
{
    if(m_decoderType != decoderName)
    {
        m_decoderType = decoderName;
        if(m_decoderType == "FFMPEG")
        {
            m_decoder = new FFMPEGDecoder;
        }
        else if(m_decoderType == "GSTREAMER")
        {
            m_decoder = new GSTStreamDecoder;
        }
    }
}

void VideoEngine::setVideo(QString video)
{
    if(m_decoder != nullptr)
    {
        m_decoder->setVideo(video);
        m_decoder->start();
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
        m_render->handleNewFrame(frameData,width,height);
    }
}
