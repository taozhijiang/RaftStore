cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

aux_source_directory(. DIR_LIB_SRCS)
add_library (StoreImpl STATIC ${DIR_LIB_SRCS})

