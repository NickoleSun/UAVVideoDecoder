#include "MCDecoder.h"
#include <QFileInfo>
#include <QTextStream>
#include <zbar.h>
#include <fstream>
#include <unistd.h>
#include <zbar/ImageScanner.h>

MCDecoder::MCDecoder(DecoderInterface *parent) : DecoderInterface(parent)
{
    m_type = "MC";
    // initialize MC library
    av_register_all();
    avformat_network_init();
    av_register_all();
    av_log_set_level(AV_LOG_QUIET);
    m_mutex = new QMutex;
    m_pauseCond = new QWaitCondition;    
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
    m_width = m_videoStream->codec->width;
    m_height = m_videoStream->codec->height;
    setSensorParams(m_sx,m_sy);
    Q_EMIT videoInformationChanged(information);

    return true;
}

void MCDecoder::run(){
    std::thread metaReadingThead(&MCDecoder::decodeMeta,this);
    metaReadingThead.detach();

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
                }
                int width = decframe->linesize[0];
                int height = decframe->height;
                m_width = width;
                m_height = height;
                memcpy(m_data,decframe->data[0],static_cast<size_t>(width*height));
                memcpy(m_data+width*height,decframe->data[1],static_cast<size_t>(width*height/4));
                memcpy(m_data+width*height*5/4,decframe->data[2],static_cast<size_t>(width*height/4));

                int frameID = decodeFrameID(m_data,200,1);
#ifdef DEBUG
                printf("MCDecode frameID[%d]\r\n",frameID);
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
    m_metaId = -1;
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
    printf("Start decodeMeta\r\n");
    std::ifstream fread;
    fread.open(m_metaSource.toStdString().c_str());
    std::string headerLine;

    // Read header line contains meta data fields.
    std::getline(fread, headerLine);
    if(!QString::fromStdString(headerLine).contains(","))
    {
        std::getline(fread, headerLine);
    }
    QStringList metaFields = QString::fromStdString(headerLine).split(",");

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
            }
            fread.open(m_metaSource.toStdString().c_str());
            std::getline(fread, headerLine);
            if(!QString::fromStdString(headerLine).contains(","))
            {
                std::getline(fread, headerLine);
            }
            metaFields = QString::fromStdString(headerLine).split(",");
            m_lineCount = 2;
        }
        if(m_metaId >= m_frameId)
        {
            if(m_metaId > m_frameId + 90)
            {
                // go to begin of the file
                fread.seekg(0);
                std::getline(fread, headerLine);
                if(!QString::fromStdString(headerLine).contains(","))
                {
                    std::getline(fread, headerLine);
                }
                m_lineCount = 2;
            }
            else
            {
                m_metaFound = true;
                m_waitToReadMetaCond.notify_all();
                msleep(30);
                continue;
            }
        }
        std::string dataLine;
        if (std::getline(fread, dataLine))
        {
            m_lineCount ++;

            QStringList metaValues = QString::fromStdString(dataLine).split(",");
            if (metaFields.size() == metaValues.size())
            {
                QVariantMap meta;
                for (int i = 0; i < metaFields.size(); ++i)
                {
                    meta[metaFields[i].replace(" ","")] = metaValues[i].replace(" ","");
                }
                float uavASML = meta["UavAMSL"].toFloat();
                meta["UavAMSL"] = QVariant::fromValue(uavASML+32);
                m_metaId = meta["ImageId"].toInt();
#ifdef DEBUG
                printf("decode m_metaId[%d]\r\n",m_metaId);
#endif
                if(m_metaId < m_frameId)
                {
                    // continue to find meta
                    continue;
                }
                else
                {
                    updateMeta(meta);
                }
            }
        }
        else
        {
            printf("EOF\r\n");
            sleep(1);
        }

    }
    if(fread.is_open()) fread.close();
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

