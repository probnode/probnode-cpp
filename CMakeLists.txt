cmake_minimum_required(VERSION 3.18)

project(probnode
  VERSION 0.1.0
  DESCRIPTION "Official C++ client for the ProbNode API"
  HOMEPAGE_URL "https://probnode.com"
  LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(PROBNODE_IS_TOP_LEVEL ON)
else()
  set(PROBNODE_IS_TOP_LEVEL OFF)
endif()

option(PROBNODE_BUILD_TESTS "Build the tests" ${PROBNODE_IS_TOP_LEVEL})
option(PROBNODE_BUILD_EXAMPLES "Build the examples" ${PROBNODE_IS_TOP_LEVEL})
option(PROBNODE_INSTALL "Generate the install target" ${PROBNODE_IS_TOP_LEVEL})
option(PROBNODE_WERROR "Treat the library's warnings as errors" OFF)
option(PROBNODE_USE_SYSTEM_JSON "Use an installed nlohmann_json instead of fetching it" OFF)

include(FetchContent)
include(GNUInstallDirs)

# libcurl does the HTTP and TLS (OpenSSL, Secure Transport or Schannel, as it was built).
find_package(CURL 7.68 REQUIRED)

# nlohmann/json parses the API's answers. It is private: no public header includes it.
if(PROBNODE_USE_SYSTEM_JSON)
  find_package(nlohmann_json 3.11 REQUIRED)
else()
  find_package(nlohmann_json 3.11 QUIET)
  if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(nlohmann_json
      URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
      URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d)
    FetchContent_MakeAvailable(nlohmann_json)
  endif()
endif()

add_library(probnode
  src/client.cpp
  src/config.cpp
  src/http.cpp
  src/parse.cpp
  src/sha256.cpp)
add_library(probnode::probnode ALIAS probnode)

target_compile_features(probnode PUBLIC cxx_std_17)
set_target_properties(probnode PROPERTIES
  CXX_EXTENSIONS OFF
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_include_directories(probnode PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(probnode
  PRIVATE CURL::libcurl $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)
target_compile_definitions(probnode PRIVATE PROBNODE_VERSION="${PROJECT_VERSION}")

if(MSVC)
  target_compile_options(probnode PRIVATE /W4 /permissive- /utf-8 $<$<BOOL:${PROBNODE_WERROR}>:/WX>)
else()
  target_compile_options(probnode PRIVATE -Wall -Wextra -Wpedantic
    $<$<BOOL:${PROBNODE_WERROR}>:-Werror>)
endif()
# GCC 8 keeps std::filesystem in a separate library.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(probnode PUBLIC stdc++fs)
endif()

if(PROBNODE_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(PROBNODE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(PROBNODE_INSTALL)
  include(CMakePackageConfigHelpers)
  install(TARGETS probnode EXPORT probnodeTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(EXPORT probnodeTargets
    NAMESPACE probnode::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/probnode)
  configure_package_config_file(cmake/probnodeConfig.cmake.in
    ${PROJECT_BINARY_DIR}/probnodeConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/probnode)
  write_basic_package_version_file(${PROJECT_BINARY_DIR}/probnodeConfigVersion.cmake
    COMPATIBILITY SameMinorVersion)
  install(FILES
    ${PROJECT_BINARY_DIR}/probnodeConfig.cmake
    ${PROJECT_BINARY_DIR}/probnodeConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/probnode)
endif()
