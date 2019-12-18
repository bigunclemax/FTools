cmake_minimum_required(VERSION 3.10)

#project(eifviewer)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_executable(eifviewer EifImage.cpp imgViewer.cpp ./EasyBMP/EasyBMP.cpp)
include_directories(../cxxopts/include)
include_directories(../CRCpp/inc)
include_directories(./EasyBMP)

install(TARGETS eifviewer DESTINATION "bin")