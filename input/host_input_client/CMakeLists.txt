cmake_minimum_required(VERSION 3.4)
project (host_input_client)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_TOOLCHAIN_FILE "clang.toolchain.cmake")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -fno-exceptions -Wno-deprecated  -DOS_UBUNTU=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fno-exceptions -Wno-deprecated -DOS_UBUNTU=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVERSION_STR=${VERSION_STR}")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libs")

add_executable(host_input_client host_input_client.cpp DirectInput.cpp)