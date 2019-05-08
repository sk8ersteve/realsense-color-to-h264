cmake_minimum_required(VERSION 3.0)

project(
    realsense-color-to-hevc
)

add_library(hve SHARED hardware-video-encoder/hve.c)

add_executable(realsense-color-to-hevc main.cpp)
target_include_directories(realsense-color-to-hevc PRIVATE hardware-video-encoder)
target_link_libraries(realsense-color-to-hevc hve avcodec avutil realsense2)
