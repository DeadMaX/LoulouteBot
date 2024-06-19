cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "RelWithDebInfo")

project(LoulouteBot)

if (WIN32)
    set(DPP_OUT_DIR "library/${CMAKE_BUILD_TYPE}")
    set(DPP_FILE "dpp.dll")
else()
    set(DPP_OUT_DIR "library")
    set(DPP_FILE "libdpp.so")
endif()

add_library(LIBDPP INTERFACE)
add_dependencies(LIBDPP build_libdpp)
target_link_libraries(LIBDPP INTERFACE dpp)
target_link_directories(LIBDPP INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/build_DPP_${CMAKE_BUILD_TYPE}/${DPP_OUT_DIR}")
target_include_directories(LIBDPP INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/DPP/include")


add_custom_target(build_libdpp DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/build_DPP_${CMAKE_BUILD_TYPE}/${DPP_OUT_DIR}/${DPP_FILE}")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/build_DPP_${CMAKE_BUILD_TYPE}/${DPP_OUT_DIR}/${DPP_FILE}"
                   COMMAND ${CMAKE_COMMAND} --build "${CMAKE_CURRENT_BINARY_DIR}/build_DPP_${CMAKE_BUILD_TYPE}" --config "${CMAKE_BUILD_TYPE}"
                   WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_DPP_${CMAKE_BUILD_TYPE}/"
                   DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/build_DPP_${CMAKE_BUILD_TYPE}/cmake_install.cmake")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/build_DPP_${CMAKE_BUILD_TYPE}/cmake_install.cmake"
                   COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} "${CMAKE_CURRENT_SOURCE_DIR}/DPP"
                   WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/build_DPP_${CMAKE_BUILD_TYPE}/"
                   DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/build_DPP_${CMAKE_BUILD_TYPE}/.glue")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/build_DPP_${CMAKE_BUILD_TYPE}/.glue"
                   COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/build_DPP_${CMAKE_BUILD_TYPE}/"
                   COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/build_DPP_${CMAKE_BUILD_TYPE}/.glue")

add_subdirectory(src)
