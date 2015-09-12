cmake_minimum_required (VERSION 2.6)
project (dup_ident)

find_package(Boost COMPONENTS program_options filesystem system thread REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OpenSSL_INCLUDE_DIRS})
include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS protos/dup_ident.proto)
add_executable(dup_ident dup_ident.cpp ${PROTO_SRCS} ${PROTO_HDRS})

target_link_libraries(dup_ident ${Boost_LIBRARIES})
target_link_libraries(dup_ident ${PROTOBUF_LIBRARIES})
target_link_libraries(dup_ident ${OPENSSL_CRYPTO_LIBRARY})
