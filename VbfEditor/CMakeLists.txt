cmake_minimum_required(VERSION 3.10)

project(vbfeditor)
# The version number.
set (vbfedit_VERSION_MAJOR 0)
set (vbfedit_VERSION_MINOR 1)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
        "VbfeditConfig.h.in"
        "${PROJECT_BINARY_DIR}/VbfeditConfig.h"
)

# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h
include_directories("${PROJECT_BINARY_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

include_directories(../rapidjson/include)
include_directories(../cxxopts/include)
include_directories(../CRCpp/inc)

add_library(vbf VbfFile.cpp VbfFile.h)
add_executable(vbfeditor main.cpp)
target_link_libraries(vbfeditor vbf)

if(WIN32)
    target_link_libraries(vbfeditor ws2_32)
endif()

install(TARGETS vbfeditor)