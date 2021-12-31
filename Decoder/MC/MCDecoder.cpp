#include "MCDecoder.h"
#include <QFileInfo>
#include <QTextStream>
#include <zbar.h>
#include <fstream>
#include <unistd.h>
#include <zbar/ImageScanner.h>

MCDecoder::MCDecoder(DecoderInterface *parent) : DecoderInterface(parent)
{
    // initialize MC library
    av_register_all();
    avformat_network_init();
    av_register_all();
    av_log_set_level(AV_LOG_QUIET);
    m_mutex = new QMutex;
    m_pauseCond = new QWaitCondition;
    m_geolocation.visionViewInit(33,25,1920,1080);
    m_geolocation.setParams("/home/hainh/uavMap/elevation/ElevationData-H1",60);
    m_geolocationSingle.visionViewInit(33,25,1920,1080);
    m_geolocationSingle.setParams("/home/hainh/uavMap/elevation/ElevationData-H1",60);
}
MCDecoder::~MCDecoder(){
    stop();
    delete m_pauseCond;
}

bool MCDecoder::openSource(QString videoSource)
{
    if(m_readFileCtx != nullptr)
        avformat_close_input(&m_readFileCtx);
    // Just decode
    int ret;
    // open input file context
    ret = avformat_open_input(&m_readFileCtx, videoSource.toStdString().c_str(), nullptr, nullptr);
    if (ret < 0) {
        printf("Fail to avformat_open_input\r\n");
        return false;
    }
    // retrive input stream information
    ret = avformat_find_stream_info(m_readFileCtx, nullptr);
    if (ret < 0) {
        printf("Fail to avformat_find_stream_info\r\n");
        return false;
    }

    // find primary video stream

    for (int i = 0; i < m_readFileCtx->nb_streams; i++) {
        if (m_readFileCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_videoStreamIndex = i;
        }
    }
    ret = av_find_best_stream(m_readFileCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &m_videoCodec, 0);
    if (ret < 0) {
        printf("Fail to av_find_best_stream: ret=%d\r\n",ret);
        return false;
    }

    m_videoStream = m_readFileCtx->streams[m_videoStreamIndex];
    // open video decoder context
    ret = avcodec_open2(m_videoStream->codec, m_videoCodec, nullptr);
    if (ret < 0) {
        printf("Fail to avcodec_open2: ret=%d\r\n",ret);
        return false;
    }

    m_fps = (double)m_videoStream->r_frame_rate.num / (double)m_videoStream->r_frame_rate.den;;
    m_durationMS = m_readFileCtx->duration/1000;
    // print input video stream informataion
    QString information;
    information +="Format: "+QString::fromStdString(std::string(m_readFileCtx->iformat->name))+"\n";
    information +="Length: "+QString::fromStdString(std::to_string(static_cast<int>(m_durationMS)))+"\n";
    information +="VCodec: "+QString::fromStdString(std::string(m_videoCodec->name))+"\n";
    information +="Size:   "+QString::fromStdString(std::to_string(m_videoStream->codec->width)+"x"+
                                                    std::to_string(m_videoStream->codec->height))+"\n";
    information +="FPS:    "+QString::fromStdString(std::to_string(m_fps));
#ifdef DEBUG
    printf("format: %s\r\n"     ,m_readFileCtx->iformat->name);
    printf("duration: %ld\r\n"  ,m_durationMS);
    printf("vcodec: %s\r\n"     ,m_videoCodec->name);
    printf("size:   %dx%d\r\n"  ,m_videoStream->codec->width,m_videoStream->codec->height);
    printf("fps:    %f\r\n"     ,m_fps);
    printf("pixfmt: %s\r\n"     ,av_get_pix_fmt_name(m_videoStream->codec->pix_fmt));
#endif

    Q_EMIT videoInformationChanged(information);

    return true;
}

void MCDecoder::run(){
    std::thread metaReadingThead(&MCDecoder::decodeMeta,this);
    metaReadingThead.detach();

    std::thread computeTargetThead(&MCDecoder::computeGeolocation,this);
    computeTargetThead.detach();
    // decoding loop
    AVFrame* decframe = av_frame_alloc();
    AVPacket pkt;
    while (!m_stop){
        m_mutex->lock();
        if(m_pause)
            m_pauseCond->wait(m_mutex); // in this place, your thread will stop to execute until someone calls resume
        m_mutex->unlock();
        if(m_sourceSet) {
            m_sourceSet = false;
            if(!openSource(m_source))
            {
                m_frameId = -1;
                pause(true);
                continue;
            }
        }
        if(m_posSeekSet){
            double timeBase = av_q2d(m_readFileCtx->streams[m_videoStreamIndex]->time_base);
            int64_t rel = m_durationMS * (m_posSeek * 1000) / (timeBase*1000000);
            int64_t startTS = m_readFileCtx->streams[m_videoStreamIndex]->start_time;
            int64_t seek_target = startTS + rel;
            avformat_seek_file(m_readFileCtx, m_videoStreamIndex,
                               seek_target, seek_target, seek_target,
                               AVSEEK_FLAG_FRAME);
            m_posSeekSet = false;
            continue;
        }
        if(av_read_frame(m_readFileCtx, &pkt) < 0)
        {
            printf("Read frame failed\r\n");
            pause(true);
            continue;
        }
        if(pkt.stream_index == m_videoStreamIndex){
            int got_pic = 0;
            avcodec_decode_video2(m_videoStream->codec, decframe, &got_pic, &pkt);
            if (got_pic != 0){
                int64_t currentTS = pkt.pts - m_readFileCtx->streams[m_videoStreamIndex]->start_time;
                double timeBase = av_q2d(m_readFileCtx->streams[m_videoStreamIndex]->time_base);
                int currentTimeMS = static_cast<int>(static_cast<double>(currentTS) * timeBase * 1000);
                if(currentTimeMS > 0)
                {
                    m_currentTimeMS = currentTimeMS;
                    //                    printf("timeBase[%f] currentTS[%ld] pkt.pts[%ld] pkt.dts[%ld] pkt.duration[%d]\r\n",
                    //                           timeBase,currentTS,pkt.pts,pkt.dts,pkt.duration);
                }
                int width = decframe->linesize[0];
                int height = decframe->height;
                memcpy(m_data,decframe->data[0],static_cast<size_t>(width*height));
                memcpy(m_data+width*height,decframe->data[1],static_cast<size_t>(width*height/4));
                memcpy(m_data+width*height*5/4,decframe->data[2],static_cast<size_t>(width*height/4));

                int frameID = decodeFrameID(m_data,200,1);
#ifdef DEBUG
                printf("frameID[%d]\r\n",frameID);
#endif
                if(frameID < 0)
                {
                    msleep(static_cast<unsigned long>(1000.0f / (m_fps * m_speed)));
                    Q_EMIT frameDecoded(m_data,width,height,m_currentTimeMS);
                }
                else {
                    m_frameId = frameID;
                    if(m_metaId < m_frameId)
                    {
                        // waitMeta
                        findMeta(m_frameId);
                        if(m_metaId >= m_frameId)
                        {
                            Q_EMIT metaDecoded(m_metaProcessed);
                        }
                    }
                    msleep(static_cast<unsigned long>(1000.0f / (m_fps * m_speed)));
                    Q_EMIT frameDecoded(m_data,width,height,m_currentTimeMS);
                }

            }
        }
        av_free_packet(&pkt);
    }
    av_frame_free(&decframe);
    if(m_videoStream != nullptr)
        avcodec_close(m_videoStream->codec);
    if(m_readFileCtx != nullptr)
        avformat_close_input(&m_readFileCtx);
    printf("MCDecoder stopped\r\n");
}

void MCDecoder::findMeta(int metaID)
{
    m_metaFound = false;
    m_waitToReadMetaCond.notify_all();
    std::unique_lock<std::mutex> lock(m_metaFoundMutex);
    m_waitToReadMetaCond.wait(lock, [this]() { return this->m_metaFound; });
}

void MCDecoder::setVideo(QString source){
    m_source = source;
    m_sourceSet = true;
    m_metaSource = source + ".csv";
    m_metaSourceSet = true;
    pause(false);
}

void MCDecoder::pause(bool pause){
    if(pause == true){
        m_mutex->lock();
        m_pause = true;
        m_mutex->unlock();
    }else{
        m_mutex->lock();
        m_pause = false;
        m_mutex->unlock();
        m_pauseCond->wakeAll();
    }
}

void MCDecoder::setSensorParams(float sx, float sy)
{
    m_geolocation.setSensorParams(sx,sy);
    m_geolocationSingle.setSensorParams(sx,sy);
}

void MCDecoder::goToPosition(float percent){
    m_posSeek = percent;
    m_posSeekSet = true;
    pause(false);
}

void MCDecoder::setSpeed(float speed){
    m_speed = speed;
}

void MCDecoder::stop(){
    m_metaFound = true;
    m_waitToReadMetaCond.notify_all();
    m_stop = true;
    pause(false);
}



void MCDecoder::decodeMeta()
{
    printf("metaSource[%s]\r\n",m_metaSource.toStdString().c_str());
    std::ifstream fread;
    fread.open(m_metaSource.toStdString().c_str());
    if (fread.is_open())
    {
        std::string line;

        // Read header line contains meta data fields.
        std::getline(fread, line);
        QStringList metaFields = QString::fromStdString(line).split(",");

        while(!m_stop)
        {
            m_mutex->lock();
            if(m_pause)
                m_pauseCond->wait(m_mutex);
            m_mutex->unlock();
            if(m_metaSourceSet)
            {
                m_metaSourceSet = false;
                if(fread.is_open())
                {
                    fread.close();
                    fread.open(m_metaSource.toStdString().c_str());
                    std::getline(fread, line);
                    QStringList metaFields = QString::fromStdString(line).split(",");
                }
            }
            if(this->m_metaId >= this->m_frameId)
            {
                if(m_metaId > m_frameId + 90)
                {
                    // go to begin of the file
                    fread.seekg(0);
                    std::getline(fread, line);
                }
                else
                {
                    m_metaFound = true;
                    m_waitToReadMetaCond.notify_all();
                    msleep(30);
                    continue;
                }
            }
            else
            {
//            std::unique_lock<std::mutex> lock(m_metaFoundMutex);
//            m_waitToReadMetaCond.wait(lock, [this]() { return !this->m_metaFound; });
//            msleep(1);
            }
            if (std::getline(fread, line))
            {
                QStringList metaValues = QString::fromStdString(line).split(",");

                if (metaFields.size() == metaValues.size())
                {
                    for (int i = 0; i < metaFields.size(); ++i)
                    {
                        m_meta[metaFields[i].replace(" ","")] = metaValues[i].replace(" ","");
                    }
                    m_metaId = m_meta["ImageId"].toInt();
#ifdef DEBUG
                    printf("m_metaId[%d]\r\n",m_metaId);
#endif
                    if(m_metaId < m_frameId)
                    {
                        // continue to find meta
                        continue;
                    }
                    else
                    {
                        updateMeta(m_meta);
                    }

                }
            }
            else
            {
                printf("EOF\r\n");
                sleep(1);
            }

        }
        fread.close();
    }
    else
    {

    }
    printf("Meta stopped\r\n");
}

int MCDecoder::decodeFrameID(unsigned char* frameData, int width, int height)
{
    int res = -1;
    // Create zbar scanner
    zbar::ImageScanner scanner;

    // Configure scanner
    scanner.set_config(zbar::ZBAR_CODE128, zbar::ZBAR_CFG_ENABLE, 1);

    // Wrap image data in a zbar image
    zbar::Image image(width, height, "Y800", frameData, width * height);

    // Scan the image for barcodes and QRCodes
    int n = scanner.scan(image);
    // Print results
    for (zbar::Image::SymbolIterator symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol)
    {
        res = std::atoi(symbol->get_data().c_str());
        break;
    }
    return res;
}

void MCDecoder::updateMeta(QVariantMap metaData)
{
    float tiltOffset = -7.0f;

    m_geolocation.targetLocationMain(
                    1920/2, 1080/2,
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

void MCDecoder::computeTargetLocation(float xRatio, float yRatio)
{
    m_xRatio = xRatio;
    m_yRatio = yRatio;
    m_computeTargetSet = true;
    m_waitComputeTargetCond.notify_all();
}

void MCDecoder::computeGeolocation()
{
    while(!m_stop)
    {
        std::unique_lock<std::mutex> lock(m_computeTargetMutex);
        m_waitComputeTargetCond.wait(lock, [this]() { return this->m_computeTargetSet; });
        int imageX = static_cast<int>(m_xRatio * m_meta["ImageWidth"].toFloat());
        int imageY = static_cast<int>(m_yRatio * m_meta["ImageHeight"].toFloat());
        m_geolocationSingle.targetLocationMain(
                        imageX,imageY,
                        m_meta["Hfov"].toFloat() / RAD_2_DEG,
                        m_meta["UavRoll"].toFloat()/ RAD_2_DEG,
                        m_meta["UavPitch"].toFloat()/ RAD_2_DEG,
                        m_meta["UavYaw"].toFloat()/ RAD_2_DEG,
                        (m_meta["GimbalPan"].toFloat() + m_panOffset)/ RAD_2_DEG,
                        (m_meta["GimbalTilt"].toFloat() + m_tiltOffset)/ RAD_2_DEG,
                        (m_meta["GimbalRoll"].toFloat() + m_rollOffset)/ RAD_2_DEG,
                        m_meta["UavLatitude"].toDouble(),
                        m_meta["UavLongitude"].toDouble(),
                        m_meta["UavAMSL"].toFloat());
        Q_EMIT locationComputed(
                    QPoint(imageX,imageY),
                    QGeoCoordinate(
                                    m_geolocationSingle.getTargetLat(),
                                    m_geolocationSingle.getTargetLon(),
                                    m_geolocationSingle.getTargetAlt()));
        m_computeTargetSet = false;
    }
}
