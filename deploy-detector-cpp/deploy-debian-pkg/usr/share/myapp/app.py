from flask import Flask, render_template, request, Response, jsonify, redirect, url_for
import subprocess
from gevent.pywsgi import WSGIServer
import fcntl
import threading
import cv2
import os
import socket
import struct
import numpy as np

app = Flask(__name__)

# Dictionary to store RTSP URLs for different cameras
camera_streams = {}

# Process for running C++ executable
cpp_process = None

# Dictionary to hold frames for each camera ID
frames = {}

def set_nonblocking(fd):
    # Set the file descriptor to non-blocking
    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

def monitor_cpp_output(process):
    set_nonblocking(process.stdout.fileno())
    set_nonblocking(process.stderr.fileno())
    
    try:
        while True:
            output = process.stdout.readline()
            error_output = process.stderr.readline()

            if output:
                print(output.strip())  # Real-time output from C++
            if error_output:
                print(error_output.strip())  # Real-time error output from C++

            if output == '' and process.poll() is not None:
                break
    finally:
        process.stdout.close()
        process.stderr.close()
        process.wait()

def receive_frames(cam_id):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('0.0.0.0', 12345))
    server_socket.listen(5)
    print("Waiting for C++ client to connect...")

    while True:
        try:
            client_socket, addr = server_socket.accept()
            print(f"Connection from {addr} established")
        except Exception as e:
            print(f"Error on accept: {e}")

        try:
            while True:
                 # Receive the camId (4 bytes)
                cam_id_data = client_socket.recv(4)
                if not cam_id_data:
                    break
                cam_id = struct.unpack('!I', cam_id_data)[0]  # Convert from network byte order

                 # Receive the frame size (4 bytes)
                frame_size_data = client_socket.recv(4)
                if not frame_size_data:
                    break
                frame_size = struct.unpack('!I', frame_size_data)[0]
                
                # Receive frame data
                frame_data = b''
                while len(frame_data) < frame_size:
                    packet = client_socket.recv(frame_size - len(frame_data))
                    if not packet:
                        break
                    frame_data += packet

                # Decode the frame and store it in frames dictionary with camId as key
                frame = cv2.imdecode(np.frombuffer(frame_data, np.uint8), cv2.IMREAD_COLOR)
                if frame is not None:
                    frames[cam_id] = frame  # Update the frame for `cam_id` camera ID
        finally:
            client_socket.close()
            print(f"Connection from {addr} closed")

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/start', methods=['POST'])
def start_inference():
    global cpp_process

    # Stop any existing process
    if cpp_process and cpp_process.poll() is None:
        cpp_process.terminate()
        cpp_process = None
    
    # Get RTSP URLs from the request
    rtsp_urls = request.json.get("rtsp_urls", [])
    model_path = "./models/yolov11n-face.onnx"
    display = "false"  # Set display option for C++ executable

    # Populate the camera_streams dictionary with RTSP URLs
    for i, url in enumerate(rtsp_urls):
        camera_streams[i] = url  # Map each RTSP URL to a camera ID

    # Construct command to start C++ executable
    command = ["/usr/local/exe/Infer_OpenCV_detector_multithreading_multiple_cameras.exe", model_path, display] + rtsp_urls

    # Start C++ executable
    cpp_process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    # Monitor output in a separate thread
    output_thread = threading.Thread(target=monitor_cpp_output, args=(cpp_process,))
    output_thread.start()

    return jsonify({"status": "C++ inference started"})

@app.route('/stop', methods=['POST'])
def stop_streams():
    global cpp_process

    # Stop the C++ process if it is running
    if cpp_process and cpp_process.poll() is None:
        cpp_process.terminate()
        cpp_process = None

    return redirect(url_for('index'))

@app.route('/video_feed/<int:cam_id>')
def video_feed(cam_id):
    receive_frames_thread = threading.Thread(target=receive_frames, args=(cam_id,), daemon=True)
    receive_frames_thread.start()
    def generate():
        while True:
            frame = frames.get(cam_id)
            if frame is not None:
                _, jpeg = cv2.imencode('.jpg', frame)
                yield (b'--frame\r\n'
                       b'Content-Type: image/jpeg\r\n\r\n' + jpeg.tobytes() + b'\r\n')
    return Response(generate(), mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == "__main__":
    # Debug/Development
    app.run(debug=True, host="0.0.0.0", port=5000)
    # Production
    # http_server = WSGIServer(('', 5000), app)
    # http_server.serve_forever()
