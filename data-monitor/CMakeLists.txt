cmake_minimum_required(VERSION 3.6)
project(data-monitor)

SET(SOURCE_FILES
        collector.cpp
        RF24/RF24.cpp
        RF24/RF24.h)

add_executable(collector ${SOURCE_FILES})
