#include "DecoderInterface.h"
#include <locale.h>
DecoderInterface::DecoderInterface(QThread *parent) : QThread(parent)
{
    setlocale(LC_NUMERIC, "French_Canada.1252");
    m_geolocation.visionViewInit(m_sx,m_sy,m_width,m_height);
    m_geolocation.setParams("/home/hainh/uavMap/elevation/ElevationData-H1",60);
    m_geolocationSingle.visionViewInit(m_sx,m_sy,m_width,m_height);
    m_geolocationSingle.setParams("/home/hainh/uavMap/elevation/ElevationData-H1",60);
    connect(this,&DecoderInterface::frameDecoded,
            this,&DecoderInterface::handleFrameDecoded);
    m_mutexCaptureFrame = new QMutex;
    m_waitCaptureFrame = new QWaitCondition;
}

bool DecoderInterface::capture(unsigned char* frameData, int& frameWidth, int& frameHeight)
{
    pause(false);
    m_mutexCaptureFrame->lock();
    m_newFrameDecoded = false;
    if(!m_newFrameDecoded) m_waitCaptureFrame->wait(m_mutexCaptureFrame);
    m_mutexCaptureFrame->unlock();
    pause(true);
    if(m_stop) return false;
    memcpy(frameData,m_frameData,3*m_width*m_height/2);
    frameWidth = m_width;
    frameHeight = m_height;
    return true;
}

void DecoderInterface::handleFrameDecoded(unsigned char* frameData, int frameWidth, int frameHeight, int timestamp)
{
    m_mutexCaptureFrame->lock();
    m_newFrameDecoded = true;
    m_mutexCaptureFrame->unlock();
    m_waitCaptureFrame->wakeAll();
    m_frameData = frameData;
    if(m_width != frameWidth || m_height != frameHeight)
    {
        m_frameSizeSet = true;
        m_waitComputeTargetCond.notify_one();
    }
    m_width = frameWidth;
    m_height = frameHeight;

}

void DecoderInterface::setGimbalOffset(float offsetPan, float offsetTilt, float offsetRoll)
{
    m_gimbalPanOffset = offsetPan;
    m_gimbalTiltOffset = offsetTilt;
    m_gimbalRollOffset = offsetRoll;
    m_changeGimbalOffsetSet = true;
    m_waitComputeTargetCond.notify_one();
}

void DecoderInterface::setUavOffset(float offsetRoll, float offsetPitch, float offsetYaw)
{
    m_uavRollOffset = offsetRoll;
    m_uavPitchOffset = offsetPitch;
    m_uavYawOffset = offsetYaw;
    m_changeUAVOffsetSet = true;
    m_waitComputeTargetCond.notify_one();
}
void DecoderInterface::setSensorParams(float sx, float sy)
{
    m_sx = sx;
    m_sy = sy;
    m_changeSensorParamSet = true;
    m_waitComputeTargetCond.notify_one();
}

void DecoderInterface::computeTargetLocation(float xRatio, float yRatio)
{
    m_xRatio = xRatio;
    m_yRatio = yRatio;
    m_computeTargetSet = true;
    m_waitComputeTargetCond.notify_one();
}

void DecoderInterface::computeGeolocation()
{
    while(!m_stop)
    {
        std::unique_lock<std::mutex> lock(m_computeTargetMutex);
        m_waitComputeTargetCond.wait(lock, [this]() {
            return this->m_computeTargetSet ||
                    this->m_changeSensorParamSet ||
                    this->m_changeGimbalOffsetSet ||
                    this->m_changeUAVOffsetSet ||
                    this->m_frameSizeSet; });

        if(m_computeTargetSet)
        {
            m_computeTargetSet = false;
        }
        if(m_frameSizeSet)
        {
            m_frameSizeSet = false;
            m_geolocation.visionViewInit(m_sx,m_sy,m_width,m_height);
            m_geolocationSingle.visionViewInit(m_sx,m_sy,m_width,m_height);
        }
        if(m_changeSensorParamSet)
        {
            m_changeSensorParamSet = false;
            m_geolocation.visionViewInit(m_sx,m_sy,m_width,m_height);
            m_geolocationSingle.visionViewInit(m_sx,m_sy,m_width,m_height);
        }
        if(m_changeGimbalOffsetSet)
        {
            m_changeGimbalOffsetSet = false;
        }

        if(m_changeUAVOffsetSet)
        {
            m_changeUAVOffsetSet = false;
        }

        int imageX = static_cast<int>(m_xRatio * static_cast<float>(m_width));
        int imageY = static_cast<int>(m_yRatio * static_cast<float>(m_height));
        if(m_metaProcessed.keys().contains("Hfov"))
        {
            QVariantMap metaData = m_metaProcessed;
            m_geolocationSingle.targetLocationMain(
                            imageX,imageY,
                            metaData["Hfov"].toFloat() / RAD_2_DEG,
                            (metaData["UavRoll"].toFloat() + m_uavRollOffset)/ RAD_2_DEG,
                            (metaData["UavPitch"].toFloat() + m_uavPitchOffset)/ RAD_2_DEG,
                            (metaData["UavYaw"].toFloat() + m_uavYawOffset)/ RAD_2_DEG,
                            (metaData["GimbalPan"].toFloat() + m_gimbalPanOffset)/ RAD_2_DEG,
                            (metaData["GimbalTilt"].toFloat() + m_gimbalTiltOffset)/ RAD_2_DEG,
                            (metaData["GimbalRoll"].toFloat() + m_gimbalRollOffset)/ RAD_2_DEG,
                            metaData["UavLatitude"].toDouble(),
                            metaData["UavLongitude"].toDouble(),
                            metaData["UavAMSL"].toFloat());
            Q_EMIT locationComputed(
                        QPoint(imageX,imageY),
                        QGeoCoordinate(
                                        m_geolocationSingle.getTargetLat(),
                                        m_geolocationSingle.getTargetLon(),
                                        m_geolocationSingle.getTargetAlt()));
            updateMeta(metaData);
            Q_EMIT metaDecoded(m_metaProcessed);
        }
    }
}

void DecoderInterface::startService()
{
    start();
    std::thread computeTargetThead(&DecoderInterface::computeGeolocation,this);
    computeTargetThead.detach();
}

void DecoderInterface::updateMeta(QVariantMap metaData)
{
    char data[2048];
    sprintf(data,
            "%.2f;%.2f;%.2f;"
            "%.6f;%.6f;%.2f;"
            "%.2f;%.2f;"
            "%.2f;%.2f;%.2f;"
            "%.2f;"
            "%.6f;%.6f;%.2f;"
            "%.6f;%.6f;"
            "%.6f;%.6f;"
            "%.6f;%.6f;"
            "%.6f;%.6f\r\n",
            metaData["UavYaw"].toFloat(), metaData["UavPitch"].toFloat(), metaData["UavRoll"].toFloat(),
            metaData["UavLatitude"].toFloat(), metaData["UavLongitude"].toFloat(), metaData["UavAMSL"].toFloat(),
            metaData["Hfov"].toFloat(), metaData["Vfov"].toFloat(),
            metaData["GimbalPan"].toFloat(), metaData["GimbalTilt"].toFloat(), metaData["GimbalRoll"].toFloat(),
            metaData["TargetSLR"].toFloat(),
            metaData["CenterLat"].toFloat(), metaData["CenterLon"].toFloat(),
            metaData["CenterAMSL"].toFloat(),
            metaData["Corner01Lat"].toFloat(),
            metaData["Corner01Lon"].toFloat(),
            metaData["Corner02Lat"].toFloat(),
            metaData["Corner02Lon"].toFloat(),
            metaData["Corner03Lat"].toFloat(),
            metaData["Corner03Lon"].toFloat(),
            metaData["Corner04Lat"].toFloat(),
            metaData["Corner04Lon"].toFloat()
            );
    printf("%s",QString(data).replace(',','.').toStdString().c_str());
    m_metaProcessed = metaData;
    m_geolocation.targetLocationMain(
                    m_width/2, m_height/2,
                    metaData["Hfov"].toFloat() / RAD_2_DEG,
                    (metaData["UavRoll"].toFloat() + m_uavRollOffset)/ RAD_2_DEG,
                    (metaData["UavPitch"].toFloat() + m_uavPitchOffset)/ RAD_2_DEG,
                    (metaData["UavYaw"].toFloat() + m_uavYawOffset)/ RAD_2_DEG,
                    (metaData["GimbalPan"].toFloat() + m_gimbalPanOffset)/ RAD_2_DEG,
                    (metaData["GimbalTilt"].toFloat() + m_gimbalTiltOffset)/ RAD_2_DEG,
                    (metaData["GimbalRoll"].toFloat() + m_gimbalRollOffset)/ RAD_2_DEG,
                    metaData["UavLatitude"].toDouble(),
                    metaData["UavLongitude"].toDouble(),
                    metaData["UavAMSL"].toFloat());

    m_metaProcessed["centerCoordinateComp"] = QVariant::fromValue(
                QGeoCoordinate( m_geolocation.getTargetLat(),
                                m_geolocation.getTargetLon()));
    m_geolocation.targetLocationMain(
                    metaData["TargetPx"].toInt(), metaData["TargetPy"].toInt(),
                    metaData["Hfov"].toFloat() / RAD_2_DEG,
                    (metaData["UavRoll"].toFloat() + m_uavRollOffset)/ RAD_2_DEG,
                    (metaData["UavPitch"].toFloat() + m_uavPitchOffset)/ RAD_2_DEG,
                    (metaData["UavYaw"].toFloat() + m_uavYawOffset)/ RAD_2_DEG,
                    (metaData["GimbalPan"].toFloat() + m_gimbalPanOffset)/ RAD_2_DEG,
                    (metaData["GimbalTilt"].toFloat() + m_gimbalTiltOffset)/ RAD_2_DEG,
                    (metaData["GimbalRoll"].toFloat() + m_gimbalRollOffset)/ RAD_2_DEG,
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
