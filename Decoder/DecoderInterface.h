#ifndef DECODERINTERFACE_H
#define DECODERINTERFACE_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QPoint>
#include <QGeoCoordinate>
#include <QWaitCondition>
#include <QVariantMap>

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
    virtual void setSensorParams(float sx, float sy) { Q_UNUSED(sx); Q_UNUSED(sy); }
    virtual void computeTargetLocation(float xRatio, float yRatio){ Q_UNUSED(xRatio); Q_UNUSED(yRatio); }
    void setOffset(float panOffset, float tiltOffset, float rollOffset)
    {   m_panOffset = panOffset;
        m_tiltOffset = tiltOffset;
        m_rollOffset = rollOffset;}
    int getCurrentTime () { return m_currentTimeMS; }
    int getDuration () { return m_durationMS; }

public Q_SLOTS:

Q_SIGNALS:
    void frameDecoded(unsigned char* frameData, int frameWidth, int frameHeight, int timestamp);
    void metaDecoded(QVariantMap data);
    void videoInformationChanged(QString information);
    void locationComputed(QPoint point, QGeoCoordinate location);

protected:
    bool m_stop = false;
    QString m_source = "";
    float m_speed = 1.0f;
    float m_fps = 30;
    int m_durationMS = 0;
    int m_currentTimeMS = 0;

    // Offset
    float m_panOffset = 0.0f;
    float m_tiltOffset = 0.0f;
    float m_rollOffset = 0.0f;
};
#endif // DECODERINTERFACE_H
