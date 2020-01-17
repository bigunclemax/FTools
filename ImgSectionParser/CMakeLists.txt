cmake_minimum_required(VERSION 3.10)

#project(imgparcer)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_executable(imgparcer ImageParcer.cpp ImgSectionParser.cpp ImgSectionParser.h)
add_subdirectory(../miniz/ binary EXCLUDE_FROM_ALL)
target_include_directories(imgparcer PUBLIC ../miniz ../cxxopts/include ../CRCpp/inc ../rapidjson/include)
target_link_libraries(imgparcer miniz)

install(TARGETS imgparcer)