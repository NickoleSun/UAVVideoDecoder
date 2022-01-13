#include "FFMPEGDecoder.h"
#include <QGeoCoordinate>

FFMPEGDecoder::FFMPEGDecoder(DecoderInterface *parent) : DecoderInterface(parent)
{
    m_type = "FFMpeg";
    // initialize FFmpeg library
    av_register_all();
    avformat_network_init();
    av_register_all();
    av_log_set_level(AV_LOG_QUIET);
    m_mutex = new QMutex;
    m_pauseCond = new QWaitCondition;
}
FFMPEGDecoder::~FFMPEGDecoder(){
    stop();
    delete m_pauseCond;
}

bool FFMPEGDecoder::openSource(QString videoSource)
{
//    if(m_videoStream != nullptr && m_videoStream->codec != nullptr)
//        avcodec_close(m_videoStream->codec);
    if(m_readFileCtx != nullptr)
        avformat_close_input(&m_readFileCtx);
    m_source = videoSource;
    // Just decode
    int ret;
    // open input file context
    ret = avformat_open_input(&m_readFileCtx, m_source.toStdString().c_str(), nullptr, nullptr);
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
        if (m_readFileCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_DATA)
        {
            m_metaStreamIndex = i;
        }else if (m_readFileCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_videoStreamIndex = i;
        }else if (m_readFileCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            m_audioStreamIndex = i;
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
    setSensorParams(m_sx,m_sy);
    Q_EMIT videoInformationChanged(information);
    return true;
}

void FFMPEGDecoder::run(){
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
            if (got_pic){
                int64_t currentTS = pkt.pts - m_readFileCtx->streams[m_videoStreamIndex]->start_time;
                double timeBase = av_q2d(m_readFileCtx->streams[m_videoStreamIndex]->time_base);
                int currentTimeMS = static_cast<int>(static_cast<double>(currentTS) * timeBase * 1000);
                if(currentTimeMS > 0)
                {
                    m_currentTimeMS = currentTimeMS;                    
//                    printf("timeBase[%f] currentTS[%ld] pkt.pts[%ld] pkt.dts[%ld] pkt.duration[%d]\r\n",
//                           timeBase,currentTS,pkt.pts,pkt.dts,pkt.duration);
                }
#ifdef DEBUG
                printf("FFMpegDecoder\r\n");
#endif
                int width = decframe->linesize[0];
                int height = decframe->height;
                memcpy(m_data,decframe->data[0],static_cast<size_t>(width*height));
                memcpy(m_data+width*height,decframe->data[1],static_cast<size_t>(width*height/4));
                memcpy(m_data+width*height*5/4,decframe->data[2],static_cast<size_t>(width*height/4));
                Q_EMIT frameDecoded(m_data,width,height,m_currentTimeMS);
                msleep(static_cast<unsigned long>(1000.0f / (m_fps * m_speed)));
            }
        }else if(pkt.stream_index == m_audioStreamIndex){
//            printf("A");
        }else if(pkt.stream_index == m_metaStreamIndex){
//            printf("M");
            decodeMeta(pkt.data,pkt.size);
            Q_EMIT metaDecoded(m_metaProcessed);
        }
        av_free_packet(&pkt);
    }
    av_frame_free(&decframe);
    if(m_videoStream != nullptr)
        avcodec_close(m_videoStream->codec);
    if(m_readFileCtx != nullptr)
        avformat_close_input(&m_readFileCtx);
    printf("FFMPEGDecoder stopped\r\n");
}

void FFMPEGDecoder::setVideo(QString source){
    m_source = source;
    m_sourceSet = true;
    pause(false);
}

void FFMPEGDecoder::pause(bool pause){
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

void FFMPEGDecoder::goToPosition(float percent){
    m_posSeek = percent;
    m_posSeekSet = true;
    pause(false);
}

void FFMPEGDecoder::setSpeed(float speed){
    m_speed = speed;
}

void FFMPEGDecoder::stop(){
    m_stop = true;
    pause(false);
    m_mutexCaptureFrame->lock();
    m_newFrameDecoded = true;
    m_mutexCaptureFrame->unlock();
    m_waitCaptureFrame->wakeAll();
}

void FFMPEGDecoder::decodeMeta(unsigned char* data, int size)
{
    QVariantMap metaData;
    metaData["TargetPx"] = QVariant::fromValue(m_width/2);
    metaData["TargetPy"] = QVariant::fromValue(m_height/2);
    unsigned char numBytesForLength = data[16] & 0x0F;
    int startIndex = 17 + static_cast<int>(numBytesForLength);
    while(startIndex < size){
        uint8_t key = data[startIndex];
        uint8_t length = data[startIndex+1];
        if(length+startIndex+1>= size)
            break;
        std::vector<uint8_t> value(data+startIndex+2,
                                   data+startIndex+2+length);
        startIndex += length+2;
        Klv klv(key,length,value);
        switch (klv.m_key) {
        case 0x02:
            if(klv.m_length >=8){
                uint64_t timestamp =  0;
                const uint16_t dummy = 1;
                const bool is_little_endian = *(const uint8_t*)&dummy == 1;
                for(size_t i=0; i<8; i++)
                {
                    size_t shift = i*8;

                    if(is_little_endian)
                    {
                        shift = 64-8-shift;
                    }

                    timestamp |= (uint64_t)klv.m_value[i] << shift;
                }
                time_t    sec = timestamp / 1000000;
                uint64_t      usec = timestamp % 1000000;
                struct tm tm;
                gmtime_r(&sec, &tm);
                char tstr[30];
                int len = strftime(tstr, sizeof(tstr), "%Y/%m/%d %H:%M:%S", &tm);
                sprintf(tstr + len, ".%03ld ", usec / 1000);
    #ifdef DEBUG
                printf("UNIX Time stamp L[%d]: [%ld] = %s\r\n",
                       klv.m_length,timestamp,
                       tstr);
    #endif

            }else{
                #ifdef DEBUG
                printf("Wrong UNIX TimeStamp\r\n");
    #endif
            }
            break;
        case 0x03:
            #ifdef DEBUG
            printf("Mission ID: ");
            for(int i=0; i< klv.m_value.size(); i++){
                printf("%c",klv.m_value[i]);
            }
            printf("\r\n");
    #endif
            break;
        case 0x04:
            #ifdef DEBUG
            printf("Platform Tail Number: ");
            for(int i=0; i< klv.m_value.size(); i++){
                printf("%c",klv.m_value[i]);
            }
            printf("\r\n");
    #endif

            break;
        case 0x05:
        {
            if(klv.m_value.size() >=2){
                uint16_t valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65536-1)*360;
                metaData["UavYaw"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Platform Heading Angle: %f\r\n",valueFloat);
    #endif

            }

        }
            break;
        case 0x06:
        {
            if(klv.m_value.size() >=2){
                int16_t valueInt = klv.m_value[0]*256 + klv.m_value[1];
                //            int valueInt = 0xFD*256 + 0x3D;
                float valueFloat = static_cast<float>(valueInt)/(65534)*40;
                metaData["UavPitch"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Platform Pitch Angle: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x07:
        {
            if(klv.m_value.size() >=2){
                int16_t valueInt = klv.m_value[0]*256 + klv.m_value[1];
                //            int valueInt = 0x08*256 + 0xB8;
                float valueFloat = static_cast<float>(valueInt)/(65534)*100;
                metaData["UavRoll"] = QVariant::fromValue(valueFloat);
    #ifdef DEBUG
                printf("Platform Roll Angle: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x0A:
            #ifdef DEBUG
            printf("Platform Designation: ");
            for(int i=0; i< klv.m_value.size(); i++){
                printf("%c",klv.m_value[i]);
            }
            printf("\r\n");
    #endif

            break;
        case 0x0D:
        {
            if(klv.m_value.size() >=4){
                int valueInt = klv.m_value[0]*256*256*256 + klv.m_value[1]*256*256 +
                        klv.m_value[2]*256 + klv.m_value[3];
                float valueFloat = static_cast<float>(valueInt)/(4294967294)*180;
                metaData["UavLatitude"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Sensor Latitude: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x0E:
        {
            if(klv.m_value.size() >=4){
                int valueInt = klv.m_value[0]*256*256*256 + klv.m_value[1]*256*256 +
                        klv.m_value[2]*256 + klv.m_value[3];
                float valueFloat = static_cast<float>(valueInt)/(4294967294)*360;
                metaData["UavLongitude"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Sensor Longitude: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x0F:
        {
            if(klv.m_value.size() >=2){
                int valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65535)*19900-900;
                metaData["UavAMSL"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Sensor Altitude: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x10:
        {
            if(klv.m_value.size() >=2){
                int valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65535)*180;
                metaData["Hfov"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Sensor HFOV: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x11:
        {
            if(klv.m_value.size() >=2){
                int valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65535)*180;
                metaData["Vfov"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Sensor VFOV: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x12:
        {
            if(klv.m_value.size() >=4){
                uint32_t valueInt = klv.m_value[0]*256*256*256 + klv.m_value[1]*256*256 +
                        klv.m_value[2]*256 + klv.m_value[3];
                float valueFloat = static_cast<float>(valueInt)/(4294967294)*360;
                if(valueFloat<0){
                    valueFloat+=360;
                }
                metaData["GimbalPan"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Sensor Relative Azimuth: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x13:
        {
            if(klv.m_value.size() >=4){
                int32_t valueInt = klv.m_value[0]*256*256*256 + klv.m_value[1]*256*256 +
                        klv.m_value[2]*256 + klv.m_value[3];
                float valueFloat = static_cast<float>(valueInt)/(4294967294)*360;
                metaData["GimbalTilt"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Sensor Relative Elevation: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x14:
        {
            if(klv.m_value.size() >=4){
                int valueInt = klv.m_value[0]*256*256*256 + klv.m_value[1]*256*256 +
                        klv.m_value[2]*256 + klv.m_value[3];
                float valueFloat = static_cast<float>(valueInt)/(4294967295)*360;
                metaData["GimbalRoll"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Sensor Relative Roll: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x15:
        {
            if(klv.m_value.size() >=4){
                int valueInt = klv.m_value[0]*256*256*256 + klv.m_value[1]*256*256 +
                        klv.m_value[2]*256 + klv.m_value[3];
                float valueFloat = static_cast<float>(valueInt)/(4294967295)*5000000;
                metaData["TargetSLR"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("SlantRanged: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x17:
        {
            if(klv.m_value.size() >=4){
                int valueInt = klv.m_value[0]*256*256*256 + klv.m_value[1]*256*256 +
                        klv.m_value[2]*256 + klv.m_value[3];
                float valueFloat = static_cast<float>(valueInt)/(4294967294)*180;
                metaData["CenterLat"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Frame center Latitude: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x18:
        {
            if(klv.m_value.size() >=4){
                int valueInt = klv.m_value[0]*256*256*256 + klv.m_value[1]*256*256 +
                        klv.m_value[2]*256 + klv.m_value[3];
                float valueFloat = static_cast<float>(valueInt)/(4294967294)*360;
                metaData["CenterLon"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Frame center Longitude: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x19:
        {
            if(klv.m_value.size() >=2){
                int valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65535)*19900-900;
                metaData["CenterAMSL"] = QVariant::fromValue(valueFloat);
                #ifdef DEBUG
                printf("Frame center Elevation: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x1A:
        {
            if(klv.m_value.size() >=2){
                int16_t valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65534)*0.15f;
                metaData["Corner01Lat"] = QVariant::fromValue(valueFloat + metaData["CenterLat"].toFloat());
    #ifdef DEBUG
                printf("Offset corner Latitude Point1: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x1B:
        {
            if(klv.m_value.size() >=2){
                int16_t valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65534)*0.15f;
                metaData["Corner01Lon"] = QVariant::fromValue(valueFloat + metaData["CenterLon"].toFloat());
                #ifdef DEBUG
                printf("Offset corner Longitude Point1: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x1C:
        {
            if(klv.m_value.size() >=2){
                int16_t valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65534)*0.15f;
                metaData["Corner02Lat"] = QVariant::fromValue(valueFloat + metaData["CenterLat"].toFloat());
                #ifdef DEBUG
                printf("Offset corner Latitude Point2: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x1D:
        {
            if(klv.m_value.size() >=2){
                int16_t valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65534)*0.15f;
                metaData["Corner02Lon"] = QVariant::fromValue(valueFloat + metaData["CenterLon"].toFloat());
                #ifdef DEBUG
                printf("Offset corner Longitude Point2: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x1E:
        {
            if(klv.m_value.size() >=2){
                int16_t valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65534)*0.15f;
                metaData["Corner03Lat"] = QVariant::fromValue(valueFloat + metaData["CenterLat"].toFloat());
                #ifdef DEBUG
                printf("Offset corner Latitude Point3: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x1F:
        {
            if(klv.m_value.size() >=2){
                int16_t valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65534)*0.15f;
                metaData["Corner03Lon"] = QVariant::fromValue(valueFloat + metaData["CenterLon"].toFloat());
                #ifdef DEBUG
                printf("Offset corner Longitude Point3: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x20:
        {
            if(klv.m_value.size() >=2){
                int16_t valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65534)*0.15f;
                metaData["Corner04Lat"] = QVariant::fromValue(valueFloat + metaData["CenterLat"].toFloat());
                #ifdef DEBUG
                printf("Offset corner Latitude Point4: %f\r\n",valueFloat);
    #endif

            }
        }
            break;
        case 0x21:
        {
            if(klv.m_value.size() >=2){
                int16_t valueInt = klv.m_value[0]*256 + klv.m_value[1];
                float valueFloat = static_cast<float>(valueInt)/(65534)*0.15f;
                metaData["Corner04Lon"] = QVariant::fromValue(valueFloat + metaData["CenterLon"].toFloat());
                #ifdef DEBUG
                printf("Offset corner Longitude Point4: %f\r\n",valueFloat);
    #endif

            }
        }
            break;

        default:
            break;
        }
    }
    updateMeta(metaData);
}

