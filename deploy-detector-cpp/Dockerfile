# Use a base image that includes the necessary runtime environment
# For Linux, you might use a Debian or Ubuntu image
FROM ubuntu:22.04

# install python
RUN apt-get update
RUN apt-get install -y python3.10 python3-pip

# Set the working directory
WORKDIR /app

# Copy the precompiled executable and shared library (if any) into the container
COPY exe /app/exe/
COPY lib /app/lib/

# Copy necessary input/output/models directory
COPY models /app/models/

# Copy the Flask application
COPY app.py /app/
COPY templates /app/templates/

# Set the LD_LIBRARY_PATH environment variable to locate the shared library
ENV LD_LIBRARY_PATH=/app/lib/

# Install required packages
RUN pip install Flask
RUN pip install gevent
RUN pip install opencv-python
RUN pip install numpy

# Install OpenGL and GLib libraries for OpenCV
RUN apt-get update && apt-get install -y libgl1 libglib2.0-0 && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y libgtk-3-dev

# Install GStreamer base libraries and plugins
RUN apt-get update && apt-get install -y \
    libgstreamer1.0-0 \
    libgstreamer-plugins-base1.0-0 \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav

# Install FFmpeg libraries
RUN apt-get update && apt-get install -y ffmpeg

# Expose port 5000 for the Flask application
EXPOSE 5000

# Run the Flask application
CMD ["python3", "app.py"]
