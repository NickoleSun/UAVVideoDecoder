#ifndef FFMPEGDECODER_H
#define FFMPEGDECODER_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
// FFmpeg
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
#include "../DecoderInterface.h"

class FFMPEGDecoder : public DecoderInterface
{
    Q_OBJECT
public:
    explicit FFMPEGDecoder(DecoderInterface *parent = nullptr);
    ~FFMPEGDecoder() override;

    void run() override;
    void stop() override;
    void setVideo(QString source) override;
    void setSpeed(float speed) override;
    void pause(bool pause) override;
    void goToPosition(float percent) override;

    bool openSource(QString videoSource);
    QVariantMap decodeMeta(unsigned char* data, int length);

Q_SIGNALS:

public Q_SLOTS:

private:
    bool m_sourceSet = false;
    bool m_posSeekSet = false;
    float m_posSeek = 0;
    QMutex *m_mutex;
    QWaitCondition* m_pauseCond;
    bool m_pause = false;
    std::string m_logFile;
    unsigned char m_data[24883200];

    AVFormatContext* m_readFileCtx = nullptr;
    AVCodec* m_videoCodec = nullptr;
    AVStream* m_videoStream = nullptr;
    int m_metaStreamIndex = -1;
    int m_audioStreamIndex = -1;
    int m_videoStreamIndex = -1;
};

#endif // FFMPEGDECODER_H
