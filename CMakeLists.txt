cmake_minimum_required(VERSION 3.10)

project(phd CXX)
set(CMAKE_CXX_STANDARD 17)
include_directories(/usr/local/cuda/include)
include(CMakeLists.txt.in)
find_package(yaml-cpp REQUIRED)

if(NOT TARGET spdlog)
    find_package(spdlog REQUIRED)
endif()

add_subdirectory(lib/eco)

add_dependencies(
    eco
    pcm
    gnuplot-iostream
    )

set(COMMON_LIBS
    ${CMAKE_CURRENT_BINARY_DIR}/pcm-src/libPCM.a
    m
    rt
    dl
    pthread
    boost_iostreams
    boost_system
    boost_filesystem
    boost_program_options
    nvidia-ml
    cuda
    yaml-cpp
    spdlog::spdlog
    )

add_subdirectory(apps/DEPO)
add_subdirectory(apps/StEP)
add_subdirectory(apps/simple)
add_subdirectory(apps/experimental)

# workloads
add_custom_target(
    minibenchmarks
    ALL COMMAND make
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minibenchmarks/openmp/
    )