cmake_minimum_required(VERSION 3.20)
project(Socket_Programming)

set(CMAKE_CXX_STANDARD 98)

add_executable(serverM serverM.cpp)
add_executable(serverA serverA.cpp)
add_executable(serverB serverB.cpp)
add_executable(serverC serverC.cpp)
add_executable(clientA clientA.cpp)
add_executable(clientB clientB.cpp)
