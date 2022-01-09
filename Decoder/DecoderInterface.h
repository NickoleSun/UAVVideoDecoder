#ifndef DECODERINTERFACE_H
#define DECODERINTERFACE_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QPoint>
#include <QGeoCoordinate>
#include <QWaitCondition>
#include <QVariantMap>
#include "Algorithm/AP_TargetLocalization/TargetLocalization.h"

class DecoderInterface : public QThread
{
    Q_OBJECT
public:
    explicit DecoderInterface(QThread *parent = nullptr);
public:
    virtual void stop(){}
    virtual void pause(bool pause){ Q_UNUSED(pause); }
    virtual void setVideo(QString videoSource){ Q_UNUSED(videoSource); }
    virtual void setSpeed(float speed){ Q_UNUSED(speed); }
    virtual void goToPosition(float percent) { Q_UNUSED(percent); }

    void setOffset(float panOffset, float tiltOffset, float rollOffset)
    {   m_panOffset = panOffset;
        m_tiltOffset = tiltOffset;
        m_rollOffset = rollOffset;
        printf("Offset P[%f] T[%f] R[%f]\r\n",
               m_panOffset,
               m_tiltOffset,
               m_rollOffset);
    }
    void setSensorParams(float sx, float sy);
    void computeTargetLocation(float xRatio, float yRatio);
    void computeGeolocation();
    void startService()
    {
        start();
        std::thread computeTargetThead(&DecoderInterface::computeGeolocation,this);
        computeTargetThead.detach();
    }

    int getCurrentTime () { return m_currentTimeMS; }
    int getDuration () { return m_durationMS; }
    QString decodeType() { return m_type; }
    QString source() { return m_source; }
    void updateMeta(QVariantMap metaData);

public Q_SLOTS:

Q_SIGNALS:
    void frameDecoded(unsigned char* frameData, int frameWidth, int frameHeight, int timestamp);
    void metaDecoded(QVariantMap data);
    void videoInformationChanged(QString information);
    void locationComputed(QPoint point, QGeoCoordinate location);

protected:
    QString m_type = "";
    bool m_stop = false;
    QString m_source = "";
    float m_speed = 1.0f;
    float m_fps = 30;
    int m_durationMS = 0;
    int m_currentTimeMS = 0;

    QVariantMap m_metaProcessed;
    int m_width = 1920;
    int m_height = 1080;
    float m_sx = 33;
    float m_sy = 25;

    // Offset
    float m_panOffset = 0.0f;
    float m_tiltOffset = 0.0f;
    float m_rollOffset = 0.0f;

    //Geo-location
    AppService::TargetLocalization  m_geolocation;
    AppService::TargetLocalization  m_geolocationSingle;
    bool                            m_computeTargetSet = false;
    float                           m_xRatio = 0.5f;
    float                           m_yRatio = 0.5f;
    std::mutex m_computeTargetMutex;
    std::condition_variable m_waitComputeTargetCond;
};
#endif // DECODERINTERFACE_H
