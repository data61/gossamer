project(gossamer)

cmake_minimum_required(VERSION 2.6)
set(PACKAGE_BUGREPORT "need-an-email-address")
set(PACKAGE_NAME "gossamer")
set(PACKAGE_VERSION "0.0.0a")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_TARNAME "${PACKAGE_NAME}")

option(BUILD_tests "build the tests" ON)
option(BUILD_translucent "build translucent" OFF)

set(CMAKE_MODULE_PATH ${gossamer_SOURCE_DIR}/cmake)
set(GOSSAMER_SOURCE_DIR ${gossamer_SOURCE_DIR}/src)

if(BUILD_tests)
    enable_testing()
endif(BUILD_tests)

include(GossDetectPlatform)
include(GossDetectDependencies)
include(GossPandoc)

add_subdirectory(src)
add_subdirectory(docs)

