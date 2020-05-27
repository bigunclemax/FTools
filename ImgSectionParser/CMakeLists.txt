cmake_minimum_required(VERSION 3.10)

#project(imgparcer)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_library(imgsec ImageSection.cpp)
target_include_directories(imgsec PUBLIC
        ../rapidjson/include
        ../utils)

add_executable(imgparcer main.cpp)
target_include_directories(imgparcer PUBLIC ../cxxopts/include)
target_link_libraries(imgparcer imgsec)

install(TARGETS imgparcer)