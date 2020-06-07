cmake_minimum_required(VERSION 3.10)

#project(eifviewer)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_library(eif EifConverter.cpp ./EasyBMP/EasyBMP.cpp ./exoquant/exoquant.c)
add_executable(eifviewer  main.cpp)
target_link_libraries(eifviewer eif)
include_directories(../cxxopts/include)
include_directories(../CRCpp/inc)
include_directories(../utils)
include_directories(./EasyBMP)
include_directories(./exoquant)

install(TARGETS eifviewer)