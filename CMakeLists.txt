cmake_minimum_required(VERSION 3.20.2 FATAL_ERROR)
project(blosc-bench LANGUAGES CXX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 ${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_RELEASE "-O3 ${CMAKE_C_FLAGS_RELEASE}")

find_package(GDAL CONFIG REQUIRED)

add_executable(create_rea6 create_rea6.cpp)
target_link_libraries(create_rea6 PRIVATE GDAL::GDAL)

add_executable(create_era5 create_era5.cpp)
target_link_libraries(create_era5 PRIVATE GDAL::GDAL)

# Upstream blosc2-btune has a defective CMake build that requires a pre-install step
if (NOT ${RAN_PREBUILD} STREQUAL "1")
    message(STATUS "Running prebuild.sh")
    execute_process (
        COMMAND bash -c "${PROJECT_SOURCE_DIR}/Blosc2-Btune/prebuild.sh"
        OUTPUT_VARIABLE outVar
    )
    set(RAN_PREBUILD "1" CACHE INTERNAL)
    message(STATUS outVar)
endif()

add_subdirectory(Blosc2-Btune)

add_executable(roundtrip roundtrip.c)
target_link_libraries(roundtrip PRIVATE blosc2 blosc2_btune m)
