#ifndef IMAGEPROCESSINGINTERFACE_H
#define IMAGEPROCESSINGINTERFACE_H

#include <QThread>
#include <QList>

class ImageProcessingInterface : public QObject
{
    Q_OBJECT
public:
    explicit ImageProcessingInterface(QObject *parent = nullptr);
    virtual void processFrame(unsigned char* frameData, int width, int height) = 0;
};

#endif // IMAGEPROCESSINGINTERFACE_H
