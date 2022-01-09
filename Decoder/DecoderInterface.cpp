#include "DecoderInterface.h"

DecoderInterface::DecoderInterface(QThread *parent) : QThread(parent)
{
    m_geolocation.visionViewInit(m_sx,m_sy,m_width,m_height);
    m_geolocation.setParams("/home/hainh/uavMap/elevation/ElevationData-H1",60);
    m_geolocationSingle.visionViewInit(m_sx,m_sy,m_width,m_height);
    m_geolocationSingle.setParams("/home/hainh/uavMap/elevation/ElevationData-H1",60);
}

void DecoderInterface::setSensorParams(float sx, float sy)
{
    m_sx = sx;
    m_sy = sy;
    m_geolocation.visionViewInit(sx,sy,m_width,m_height);
    m_geolocationSingle.visionViewInit(sx,sy,m_width,m_height);
}

void DecoderInterface::computeTargetLocation(float xRatio, float yRatio)
{
    m_xRatio = xRatio;
    m_yRatio = yRatio;
    m_computeTargetSet = true;
    m_waitComputeTargetCond.notify_all();
}

void DecoderInterface::computeGeolocation()
{
    while(!m_stop)
    {
        std::unique_lock<std::mutex> lock(m_computeTargetMutex);
        m_waitComputeTargetCond.wait(lock, [this]() { return this->m_computeTargetSet; });
        int imageX = static_cast<int>(m_xRatio * static_cast<float>(m_width));
        int imageY = static_cast<int>(m_yRatio * static_cast<float>(m_height));
        m_geolocationSingle.targetLocationMain(
                        imageX,imageY,
                        m_metaProcessed["Hfov"].toFloat() / RAD_2_DEG,
                        m_metaProcessed["UavRoll"].toFloat()/ RAD_2_DEG,
                        m_metaProcessed["UavPitch"].toFloat()/ RAD_2_DEG,
                        m_metaProcessed["UavYaw"].toFloat()/ RAD_2_DEG,
                        (m_metaProcessed["GimbalPan"].toFloat() + m_panOffset)/ RAD_2_DEG,
                        (m_metaProcessed["GimbalTilt"].toFloat() + m_tiltOffset)/ RAD_2_DEG,
                        (m_metaProcessed["GimbalRoll"].toFloat() + m_rollOffset)/ RAD_2_DEG,
                        m_metaProcessed["UavLatitude"].toDouble(),
                        m_metaProcessed["UavLongitude"].toDouble(),
                        m_metaProcessed["UavAMSL"].toFloat());
        Q_EMIT locationComputed(
                    QPoint(imageX,imageY),
                    QGeoCoordinate(
                                    m_geolocationSingle.getTargetLat(),
                                    m_geolocationSingle.getTargetLon(),
                                    m_geolocationSingle.getTargetAlt()));
        m_computeTargetSet = false;
    }
}

void DecoderInterface::updateMeta(QVariantMap metaData)
{
    m_metaProcessed = metaData;
    m_geolocation.targetLocationMain(
                    m_width/2, m_height/2,
                    metaData["Hfov"].toFloat() / RAD_2_DEG,
                    metaData["UavRoll"].toFloat()/ RAD_2_DEG,
                    metaData["UavPitch"].toFloat()/ RAD_2_DEG,
                    metaData["UavYaw"].toFloat()/ RAD_2_DEG,
                    (metaData["GimbalPan"].toFloat() + m_panOffset)/ RAD_2_DEG,
                    (metaData["GimbalTilt"].toFloat() + m_tiltOffset)/ RAD_2_DEG,
                    (metaData["GimbalRoll"].toFloat() + m_rollOffset)/ RAD_2_DEG,
                    metaData["UavLatitude"].toDouble(),
                    metaData["UavLongitude"].toDouble(),
                    metaData["UavAMSL"].toFloat());

    m_metaProcessed["centerCoordinateComp"] = QVariant::fromValue(
                QGeoCoordinate( m_geolocation.getTargetLat(),
                                m_geolocation.getTargetLon()));
    m_geolocation.targetLocationMain(
                    metaData["TargetPx"].toInt(), metaData["TargetPy"].toInt(),
                    metaData["Hfov"].toFloat() / RAD_2_DEG,
                    metaData["UavRoll"].toFloat()/ RAD_2_DEG,
                    metaData["UavPitch"].toFloat()/ RAD_2_DEG,
                    metaData["UavYaw"].toFloat()/ RAD_2_DEG,
                    (metaData["GimbalPan"].toFloat() + m_panOffset)/ RAD_2_DEG,
                    (metaData["GimbalTilt"].toFloat() + m_tiltOffset)/ RAD_2_DEG,
                    (metaData["GimbalRoll"].toFloat() + m_rollOffset)/ RAD_2_DEG,
                    metaData["UavLatitude"].toDouble(),
                    metaData["UavLongitude"].toDouble(),
                    metaData["UavAMSL"].toFloat());
    m_metaProcessed["targetCoordinateComp"] = QVariant::fromValue(
                QGeoCoordinate( m_geolocation.getTargetLat(),
                                m_geolocation.getTargetLon()));
    m_metaProcessed["coordinate"] = QVariant::fromValue(
                QGeoCoordinate( metaData["UavLatitude"].toDouble(),
                                metaData["UavLongitude"].toDouble()));
    m_metaProcessed["azimuth"] = metaData["Azimuth"];
    m_metaProcessed["elevation"] = metaData["Elevation"];

    m_metaProcessed["azimuthComp"] = QVariant::fromValue(m_geolocation.getTargetAzimuth());
    m_metaProcessed["elevationComp"] = QVariant::fromValue(m_geolocation.getTargetElevator());

    m_metaProcessed["yaw"] = metaData["UavYaw"].toDouble();
    m_metaProcessed["centerCoordinate"] = QVariant::fromValue(
                QGeoCoordinate( metaData["CenterLat"].toDouble(),
                                metaData["CenterLon"].toDouble()));
    m_metaProcessed["targetCoordinate"] = QVariant::fromValue(
                QGeoCoordinate( metaData["TargetLat"].toDouble(),
                                metaData["TargetLon"].toDouble()));

#ifdef DEBUG
    printf("UAV(%f,%f,%.02f) R(%.02f) P(%.02f) Y(%.02f) T(%f,%f)\r\n",
            metaData["UavLatitude"].toDouble(),
            metaData["UavLongitude"].toDouble(),
            metaData["UavAMSL"].toDouble(),
            metaData["UavRoll"].toDouble(),
            metaData["UavPitch"].toDouble(),
            metaData["UavYaw"].toDouble(),
            metaData["TargetLat"].toDouble(),
            metaData["TargetLon"].toDouble());
    printf("[%d] AL/AC [%.04f]/[%.04f] EL/EC [%.04f]/[%.04f]\r\n",
            metaData["ImageId"].toInt(),
            m_metaProcessed["azimuth"].toFloat(),m_geolocation.getTargetAzimuth(),
            m_metaProcessed["elevation"].toFloat(),m_geolocation.getTargetElevator());
#endif
}
