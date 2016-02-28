cmake_minimum_required (VERSION 2.6)
project (final)
file(GLOB SRC_FILES *.cpp)
add_executable(final ${SRC_FILES})

TARGET_LINK_LIBRARIES(final uv)
