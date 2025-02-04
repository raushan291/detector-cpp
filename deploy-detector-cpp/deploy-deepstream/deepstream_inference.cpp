#include <gst/gst.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <functional> // Include this for std::ref if needed


void onPadAdded(GstElement *source, GstPad *srcPad, gpointer user_data) {
    // Get the nvstreammux element from user_data
    GstElement *nvstreammux = static_cast<GstElement *>(user_data);

    // Request a sink pad from the nvstreammux element
    GstPad *sinkPad = gst_element_request_pad_simple(nvstreammux, "sink_0");

    if (!sinkPad) {
        g_printerr("Failed to get request pad from nvstreammux\n");
        return;
    }

    // Link the source pad to the requested sink pad
    GstPadLinkReturn linkReturn = gst_pad_link(srcPad, sinkPad);
    if (linkReturn != GST_PAD_LINK_OK) {
        g_printerr("Failed to link source pad to nvstreammux sink pad\n");
    } else {
        g_print("Source pad linked to nvstreammux sink pad successfully\n");
    }

    // Unreference the sink pad when done
    gst_object_unref(sinkPad);
}


void startDeepStreamPipeline(const std::string &rtspURL, int cameraId) {
    GstElement *pipeline = gst_pipeline_new("deepstream-pipeline");
    GstElement *source = gst_element_factory_make("uridecodebin", "source");
    GstElement *nvstreammux = gst_element_factory_make("nvstreammux", "stream-muxer");
    GstElement *nvinfer = gst_element_factory_make("nvinfer", "primary-inference");
    GstElement *nvvidconv = gst_element_factory_make("nvvideoconvert", "video-converter");
    GstElement *nvosd = gst_element_factory_make("nvdsosd", "onscreen-display");
    GstElement *sink = gst_element_factory_make("nveglglessink", "video-output");

    if (!pipeline || !source || !nvstreammux || !nvinfer || !nvvidconv || !nvosd || !sink) {
        g_printerr("Failed to create elements\n");
        return;
    }

    // Configure elements
    g_object_set(source, "uri", rtspURL.c_str(), NULL);
    g_object_set(nvstreammux, "batch-size", 1, "width", 1920, "height", 1080, NULL);
    g_object_set(nvinfer, "config-file-path", "../custom_yolo_config.txt", NULL);

    // Add elements to the pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, nvstreammux, nvinfer, nvvidconv, nvosd, sink, NULL);

    if (!gst_element_link_many(nvstreammux, nvinfer, nvvidconv, nvosd, sink, NULL)) {
        g_printerr("Failed to link elements\n");
        gst_object_unref(pipeline);
        return;
    }

    // Connect pad-added signal for the source
    g_signal_connect(source, "pad-added", G_CALLBACK(onPadAdded), nvstreammux);

    // Start pipeline
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to start pipeline\n");
        gst_object_unref(pipeline);
        return;
    }

    g_print("Pipeline is running for camera %d\n", cameraId);

    // Run the main loop
    GMainLoop *mainLoop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(mainLoop);

    // Clean up
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(mainLoop);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <rtsp_url1> [<rtsp_url2> ... <rtsp_urlN>]" << std::endl;
        return -1;
    }

    gst_init(&argc, &argv);

    std::vector<std::string> rtspURLs;
    for (int i = 1; i < argc; ++i) {
        rtspURLs.emplace_back(argv[i]);
    }

    std::vector<std::thread> pipelineThreads;
    for (size_t i = 0; i < rtspURLs.size(); ++i) {
        pipelineThreads.emplace_back(startDeepStreamPipeline, rtspURLs[i], static_cast<int>(i));
    }

    for (auto &thread : pipelineThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    return 0;
}


// ./DeepStream_Inference file:///workspace/cpp/classifier-cpp/classifier-cpp/frs1.mp4
