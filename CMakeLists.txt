cmake_minimum_required (VERSION 3.4)
project(Buffer CXX)

find_package(Doxygen)
if(DOXYGEN_FOUND)
    add_custom_target(doxygen ALL
        ${DOXYGEN_EXECUTABLE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
        SOURCES Doxyfile
    )
endif(DOXYGEN_FOUND)

add_definitions(
    -DNOMINMAX
    -DWIN32_LEAN_AND_MEAN
    -DVC_EXTRALEAN
    -D_CRT_SECURE_NO_WARNINGS
    -D_SECURE_SCL=0
    -D_SCL_SECURE_NO_WARNINGS
)

option(BUILD_SHARED_LIBS "Build libraries as DLLs" FALSE)

#set(Buffer_VERSION_MAJOR 0)
#set(Buffer_VERSION_MINOR 1)
#configure_file("${PROJECT_SOURCE_DIR}/Version.h.in" "${PROJECT_BINARY_DIR}/Version.h")

set(PUBLIC_INCLUDE_PATHS
    include/Buffer
)

set(SOURCES
    include/Buffer/Buffer.h
    src/Buffer.cpp
)

#add_library(Buffer ${SOURCES})
#target_include_directories(Buffer PUBLIC ${PUBLIC_INCLUDE_PATH})
