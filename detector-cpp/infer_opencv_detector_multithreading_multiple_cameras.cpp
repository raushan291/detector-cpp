#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <vector>
#include <map>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

std::mutex queueMutex;
std::mutex inferenceMutex;
std::map<int, std::queue<cv::Mat>> frameQueues;
std::atomic<bool> keepRunning(true);

const int maxQueueSize = 30;
const int skipFrames = 5;

bool toBool(const std::string& str) {
    return str == "true" || str == "1";
}

void captureFrames(const std::string& rtspURL, int camId) {
    cv::VideoCapture cap(rtspURL, cv::CAP_ANY);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open RTSP stream for camera " << camId << "!" << std::endl;
        keepRunning = false;
        return;
    }

    while (keepRunning) {
        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) break;

        std::lock_guard<std::mutex> lock(queueMutex);
        
        if (frameQueues[camId].size() < maxQueueSize) {
            frameQueues[camId].push(frame);
        } else {
            std::cout << "Queue for camera " << camId << " is full. Skipping frame." << std::endl;
        }
    }

    cap.release();
}

void processFrames(cv::dnn::Net& net, int camId,  int clientSocket, bool displayFrame = false) {
    int frameCounter = 0;

    while (keepRunning) {
        cv::Mat frame;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!frameQueues[camId].empty()) {
                frame = frameQueues[camId].front();
                frameQueues[camId].pop();
            }
        }

        {
            std::lock_guard<std::mutex> lock(inferenceMutex);

            if (frame.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            frameCounter++;

            if (frameCounter <= skipFrames) {
                continue;
            }

            frameCounter = 0;

            // Preprocess the frame for YOLOv8
            cv::Mat blob;
            cv::Size inputSize(640, 640);
            cv::dnn::blobFromImage(frame, blob, 1 / 255.0, inputSize, cv::Scalar(), true, false);
            net.setInput(blob);

            auto start = std::chrono::high_resolution_clock::now();
            cv::Mat output = net.forward();
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = (end - start) * 1000;

            // YOLOv8 output interpretation
            std::vector<cv::Rect> boxes;
            std::vector<float> confidences;
            std::vector<int> classIds;

            float confThreshold = 0.3;

            // Loop over all detections (second dimension of the output) # [1, 5, 8400]
            for (int i = 0; i < output.size[2]; ++i) {
                float xc = output.at<float>(0, 0, i) * frame.cols / inputSize.width;  // x center
                float yc = output.at<float>(0, 1, i) * frame.rows / inputSize.height; // y center
                float w = output.at<float>(0, 2, i) * frame.cols / inputSize.width;   // width
                float h = output.at<float>(0, 3, i) * frame.rows / inputSize.height;  // height

                float maxClassProb = -1;
                int classId = -1;

                for (int j = 4; j < output.size[1]; ++j) {
                    float classProb = output.at<float>(0, j, i);
                    if (classProb > maxClassProb) {
                        maxClassProb = classProb;
                        classId = j - 4;
                    }
                }

                // Apply confidence threshold and save box if it meets criteria
                if (maxClassProb > confThreshold) {
                    int left = static_cast<int>(xc - w / 2);
                    int top = static_cast<int>(yc - h / 2);
                    boxes.push_back(cv::Rect(left, top, static_cast<int>(w), static_cast<int>(h)));
                    confidences.push_back(maxClassProb);
                    classIds.push_back(classId);
                }
            }

            // Apply Non-Maximum Suppression
            std::vector<int> indices;
            float nmsThreshold = 0.4;
            cv::dnn::NMSBoxes(boxes, confidences, 0.3, nmsThreshold, indices);

            // Draw only the selected bounding boxes
            for (int idx : indices) {
                cv::Rect box = boxes[idx];
                int classId = classIds[idx];
                float confidence = confidences[idx];

                cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);
                std::string label = "Class " + std::to_string(classId) + " (" + std::to_string(confidence) + ")";
                cv::putText(frame, label, cv::Point(box.x, box.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);

                std::cout << "Camera ID: " << camId
                        << ", Class ID: " << classId
                        << ", Confidence: " << confidence
                        << ", time_taken: " << duration.count() << "ms" << std::endl;
            }

            if (displayFrame) {
                cv::imshow("Camera " + std::to_string(camId), frame);
                int key = cv::waitKey(30);
                if (key == 'q' || key == 27) {
                    std::cout << "Playback interrupted by user." << std::endl;
                    cv::destroyAllWindows();
                    break;
                }
            }

            // Encode and send the frame
            std::vector<uchar> buffer;
            cv::imencode(".jpg", frame, buffer);
            int32_t frameSize = buffer.size();
            frameSize = htonl(frameSize);       // Convert frameSize to network byte order
            int32_t camIdNet = htonl(camId);    // Convert camId to network byte order

            // Send camId first, then frameSize, then frame data
            send(clientSocket, &camIdNet, sizeof(camIdNet), 0);
            send(clientSocket, &frameSize, sizeof(frameSize), 0);
            send(clientSocket, buffer.data(), buffer.size(), 0);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <display_frame> <rtsp_url1> [<rtsp_url2> ... <rtsp_urlN>]" << std::endl;
        return -1;
    }

    std::string modelPath = argv[1];
    bool displayFrame = toBool(argv[2]);

    cv::dnn::Net net = cv::dnn::readNetFromONNX(modelPath);
    if (net.empty()) {
        std::cerr << "Failed to load the model!" << std::endl;
        return -1;
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

    std::vector<std::thread> captureThreads;
    std::vector<std::thread> processingThreads;

    for (int i = 3; i < argc; ++i) {
        std::string rtspURL = argv[i];
        int camId = i - 3;

        frameQueues[camId] = std::queue<cv::Mat>();  // Initialize frame queue for each camera

        captureThreads.emplace_back(captureFrames, rtspURL, camId);
        processingThreads.emplace_back(processFrames, std::ref(net), camId, clientSocket, displayFrame);
    }

    for (auto& thread : captureThreads) thread.join();
    keepRunning = false;
    for (auto& thread : processingThreads) thread.join();

    close(clientSocket);
    
    return 0;
}
