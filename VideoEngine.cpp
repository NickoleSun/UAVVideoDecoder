#include "VideoEngine.h"
#include "Decoder/Gstreamer/GStreamDecoder.h"
#include "Decoder/FFMpeg/FFMPEGDecoder.h"
#ifdef SUPPORT_MC_DECODER
    #include "Decoder/MC/MCDecoder.h"
#endif
#include "ImageProcessing/CPU/CPUImageProcessing.h"
#include "ImageProcessing/CPU/Detect/utils.hpp"
#include "ImageProcessing/CPU/Tracker/mosse/tracker.h"
#define INIT_SIZE    64
VideoEngine::VideoEngine(QThread *parent) : QThread(parent)
{
#ifdef SUPPORT_MC_DECODER
    m_decoderList.append(new MCDecoder);
#endif
    m_decoderList.append(new FFMPEGDecoder);
    m_decoderList.append(new GStreamDecoder);
    Q_EMIT decoderListChanged();
    changeDecoder(m_decoderList[0]->decodeType());
    connect(this,&VideoEngine::frameProcessed,
            this,&VideoEngine::renderFrame);

    m_mutexClick = new QMutex;
    m_waitClick = new QWaitCondition;
}
VideoEngine::~VideoEngine()
{

    printf("Kill Video engine\r\n");
    this->stop();
    while(this->isRunning())
    {
        sleep(1);
        this->stop();
    }
    this->quit();
}

void VideoEngine::stop()
{
    printf("Stop VideoEngine\r\n");
    m_stop = true;
    m_mutexClick->lock();
    m_mutexClick->unlock();
    m_waitClick->notify_one();
    m_decoder->stop();
}
void VideoEngine::changeDecoder(QString decoderName)
{
    QString currentSource = "";
    if(m_processor == nullptr) m_processor = new CPUImageProcessing;
    if(m_decoder != nullptr)
    {
        currentSource = m_decoder->source();
        if(m_decoder->isRunning())
            m_decoder->pause(true);
        disconnect(m_decoder,&DecoderInterface::videoInformationChanged,
                this,&VideoEngine::videoInformationChanged);
        disconnect(m_decoder,&DecoderInterface::metaDecoded,
                this,&VideoEngine::metadataFound);
        disconnect(m_decoder,&DecoderInterface::locationComputed,
                this,&VideoEngine::handleLocationComputed);
    }
    for(int i=0; i< m_decoderList.size(); i++)
    {
        if(m_decoderList[i]->decodeType() == decoderName)
        {
            m_decoder = m_decoderList[i];
            connect(m_decoder,&DecoderInterface::videoInformationChanged,
                    this,&VideoEngine::videoInformationChanged);
            connect(m_decoder,&DecoderInterface::metaDecoded,
                    this,&VideoEngine::metadataFound);
            connect(m_decoder,&DecoderInterface::locationComputed,
                    this,&VideoEngine::handleLocationComputed);
            if(currentSource != "") setVideo(currentSource);
            if(!isRunning()) start();
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
    m_pauseSet = true;
    m_pause = pause;
    if(!m_pause) m_waitClick->wakeAll();
    playingChanged();
}

void VideoEngine::setSpeed(float speed)
{
    m_decoder->setSpeed(speed);
}

void VideoEngine::goToPosition(float percent)
{
    if(m_pause) pause(false);
    m_decoder->goToPosition(percent);
}

void VideoEngine::setSensorParams(float sx, float sy)
{
    m_decoder->setSensorParams(sx,sy);
}

void VideoEngine::computeTargetLocation(float xRatio, float yRatio)
{
    m_decoder->computeTargetLocation(xRatio,yRatio);
    m_mutexClick->lock();
    m_clickSet = true;
    m_xRatio = xRatio;
    m_yRatio = yRatio;
    m_mutexClick->unlock();
    m_waitClick->wakeAll();
}

int VideoEngine::getCurrentTime ()
{
    return m_decoder->getCurrentTime();
}

int VideoEngine::getDuration ()
{
    return m_decoder->getDuration();
}

void VideoEngine::setGimbalOffset(float offsetPan, float offsetTilt, float offsetRoll)
{
    m_decoder->setGimbalOffset(offsetPan,offsetTilt,offsetRoll);
}

void VideoEngine::setUavOffset(float offsetRoll, float offsetPitch, float offsetYaw)
{
    m_decoder->setUavOffset(offsetRoll,offsetPitch,offsetYaw);
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

void VideoEngine::run()
{
    unsigned char* frameData = new unsigned char[33177600];
    unsigned char* frameDataShow = new unsigned char[33177600];
    int width;
    int height;
    GSauvola *binor = new GSauvola();
    Tracker* tracker = new Tracker();
    int trackSize = 200;
    cv::Mat frameRGB, frameShow, frameGray, frameI420;
    while(!m_stop)
    {
        if(m_pauseSet)
        {
            m_pauseSet = false;
            m_decoder->pause(m_pause);
        }
        if(!m_pause)
        {
            m_decoder->capture(frameData,width,height);

            if(tracker->isInitialized())
            {
                frameGray = cv::Mat(height,width,CV_8UC1,frameData);
                tracker->performTrack(frameGray);
                frameI420 = cv::Mat (height*3/2,width,CV_8UC1,frameData);
                cv::cvtColor(frameI420,frameRGB,cv::COLOR_YUV2RGB_I420);
                cv::rectangle(frameRGB,tracker->getPosition(),cv::Scalar(0,255,0),2,cv::LINE_AA);
                cv::cvtColor(frameRGB,frameShow,cv::COLOR_RGB2YUV_I420);
                memcpy(frameDataShow,frameShow.data,width*height*3/2);
                Q_EMIT frameProcessed(frameDataShow,width,height);
            }
            else {
                Q_EMIT frameProcessed(frameData,width,height);
            }
            if(m_clickSet && !m_pause) pause(true);
        }
        else
        {
            m_mutexClick->lock();
            if(!m_clickSet)
                m_waitClick->wait(m_mutexClick);
            m_mutexClick->unlock();
            if(m_clickSet)
            {
                m_clickSet = false;
                int clickPointX = static_cast<int>((float)width * m_xRatio);
                int clickPointY = static_cast<int>((float)height * m_yRatio);
                if(tracker->isInitialized()) tracker->resetTrack();
                frameGray = cv::Mat(height,width,CV_8UC1,frameData);
                tracker->initTrack(frameGray,
                                   cv::Rect(clickPointX-trackSize/2,
                                            clickPointY-trackSize/2,
                                            trackSize,trackSize));
//                cv::Point clickPoint(clickPointX,clickPointY);
//                cv::Mat frameI420(height*3/2,width,CV_8UC1,frameData);
//                cv::Mat frameRGB, frameShow;
//                cv::cvtColor(frameI420,frameRGB,cv::COLOR_YUV2RGB_I420);
//                std::vector<cv::Rect> finalRect =
//                        binor->detectObject(frameRGB, clickPoint, cv::Size(INIT_SIZE, INIT_SIZE));
//                cv::circle(frameRGB, clickPoint, 2, cv::Scalar(255, 0, 0));
//                for(size_t i = 0; i < finalRect.size(); i++)
//                {
//                    cv::rectangle(frameRGB, finalRect[i], cv::Scalar(0, 0, 255));
//                    cv::putText(frameRGB, std::to_string(i), finalRect[i].br(), cv::FONT_HERSHEY_COMPLEX, 1.f, cv::Scalar(0, 0, 255));
//                }
//                cv::cvtColor(frameRGB,frameShow,cv::COLOR_RGB2YUV_I420);
//                Q_EMIT frameProcessed(frameShow.data,width,height);
            }
            pause(false);
        }
    }
    printf("VideoEngine stopped\r\n");
}
