cmake_minimum_required (VERSION 3.1)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
project (dup_ident C CXX)

set (CMAKE_CXX_STANDARD 11)
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
find_package(OpenSSL REQUIRED)
find_package(Sqlite3 REQUIRED)


include_directories(third_party/gtest/include)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OpenSSL_INCLUDE_DIRS})
include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${Sqlite3_INCLUDE_DIRS})

add_subdirectory(src)
