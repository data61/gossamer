project(gossamer)

cmake_minimum_required(VERSION 3.1)
set(PACKAGE_BUGREPORT "need-an-email-address")
set(PACKAGE_NAME "gossamer")
set(PACKAGE_VERSION "0.0.0a")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_TARNAME "${PACKAGE_NAME}")

option(BUILD_docs "build the documents" ON)
option(BUILD_tests "build the tests" ON)
option(BUILD_translucent "build translucent" OFF)
option(MY_GLIBCXX_HAS_THE_WRONG_ABI "the local GNU libstdc++ is using the wrong ABI" OFF)

set(CMAKE_MODULE_PATH ${gossamer_SOURCE_DIR}/cmake)
set(GOSSAMER_SOURCE_DIR ${gossamer_SOURCE_DIR}/src)

if(BUILD_tests)
    enable_testing()
endif(BUILD_tests)

include(GossDetectPlatform)
include(GossDetectDependencies)
if(BUILD_docs)
    include(GossPandoc)
endif(BUILD_docs)

add_subdirectory(src)
if(BUILD_docs)
    add_subdirectory(docs)
endif(BUILD_docs)

