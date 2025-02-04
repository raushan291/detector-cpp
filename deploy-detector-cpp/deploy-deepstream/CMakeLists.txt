# ###################### DeepStream Inference with GStreamer ########################
# cmake_minimum_required(VERSION 3.10)
# project(DeepStream_Inference)

# # Set the C++ standard to C++17
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED True)

# # Set the path to OpenCV (optional if not used with GStreamer)
# set(OpenCV_DIR ../../classifier-cpp/classifier-cpp/opencv/build)

# # Find OpenCV package (optional if needed for preprocessing or additional functionality)
# find_package(OpenCV REQUIRED)

# # Find GStreamer package
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
# pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)
# pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)

# # Include directories
# include_directories(
#     ${OpenCV_INCLUDE_DIRS}          # Optional, if OpenCV is used
#     ${GSTREAMER_INCLUDE_DIRS}
#     ${GSTREAMER_APP_INCLUDE_DIRS}
#     ${GSTREAMER_VIDEO_INCLUDE_DIRS}
# )

# # Add executable
# add_executable(DeepStream_Inference deepstream_inference.cpp)

# # Link libraries
# target_link_libraries(
#     DeepStream_Inference pthread
#     ${OpenCV_LIBS}                  # Optional, if OpenCV is used
#     ${GSTREAMER_LIBRARIES}
#     ${GSTREAMER_APP_LIBRARIES}
#     ${GSTREAMER_VIDEO_LIBRARIES}
# )


###################### DeepStream Inference with GStreamer - Deployment ########################
cmake_minimum_required(VERSION 3.10)
project(DeepStream_Inference)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find OpenCV package (using system-installed OpenCV)
find_package(OpenCV REQUIRED)

# Find Boost package
find_package(Boost REQUIRED COMPONENTS system)

# Find GStreamer package
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)

# Include directories
include_directories(
    /opt/nvidia/deepstream/deepstream/sources/includes
    /opt/nvidia/deepstream/deepstream/lib
    ${OpenCV_INCLUDE_DIRS}          # OpenCV include directories
    ${GSTREAMER_INCLUDE_DIRS}       # GStreamer core include directories
    ${GSTREAMER_APP_INCLUDE_DIRS}   # GStreamer App include directories
    ${GSTREAMER_VIDEO_INCLUDE_DIRS} # GStreamer Video include directories
    ${Boost_INCLUDE_DIRS}           # Boost include directories
)


# Add executable
add_executable(DeepStream_Inference_Deploy deepstream_inference_deploy.cpp)

# Set the path to the DeepStream SDK libraries
link_directories(/opt/nvidia/deepstream/deepstream-7.1/lib)

# Link libraries
target_link_libraries(
    DeepStream_Inference_Deploy
    pthread                        # Required for multithreading
    ${OpenCV_LIBS}                 # Link OpenCV libraries
    ${GSTREAMER_LIBRARIES}         # GStreamer core libraries
    ${GSTREAMER_APP_LIBRARIES}     # GStreamer App libraries
    ${GSTREAMER_VIDEO_LIBRARIES}   # GStreamer Video libraries
    ${Boost_LIBRARIES}             # Link Boost libraries
    # nvdsgst_meta                 # DeepStream GStreamer meta library
    # nvds_meta                    # NVIDIA DeepStream metadata API
    # nvdsgst_helper               # DeepStream GStreamer helper library
    # nvds_inferutils              # NVIDIA inference utilities
    /opt/nvidia/deepstream/deepstream-7.1/lib/libnvdsgst_meta.so
    /opt/nvidia/deepstream/deepstream-7.1/lib/libnvds_meta.so
    /opt/nvidia/deepstream/deepstream-7.1/lib/libnvdsgst_helper.so
    /opt/nvidia/deepstream/deepstream-7.1/lib/libnvds_inferutils.so
)
