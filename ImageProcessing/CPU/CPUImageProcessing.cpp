#include "CPUImageProcessing.h"
CPUImageProcessing::CPUImageProcessing(ImageProcessingInterface* parent)  : ImageProcessingInterface(parent)
{
    m_binor = new GSauvola();
}

void CPUImageProcessing::processFrame(unsigned char* frameData, int width, int height)
{

    cv::Mat frameI420(height,width,CV_8UC1,frameData);

//    std::vector<cv::Rect> finalRect =
//            m_binor->detectObject(grayFrame, clickPoint, cv::Size(INIT_SIZE, INIT_SIZE));
}
