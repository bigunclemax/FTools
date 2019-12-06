cmake_minimum_required(VERSION 3.12)

project(FordTools)

# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h
#include_directories("${PROJECT_BINARY_DIR}")

set(CMAKE_SHARED_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")

#add_subdirectory(EifViewer)
add_subdirectory(ImgSectionParser)
add_subdirectory(VbfEditor)

#install(TARGETS vbfedit DESTINATION "../bin")
#install(TARGETS imgparcer DESTINATION "../bin")
