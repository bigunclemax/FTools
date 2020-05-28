cmake_minimum_required(VERSION 3.10)

#project(ziprepacker)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_executable(ziprepacker ziprepacker.cpp)
if(NOT TARGET miniz)
    add_subdirectory(../miniz/ binary EXCLUDE_FROM_ALL)
endif()

target_include_directories(ziprepacker PUBLIC ../miniz ../cxxopts/include)
target_link_libraries(ziprepacker miniz)

install(TARGETS ziprepacker)