cmake_minimum_required (VERSION 2.6)
project (dup_ident C CXX)

enable_testing()

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
add_library(file_tree_lib file_tree.cpp)

add_executable(file_tree_test file_tree_test.cpp)
target_link_libraries(file_tree_test file_tree_lib)
target_link_libraries(file_tree_test gtest_main)
add_test(file_tree_test file_tree_test)

add_executable(dup_ident dup_ident.cpp)
target_link_libraries(dup_ident ${Boost_LIBRARIES})
target_link_libraries(dup_ident ${PROTOBUF_LIBRARIES})
target_link_libraries(dup_ident ${OPENSSL_CRYPTO_LIBRARY})
target_link_libraries(dup_ident dup_ident_proto)
target_link_libraries(dup_ident file_tree_lib)

