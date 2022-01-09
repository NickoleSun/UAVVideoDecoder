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
#include <QList>
#include <QQmlListProperty>
#include "Decoder/DecoderInterface.h"
#include "VideoDisplay/VideoRender.h"

class GeoPoint : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QSize size READ size WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(QPoint point READ point WRITE setPoint NOTIFY pointChanged)
    Q_PROPERTY(QGeoCoordinate gps READ gps WRITE setGPS NOTIFY gpsChanged)
public:
    GeoPoint(QSize size, QPoint point, QGeoCoordinate gps):
        m_size(size),m_point(point),m_gps(gps)
    {}
    ~GeoPoint(){}
    QSize size() { return m_size; }
    void setSize(QSize size) { m_size = size; Q_EMIT sizeChanged(); }
    QPoint point() { return m_point; }
    void setPoint(QPoint point) { m_point = point; Q_EMIT pointChanged(); }
    QGeoCoordinate gps() { return m_gps; }
    void setGPS(QGeoCoordinate gps) { gps = m_gps; Q_EMIT gpsChanged(); }

Q_SIGNALS:
    void sizeChanged();
    void pointChanged();
    void gpsChanged();

private:
    QSize           m_size;
    QPoint          m_point;
    QGeoCoordinate  m_gps;
};

class VideoEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList decoderList READ decoderList NOTIFY decoderListChanged)
    Q_PROPERTY(QQmlListProperty<GeoPoint> geoPoints READ geoPoints NOTIFY geoPointsChanged)
public:
    explicit VideoEngine(QObject *parent = nullptr);
    ~VideoEngine();
    static void expose(){
        qmlRegisterType<GeoPoint>();
        qmlRegisterType<VideoRender>("viettel.dev", 1, 0, "VideoRender");
        qmlRegisterType<VideoEngine>("viettel.dev", 1, 0, "VideoEngine");
    }
    Q_INVOKABLE void changeDecoder(QString decoderName);
    Q_INVOKABLE void setVideo(QString video);
    Q_INVOKABLE void setRender(VideoRender* render);
    Q_INVOKABLE void pause(bool pause);
    Q_INVOKABLE void setSpeed(float speed);
    Q_INVOKABLE void goToPosition(float percent);
    Q_INVOKABLE void setSensorParams(float sx, float sy);
    Q_INVOKABLE void computeTargetLocation(float xRatio, float yRatio);
    Q_INVOKABLE int getCurrentTime ();
    Q_INVOKABLE int getDuration ();

    QQmlListProperty<GeoPoint> geoPoints(){
        return QQmlListProperty<GeoPoint>(this, m_geoPoints);
    }
    void appendGeoPoint(GeoPoint* point) {
        m_geoPoints.append(point);
        Q_EMIT geoPointsChanged();
    }
    int geoPointCount() const{return m_geoPoints.count();}
    GeoPoint* geoPoint(int index) const{ return m_geoPoints.at(index);}
    Q_INVOKABLE void removeGeoPoint(int index) {
        if(index >=0 && index < m_geoPoints.size())
        {
            m_geoPoints.removeAt(index);
            Q_EMIT geoPointsChanged();
        }
    }
    void clearGeoPoints() {
        m_geoPoints.clear();
        Q_EMIT geoPointsChanged();
    }

    QStringList decoderList()
    {
        QStringList res;
        for(int i =0; i< m_decoderList.size(); i++)
        {
            res.append(m_decoderList[i]->decodeType());
        }
        return res;
    }

public Q_SLOTS:
    void renderFrame(unsigned char* frameData, int width, int height);    
    void handleLocationComputed(QPoint point, QGeoCoordinate location);

Q_SIGNALS:
    void videoInformationChanged(QString information);
    void metadataFound(QVariantMap data);
    void geoPointsChanged();
    void decoderListChanged();

private:
    DecoderInterface* m_decoder = nullptr;
    QList<DecoderInterface*> m_decoderList;
    VideoRender* m_render = nullptr;
    QList<GeoPoint*> m_geoPoints;
    QSize m_frameSize;
    float m_xRatio;
    float m_yRatio;
};

#endif // VIDEOENGINE_H
