cmake_minimum_required (VERSION 2.8.11)

#set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_BUILD_TYPE RELEASE)
set(CMAKE_CXX_FLAGS_DEBUG   "$ENV{CXXFLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2 ")

set (THRIFT_GEN_CMD "chmod a+x ${PROJECT_SOURCE_DIR}/../xtra_rhel6.x/bin/protoc-2.5.0 && cd ${CMAKE_CURRENT_SOURCE_DIR}/Protocol/source && rm -fr ../gen-cpp && mkdir -p ../gen-cpp && make ")
exec_program(${THRIFT_GEN_CMD})

SET(SUBDIRS Server Storage Client Protocol/gen-cpp RPC Event Core)
foreach(dir ${SUBDIRS})
    aux_source_directory(${dir} CUR_RAFT_DIR_SRCS)
    SET(RAFT_SRCS ${RAFT_SRCS} ${CUR_RAFT_DIR_SRCS})
endforeach()

# ld iconv ?

message("Full Raft Sources: ${RAFT_SRCS}")
add_library (Raft STATIC ${RAFT_SRCS})
