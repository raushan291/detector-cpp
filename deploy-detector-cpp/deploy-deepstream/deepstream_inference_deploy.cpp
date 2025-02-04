#include <gst/gst.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <functional> // For std::ref
#include <boost/asio.hpp>

#include <nvdsmeta.h>
#include <nvdsinfer.h>
#include <nvdsmeta_schema.h>

#include <gstnvdsmeta.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <opencv2/opencv.hpp>


// Probe callback function
static GstPadProbeReturn inferProbeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    GstBuffer *buffer = (GstBuffer *)info->data;
    if (!buffer) {
        g_print("No buffer received\n");
        return GST_PAD_PROBE_OK;
    }

    // Access metadata
    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buffer);
    if (!batch_meta) {
        g_print("No batch metadata\n");
        return GST_PAD_PROBE_OK;
    }

    for (NvDsMetaList *l_frame = batch_meta->frame_meta_list; l_frame != NULL; l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)l_frame->data;

        for (NvDsMetaList *l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next) {
            NvDsObjectMeta *obj_meta = (NvDsObjectMeta *)l_obj->data;

            // Access bounding box coordinates
            float left = obj_meta->rect_params.left;
            float top = obj_meta->rect_params.top;
            float width = obj_meta->rect_params.width;
            float height = obj_meta->rect_params.height;

            // Access confidence score
            float confidence = obj_meta->confidence;

            // Access label
            const char *label = obj_meta->obj_label;

            // if (label) {
            //     g_print("Detected Object: Label=%s, Confidence=%.2f, BBox=[%.2f, %.2f, %.2f, %.2f]\n",
            //             label, confidence, left, top, width, height);
            // } else {
            //     g_print("No label for detected object\n");
            // }
        }
    }

    return GST_PAD_PROBE_OK;
}


void onPadAdded(GstElement *source, GstPad *srcPad, gpointer user_data) {
    GstElement *nvstreammux = static_cast<GstElement *>(user_data);
    GstPad *sinkPad = gst_element_request_pad_simple(nvstreammux, "sink_0");

    if (!sinkPad) {
        g_printerr("Failed to request sink pad from nvstreammux\n");
        return;
    }

    GstPadLinkReturn linkReturn = gst_pad_link(srcPad, sinkPad);
    if (linkReturn != GST_PAD_LINK_OK) {
        g_printerr("Failed to link source pad to nvstreammux sink pad\n");
    } else {
        g_print("Source pad linked successfully\n");
    }

    gst_object_unref(sinkPad);
}

void startDeepStreamPipeline(const std::string &rtspURL, int cameraId, int clientSocket) {
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
    // g_object_set(nvinfer, "config-file-path", "../custom_yolo_config.txt", NULL);
    g_object_set(nvinfer, "config-file-path", "/workspace/cpp/detector-cpp/detector-cpp/custom_yolo_config.txt", NULL);

    // Add elements to the pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, nvstreammux, nvinfer, nvvidconv, nvosd, sink, NULL);

    if (!gst_element_link_many(nvstreammux, nvinfer, nvvidconv, nvosd, sink, NULL)) {
        g_printerr("Failed to link elements\n");
        gst_object_unref(pipeline);
        return;
    }

    // Connect pad-added signal for the source
    g_signal_connect(source, "pad-added", G_CALLBACK(onPadAdded), nvstreammux);


    // Get the sink pad of nvosd and add the probe
    GstPad *nvosd_sink_pad = gst_element_get_static_pad(nvosd, "sink");
    if (!nvosd_sink_pad) {
        g_printerr("Could not get sink pad of nvosd\n");
    }
    gst_pad_add_probe(nvosd_sink_pad, GST_PAD_PROBE_TYPE_BUFFER, inferProbeCallback, NULL, NULL);
    gst_object_unref(nvosd_sink_pad);

    // Start the pipeline
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to start pipeline\n");
        gst_object_unref(pipeline);
        return;
    }

    g_print("Pipeline is running for camera %d\n", cameraId);

    //==================================================================================================
    // Encode and send the frame
    std::vector<uchar> buffer;
    // cv::imencode(".jpg", frame, buffer);
    cv::Mat frame = cv::imread("/workspace/cpp/detector-cpp/detector-cpp/lenna_2.jpg", cv::IMREAD_COLOR);
    cv::imencode(".jpg", frame, buffer);
    int32_t frameSize = buffer.size();
    frameSize = htonl(frameSize);       // Convert frameSize to network byte order
    int32_t camIdNet = htonl(cameraId);    // Convert camId to network byte order

    // Send camId first, then frameSize, then frame data
    send(clientSocket, &camIdNet, sizeof(camIdNet), 0);
    send(clientSocket, &frameSize, sizeof(frameSize), 0);
    send(clientSocket, buffer.data(), buffer.size(), 0);
    //==================================================================================================

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

    // Set up socket connection to Flask before starting pipelines
    g_print("Establishing connection to Flask server before starting pipelines...\n");

    std::vector<std::string> rtspURLs;
    for (int i = 1; i < argc; ++i) {
        rtspURLs.emplace_back(argv[i]);
    }


    // Set up socket connection to Python server
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);  // Choose port to match Python server
    inet_pton(AF_INET, "0.0.0.0", &serverAddr.sin_addr);

    if (clientSocket < 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return -1;
    } else {
        std::cout << "Socket created successfully." << std::endl;
    }

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection to Flask server failed." << std::endl;
        return -1;
    }


    std::vector<std::thread> pipelineThreads;
    for (size_t i = 0; i < rtspURLs.size(); ++i) {
        pipelineThreads.emplace_back(startDeepStreamPipeline, rtspURLs[i], static_cast<int>(i), clientSocket);
    }

    for (auto &thread : pipelineThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    close(clientSocket);

    return 0;
}

// ./DeepStream_Inference_Deploy file:///workspace/cpp/detector-cpp/detector-cpp/11-00-12.ts
