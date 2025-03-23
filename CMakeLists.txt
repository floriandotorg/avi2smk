cmake_minimum_required(VERSION 3.10)
project(avi2smk)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB LIB_SOURCES lib/**/*.cpp)

add_library(shared_lib STATIC ${LIB_SOURCES})

add_executable(avi2smk src/avi2smk.cpp)
add_executable(smk2avi src/smk2avi.cpp)

target_link_libraries(avi2smk shared_lib)
target_link_libraries(smk2avi shared_lib)

target_include_directories(shared_lib PUBLIC lib)
