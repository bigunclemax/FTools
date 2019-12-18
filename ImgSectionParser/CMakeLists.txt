cmake_minimum_required(VERSION 3.10)

#project(imgparcer)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_executable(imgparcer ImageParcer.cpp ImgSectionParser.cpp ImgSectionParser.h)
include_directories(../cxxopts/include)
include_directories(../CRCpp/inc)

install(TARGETS imgparcer DESTINATION "../bin")