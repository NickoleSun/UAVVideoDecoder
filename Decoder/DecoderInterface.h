#ifndef DECODERINTERFACE_H
#define DECODERINTERFACE_H

#include <QObject>
#include <QThread>
#include <QMutex>
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
    int getCurrentTime () { return m_currentTimeMS; }
    int getDuration () { return m_durationMS; }

public Q_SLOTS:

Q_SIGNALS:
    void frameDecoded(unsigned char* frameData, int frameWidth, int frameHeight, int timestamp);
    void metaDecoded(QVariantMap data);
    void videoInformationChanged(QString information);

public:
    bool m_stop = false;
    QString m_source = "";
    float m_speed = 1.0f;
    float m_fps = 30;
    int m_durationMS = 0;
    int m_currentTimeMS = 0;
};
#endif // DECODERINTERFACE_H
