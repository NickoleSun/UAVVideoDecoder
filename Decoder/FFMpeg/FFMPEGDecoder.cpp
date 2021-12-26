#include "FFMPEGDecoder.h"
#include <QGeoCoordinate>
FFMPEGDecoder::FFMPEGDecoder(DecoderInterface *parent) : DecoderInterface(parent)
{
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
            QVariantMap meta = decodeMeta(pkt.data,pkt.size);
            Q_EMIT metaDecoded(meta);
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
}

QVariantMap FFMPEGDecoder::decodeMeta(unsigned char* data, int length)
{
    QVariantMap res;
    res["coordinate"] = QVariant::fromValue(QGeoCoordinate(21.0388022,105.4892664));
    res["yaw"] = QVariant::fromValue(40);
    res["targetCoordinate"] = QVariant::fromValue(QGeoCoordinate(21.0361586,105.4961328));
    res["centerCoordinate"] = QVariant::fromValue(QGeoCoordinate(21.0353575,105.4941587));
    return res;
}
