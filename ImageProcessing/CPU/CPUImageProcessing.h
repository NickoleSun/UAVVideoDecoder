#ifndef CPUIMAGEPROCESSING_H
#define CPUIMAGEPROCESSING_H

#include <QMutex>
#include <QWaitCondition>
#include "../ImageProcessingInterface.h"
#include "Detect/utils.hpp"

class CPUImageProcessing : public ImageProcessingInterface
{
    Q_OBJECT
public:
    explicit CPUImageProcessing(ImageProcessingInterface* parent = nullptr);
    void processFrame(unsigned char* frameData, int width, int height) override;

private:
    GSauvola* m_binor;
};

#endif // CPUIMAGEPROCESSING_H
