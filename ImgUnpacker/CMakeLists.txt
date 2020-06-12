cmake_minimum_required(VERSION 3.10)

project(imgunpkr)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

include_directories(../cxxopts/include)
include_directories(../VbfEditor)
include_directories(../ImgSectionParser)
include_directories(../EifViewer)
include_directories(../utils)
include_directories(../fast-cpp-csv-parser)
include_directories(../CRCpp/inc)

add_executable(imgunpkr main.cpp)
if(NOT TARGET miniz)
    add_subdirectory(../miniz/ binary EXCLUDE_FROM_ALL)
endif()
target_include_directories(imgunpkr PUBLIC ../miniz)
target_link_libraries(imgunpkr vbf imgsec eif miniz)
target_compile_definitions(imgunpkr PUBLIC CSV_IO_NO_THREAD)
if(WIN32)
    target_link_libraries(imgunpkr ws2_32)
endif()

install(TARGETS imgunpkr)