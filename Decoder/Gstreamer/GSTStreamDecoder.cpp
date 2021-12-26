#include "GSTStreamDecoder.h"

GSTStreamDecoder::GSTStreamDecoder(DecoderInterface *parent) : DecoderInterface(parent)
{
    gst_init(nullptr,nullptr);
}
GSTStreamDecoder::~GSTStreamDecoder(){
    stop();
    printf("Delete GSTStreamDecoder\r\n");
}

GstFlowReturn GSTStreamDecoder::wrap_read_frame_buffer(GstAppSink *sink, gpointer user_data) {
    GSTStreamDecoder * itself = (GSTStreamDecoder*)user_data;
    itself->read_frame_buffer(sink,NULL);
    return GST_FLOW_OK;
}

GstFlowReturn GSTStreamDecoder::read_frame_buffer(GstAppSink* sink, gpointer user_data) {
    GstSample *sample = gst_app_sink_pull_sample(sink);

    if (sample == nullptr) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    if(sample!=NULL && GST_IS_SAMPLE (sample))
    {
        gboolean res;
        gint32 width,height;
        GstCaps *caps;

        GstStructure *str;

        std::pair <int,GstSample*> data;

        caps = gst_sample_get_caps (sample);
        if(!GST_IS_CAPS(caps)) {
            g_print("Could not get cap\n");
            return GST_FLOW_ERROR;
        }
        str = gst_caps_get_structure(caps, 0);
        if(!GST_IS_STRUCTURE(str)) {
            g_print("Could not get structure\n");
            return GST_FLOW_ERROR;
        }
        res = gst_structure_get_int (str, "width", &width);
        res |= gst_structure_get_int (str, "height", &height);
        if (!res || width == 0 || height == 0)
        {
            g_print ("could not get snapshot dimension\n");
            return GST_FLOW_ERROR;
        }

        m_buf = gst_sample_get_buffer (sample);
        if(!GST_IS_BUFFER(m_buf)) {
            g_print("Could not get buf\n");
            return GST_FLOW_ERROR;
        }

        gst_buffer_map(m_buf, &m_map, GST_MAP_READ);
//        printf("frameDecoded[%dx%d] [%d]\r\n",width,height,m_map.size);
        memcpy(m_data,m_map.data,m_map.size);
        Q_EMIT frameDecoded(m_data,width,height,m_buf->dts);
        gst_sample_unref(sample);
    }
    return GST_FLOW_OK;
}

gboolean GSTStreamDecoder::gstreamer_pipeline_operate() {
    m_loop = g_main_loop_new(NULL, FALSE);
    // launch pipeline
    std::ostringstream ss;
    ss << m_source.toStdString();
    std::cout << "gstreamer_pipeline_operate [" << m_source.toStdString().length() << "]" << m_source.toStdString() << std::endl;
    GError* err;
    m_pipeline = gst_parse_launch(ss.str().c_str(), &err);
    if( err != NULL )
    {
        g_print("gstreamer decoder failed to create pipeline\n");
        g_error_free(err);
        return FALSE;
    }else{
        g_print("gstreamer decoder create pipeline success\n");
    }
    GstElement *m_sink = gst_bin_get_by_name((GstBin*)m_pipeline, "mysink");
    GstAppSink *m_appsink = (GstAppSink *)m_sink;
    if(!m_sink || !m_appsink)
    {
#ifdef DEBUG
        g_print("Fail to get element \n");
#endif
        return FALSE;
    }
    // drop
    gst_app_sink_set_drop(m_appsink, true);
    g_object_set(m_appsink, "emit-signals", TRUE, NULL);
    // check end of stream
    // add call back received video data
    GstAppSinkCallbacks cbs;
    memset(&cbs, 0, sizeof(GstAppSinkCallbacks));
    cbs.new_sample = wrap_read_frame_buffer;
    gst_app_sink_set_callbacks(m_appsink, &cbs, (void*)this, NULL);
    const GstStateChangeReturn result = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if(result != GST_STATE_CHANGE_SUCCESS)
    {
        g_print("gstreamer failed to playing\n");
    }
    g_main_loop_run(m_loop);
    gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
    g_object_unref(m_pipeline);
    printf("gstreamer setup done\n");

    return TRUE;
}
void GSTStreamDecoder::setVideo(QString source){
    m_source = source + " ! appsink name=mysink sync="+ (source.contains("filesrc")?"true":"false");
    printf("pipeline: %s\r\n",m_source.toStdString().c_str());
    if(m_pipeline != NULL){
        GError *err = NULL;
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
        m_pipeline = gst_parse_launch(m_source.toStdString().c_str(), &err);
        if( err != NULL )
        {
            g_print("gstreamer decoder failed to reset filesrc\n");
            g_error_free(err);
        }else{
            g_print("gstreamer decoder reset filesrc success\n");
        }
        GstElement *m_sink = gst_bin_get_by_name((GstBin*)m_pipeline, "mysink");
        GstAppSink *m_appsink = (GstAppSink *)m_sink;
        if(!m_sink || !m_appsink)
        {
    #ifdef DEBUG
            g_print("Fail to get element \n");
    #endif
        }
        // drop
        gst_app_sink_set_drop(m_appsink, true);
        g_object_set(m_appsink, "emit-signals", TRUE, NULL);
        // check end of stream
        // add call back received video data
        GstAppSinkCallbacks cbs;
        memset(&cbs, 0, sizeof(GstAppSinkCallbacks));
        cbs.new_sample = wrap_read_frame_buffer;
        gst_app_sink_set_callbacks(m_appsink, &cbs, (void*)this, NULL);
        const GstStateChangeReturn result = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        if(result != GST_STATE_CHANGE_SUCCESS)
        {
            g_print("gstreamer failed to playing\n");
        }
    }else{

    }
}
void GSTStreamDecoder::setStateRun(bool running) {
    if(m_loop != NULL &&  g_main_loop_is_running(m_loop) == TRUE){
//            printf("Set video capture state to null\r\n");
        g_main_loop_quit(m_loop);
    }
}

void GSTStreamDecoder::run() {
    while(!m_stop)
    {
        if(GSTStreamDecoder::gstreamer_pipeline_operate()) {
            g_print("Pipeline running successfully . . .\n");
        }else {
            g_print("Running Error!");
        }
    }
}

void GSTStreamDecoder::pause(bool pause){
    if(m_pipeline == NULL) return;
    if(pause){
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PAUSED);
    }else{
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PLAYING);
    }
}

void GSTStreamDecoder::stop(){
    printf("Stopping capture thread\r\n");
    setStateRun(false);
    gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
}
