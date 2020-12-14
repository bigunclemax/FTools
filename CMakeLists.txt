cmake_minimum_required(VERSION 3.10)

project(FordTools)

set(CMAKE_CXX_STANDARD 17)
# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h
#include_directories("${PROJECT_BINARY_DIR}")

#set(CMAKE_SHARED_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
#set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")

add_subdirectory(EifViewer)
add_subdirectory(ImgSectionParser)
add_subdirectory(TextSectionParser)
add_subdirectory(VbfEditor)
add_subdirectory(ZipRepacker)
add_subdirectory(ImgUnpacker)

set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}")
