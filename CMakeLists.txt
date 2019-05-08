cmake_minimum_required(VERSION 3.0)

project(
    realsense-color-to-h264
)

add_library(hve SHARED hardware-video-encoder/hve.c)

add_executable(realsense-color-to-h264 main.cpp)
target_include_directories(realsense-color-to-h264 PRIVATE hardware-video-encoder)
target_link_libraries(realsense-color-to-h264 hve avcodec avutil realsense2)
