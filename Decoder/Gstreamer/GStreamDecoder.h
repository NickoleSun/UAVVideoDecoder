#ifndef GSTREAMDECODER_H
#define GSTREAMDECODER_H

#include <QObject>
#include <QVariant>
#include <iostream>
#include <stdio.h>
#include <sstream>
#include <gst/app/gstappsink.h>
#include <gst/gstutils.h>
#include <gst/gstsegment.h>
#include "../DecoderInterface.h"
class GStreamDecoder : public DecoderInterface
{
    Q_OBJECT
public:
    explicit GStreamDecoder(DecoderInterface *parent = nullptr);
    ~GStreamDecoder();

    void run() override;
    void stop() override;
    void setVideo(QString source) override;
    void pause(bool pause) override;

private:
    GstFlowReturn read_frame_buffer(GstAppSink *vsink, gpointer user_data);
    static GstFlowReturn wrap_read_frame_buffer(GstAppSink *vsink, gpointer user_data);
    gboolean gstreamer_pipeline_operate();
    void setStateRun(bool running);

Q_SIGNALS:

public Q_SLOTS:

private:
    GMainLoop *m_loop  = nullptr;
    GstElement* m_pipeline = nullptr;
    GstBuffer *m_buf;
    GstMapInfo m_map;
    unsigned char m_data[24883200];
};

#endif // GSTREAMDECODER_H
