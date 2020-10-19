cmake_minimum_required(VERSION 3.12)
project(TCP_UDP_Client)
find_package(Threads)
set(CMAKE_CXX_STANDARD 14)

add_executable(TCP_UDP_Client main.cpp)

target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

