# A consumer of the installed library: CI installs probnode, then configures this with
# -DCMAKE_PREFIX_PATH=<prefix> to check that find_package(probnode) works.
cmake_minimum_required(VERSION 3.18)
project(probnode_consumer LANGUAGES CXX)

find_package(probnode 0.1 REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE probnode::probnode)
