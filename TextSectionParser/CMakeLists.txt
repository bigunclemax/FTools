cmake_minimum_required(VERSION 3.10)

#project(textparser)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_executable(textparser main.cpp TextSectionPacker.cpp)
target_include_directories(textparser PUBLIC
        ../utils
        ../cxxopts/include
        ../rapidjson/include
        ../fast-cpp-csv-parser)
target_compile_definitions(textparser PUBLIC CSV_IO_NO_THREAD)

install(TARGETS textparser)