#ifndef MCDECODER_H
#define MCDECODER_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
// Libavcodec
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
#include "../DecoderInterface.h"

class MCDecoder : public DecoderInterface
{
    Q_OBJECT
public:
    explicit MCDecoder(DecoderInterface *parent = nullptr);
    ~MCDecoder() override;

    void run() override;
    void stop() override;
    void setVideo(QString source) override;
    void setSpeed(float speed) override;
    void pause(bool pause) override;
    void goToPosition(float percent) override;
    bool openSource(QString videoSource);
    int decodeFrameID(unsigned char* frameData, int width, int height);

    void decodeMeta();
    void findMeta(int metaID);

Q_SIGNALS:

public Q_SLOTS:

private:
    bool m_sourceSet = false;
    bool m_posSeekSet = false;
    float m_posSeek = 0;
    QMutex *m_mutex;
    QWaitCondition* m_pauseCond;
    bool m_pause = false;

    unsigned char m_data[24883200];

    AVFormatContext* m_readFileCtx = nullptr;
    AVCodec* m_videoCodec = nullptr;
    AVStream* m_videoStream = nullptr;

    int m_videoStreamIndex = -1;

    QString m_metaSource;
    bool m_metaSourceSet = false;

    std::mutex m_readMetaMutex;
    std::condition_variable m_waitToReadMetaCond;
    bool m_readMeta = false;

    std::mutex m_metaFoundMutex;
    std::condition_variable m_waitMetaFoundCond;
    bool m_metaFound = false;

    int m_frameId = -1;
    int m_metaId = -1;
    int m_lineCount = 0;
};

#endif // MCDECODER_H
