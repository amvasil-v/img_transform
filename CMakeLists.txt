cmake_minimum_required(VERSION 3.3)
project(img_transform)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    main.c)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/mbedtls/include)

add_executable(img_transform ${SOURCE_FILES})

# Pngle library

set(PNGLE_SOURCES
    pngle/pngle.c
    pngle/miniz.c
)

include_directories(${CMAKE_SOURCE_DIR}/pngle)

add_library(pngle STATIC
    ${PNGLE_SOURCES}
)

target_link_libraries(img_transform
    PUBLIC pngle
)

# System libraries

target_link_libraries(img_transform PUBLIC
    png
    m
)
