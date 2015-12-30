cmake_minimum_required (VERSION 2.6)
project (dup_ident C CXX)

enable_testing()

set(COMMON_FLAGS " -D_FILE_OFFSET_BITS=64 -rdynamic")
set(WARNING_FLAGS " -Wall -Wunused-function -Wwrite-strings -Wformat -Wformat-security -Wparentheses -Wsequence-point -Wno-system-headers -Werror -Winit-self")
set(CFLAGS_DEBUG " -g -O0 -fstack-protector-all -D_GLIBXX_DEBUG -D_GLIBXX_DEBUG_PEDANTIC")
set(CFLAGS_REL " -O2 -DNDEBUG -Wuninitialized")

set(CMAKE_CXX_FLAGS_DEBUG " ${COMMON_FLAGS} ${WARNING_FLAGS} ${CFLAGS_DEBUG}")
set(CMAKE_C_FLAGS_DEBUG " ${COMMON_FLAGS} ${WARNING_FLAGS} ${CFLAGS_DEBUG}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG)


set(CMAKE_CXX_FLAGS_RELEASE " ${COMMON_FLAGS} ${WARNING_FLAGS} ${CFLAGS_REL}")
set(CMAKE_C_FLAGS_RELEASE " ${COMMON_FLAGS} ${WARNING_FLAGS} ${CFLAGS_REL}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

#I have no idea why CMAKE_CXX_FLAGS is empty
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	set(EFFECTIVE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG}")
	set(EFFECTIVE_C_FLAGS "${CMAKE_C_FLAGS_DEBUG}")
else(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	set(EFFECTIVE_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELEASE}")
	set(EFFECTIVE_C_FLAGS "${CMAKE_C_FLAGS_RELEASE}")
endif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")

message(STATUS "Currnet build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Selected CXXFLAGS: ${EFFECTIVE_CXX_FLAGS}")


add_subdirectory(third_party/gtest)

find_package(Threads REQUIRED)
find_package(Boost COMPONENTS program_options filesystem system thread REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)

include_directories(third_party/gtest/include)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OpenSSL_INCLUDE_DIRS})
include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${GTEST_INCLUDE_DIRS})

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS protos/dup_ident.proto)

add_library(dup_ident_proto ${PROTO_SRCS})

add_library(exceptions_lib exceptions.cpp)
target_link_libraries(exceptions_lib ${Boost_LIBRARIES})

add_library(file_tree_lib file_tree.cpp)
target_link_libraries(file_tree_lib ${Boost_LIBRARIES})

add_library(hash_cache_lib hash_cache.cpp)
target_link_libraries(hash_cache_lib ${Boost_LIBRARIES})
target_link_libraries(hash_cache_lib ${PROTOBUF_LIBRARIES})
target_link_libraries(hash_cache_lib ${OPENSSL_CRYPTO_LIBRARY})
target_link_libraries(hash_cache_lib exceptions_lib)
target_link_libraries(hash_cache_lib dup_ident_proto)

add_executable(file_tree_test file_tree_test.cpp)
target_link_libraries(file_tree_test file_tree_lib)
target_link_libraries(file_tree_test gtest_main)
add_test(file_tree_test file_tree_test)

add_executable(dup_ident dup_ident.cpp)
target_link_libraries(dup_ident ${Boost_LIBRARIES})
target_link_libraries(dup_ident file_tree_lib)
target_link_libraries(dup_ident hash_cache_lib)
target_link_libraries(dup_ident exceptions_lib)

