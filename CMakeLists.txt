cmake_minimum_required(VERSION 3.20)
project(krb VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_library(krb INTERFACE)
add_library(krb::krb ALIAS krb)

target_include_directories(krb INTERFACE "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")
target_compile_features(krb INTERFACE cxx_std_20)

option(KRB_BUILD_TEST "Build simple test for korobok")

if (KRB_BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif()