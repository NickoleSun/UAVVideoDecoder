/**
 *========================================================================
 * Project: %{ProjectName}
 * Module: VideoEngine.h
 * Module Short Description:
 * Author: %{AuthorName}
 * Date: %{Date}
 * Organization: Viettel Aerospace Institude - Viettel Group
 * =======================================================================
 */


#ifndef VIDEOENGINE_H
#define VIDEOENGINE_H

#include <QObject>
#include <QVariantMap>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "Decoder/DecoderInterface.h"
#include "VideoDisplay/VideoRender.h"

class VideoEngine : public QObject
{
    Q_OBJECT
public:
    explicit VideoEngine(QObject *parent = nullptr);
    ~VideoEngine();
    Q_INVOKABLE void changeDecoder(QString decoderName);
    Q_INVOKABLE void setVideo(QString video);
    Q_INVOKABLE void setRender(VideoRender* render);
    Q_INVOKABLE void pause(bool pause);
    Q_INVOKABLE void setSpeed(float speed);
    Q_INVOKABLE void goToPosition(float percent);
    Q_INVOKABLE int getCurrentTime ();
    Q_INVOKABLE int getDuration ();

public Q_SLOTS:
    void renderFrame(unsigned char* frameData, int width, int height);    

Q_SIGNALS:
    void videoInformationChanged(QString information);
    void metadataFound(QVariantMap data);
private:
    DecoderInterface* m_decoder = nullptr;
    VideoRender* m_render = nullptr;
    QString m_decoderType = "FFMPEG";
};

#endif // VIDEOENGINE_H
