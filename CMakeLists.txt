cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x " )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Woverloaded-virtual -Wpointer-arith -Wwrite-strings -Werror=format -march=native " )


####################
### 修改编译选项 ###

# if what to see debug in verbose mode
# cmake -DCMAKE_VERBOSE_MAKEFILE=true ../

#set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_BUILD_TYPE RELEASE)
set(CMAKE_CXX_FLAGS_DEBUG   "$ENV{CXXFLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O2 ")

IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
    set(BUILD_TYPE_STR "debug")
ELSEIF(CMAKE_BUILD_TYPE MATCHES RELEASE)
    set(BUILD_TYPE_STR "release")
    add_definitions(-DNP_DEBUG)
ELSE(CMAKE_BUILD_TYPE MATCHES DEBUG)
    message(FATAL_ERROR "Unknown BUILD_TYPE ... ")
ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)


set (INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/
                  ${PROJECT_SOURCE_DIR}/libRaft/
				  ${PROJECT_SOURCE_DIR}/../xtra_rhel6.x/include/
)
include_directories( ${INCLUDE_DIRS} )
    
add_subdirectory( libRaft )
add_subdirectory( StoreImpl )
add_subdirectory( tzhttpd )

link_directories( 
    ${PROJECT_SOURCE_DIR}/../xtra_rhel6.x/libs/
    ${PROJECT_SOURCE_DIR}/../xtra_rhel6.x/libs/leveldb/
    ${PROJECT_SOURCE_DIR}/../xtra_rhel6.x/libs/protobuf-2.5.0/
)

# local temp generated file
exec_program( "export BUILD_VAR=`git log -1 --pretty=%H` && echo 'const char *build_commit_version = \"VCS: Commit:' $BUILD_VAR '\";' > build_version.cc ")
exec_program( "export BUILD_VAR=`git symbolic-ref HEAD` && echo 'const char *build_commit_branch = \"VCS: Branch:' $BUILD_VAR '\";' >> build_version.cc ")
exec_program( "export BUILD_VAR=`git log -1 --pretty=%cd` && echo 'const char *build_commit_date = \"VCS: Date:' $BUILD_VAR '\";' >> build_version.cc ")
exec_program( "export BUILD_VAR=`git log -1 --pretty=\"%an %ae\"` && echo 'const char *build_commit_author = \"VCS: Author:' $BUILD_VAR '\";' >> build_version.cc ")
exec_program( "export BUILD_VAR=`date` && echo 'const char *build_time = \"Build At:' $BUILD_VAR '\";' >> build_version.cc ")

add_executable( RaftStoreSrv Main.cc build_version.cc )

# ld iconv ?

set (WHOLE_LIBS Raft StoreImpl )
set (EXTRA_LIBS ${EXTRA_LIBS} leveldb snappy )
set (EXTRA_LIBS ${EXTRA_LIBS} pthread )
set (EXTRA_LIBS ${EXTRA_LIBS} cryptopp protoc protobuf )

target_link_libraries( RaftStoreSrv -lrt -rdynamic -ldl
    -Wl,--whole-archive ${WHOLE_LIBS} -Wl,--no-whole-archive
    ${EXTRA_LIBS}
)
