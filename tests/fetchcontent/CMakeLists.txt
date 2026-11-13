# A consumer that builds probnode as part of itself with FetchContent. CI points it at the
# checkout with -DPROBNODE_SOURCE=<dir>; a real project uses the GIT_REPOSITORY form in the README.
cmake_minimum_required(VERSION 3.18)
project(probnode_fetchcontent_consumer LANGUAGES CXX)

include(FetchContent)
if(PROBNODE_SOURCE)
  FetchContent_Declare(probnode SOURCE_DIR ${PROBNODE_SOURCE})
else()
  FetchContent_Declare(probnode
    GIT_REPOSITORY https://github.com/probnode/probnode-cpp
    GIT_TAG v0.1.0)
endif()
FetchContent_MakeAvailable(probnode)

add_executable(consumer ../find_package/main.cpp)
target_link_libraries(consumer PRIVATE probnode::probnode)
