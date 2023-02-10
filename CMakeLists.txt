cmake_minimum_required(VERSION 3.10)

project(phd CXX)
set(CMAKE_CXX_STANDARD 17)
include_directories(/usr/local/cuda/include)
include(CMakeLists.txt.in)

set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

# build eco library from sources
add_library(
    eco
    src/Eco.hpp
    src/Eco.cpp
    src/gpu_eco.hpp
    src/gpu_eco.cpp
    src/power_if/Rapl.hpp
    src/power_if/Rapl.cpp
    src/device/Device.hpp
    src/device/Device.cpp
    src/helpers/eco_constants.hpp
    src/helpers/power_and_perf_result.hpp
    src/helpers/power_and_perf_result.cpp
    src/helpers/final_power_and_perf_result.hpp
    src/helpers/final_power_and_perf_result.cpp
    src/helpers/params_config.hpp
    src/helpers/params_config.cpp
    src/plot_builder.hpp
    src/plot_builder.cpp
    src/helpers/params_config.hpp
    src/helpers/params_config.cpp
    src/helpers/data_filter.hpp
    src/helpers/data_filter.cpp
    src/helpers/both_stream.hpp
    src/power_if/msr.hpp
    src/power_if/msr.cpp
    src/power_if/msr_offsets.hpp
    src/device/device_state.hpp
    src/device/device_state.cpp
    src/helpers/results_container.hpp
    src/helpers/results_container.cpp
)

add_dependencies(
    eco
    pcm
    #smtp-mail
    gnuplot-iostream
    )

# define common libraries used by below apps
set(COMMON_LIBS
    eco
    ${CMAKE_CURRENT_BINARY_DIR}/pcm-src/libPCM.a
    m
    rt
    # ${CMAKE_CURRENT_BINARY_DIR}/smtp-mail-build/libsmtpclient.a
    OpenSSL::Crypto
    OpenSSL::SSL
    dl
    pthread  # these libs are linked from the bottom to the top and
             # there is a dependency: pthread <- dl <- ssl <- crypto
    boost_iostreams
    boost_system
    boost_filesystem
    boost_program_options
    nvidia-ml
    cuda
    )

# build Static Energy Profiler
add_executable(
    StEP
    src/apps/EnergyProfiler.cpp
    )

target_link_libraries(
    StEP
    ${COMMON_LIBS}
    )

# build Dynamic Energy-Performance Optimizer
add_executable(
    DEPO
    src/apps/DynamicECO.cpp
    )

target_link_libraries(
    DEPO
    ${COMMON_LIBS}
    )

# build Get Application Power
add_executable(
    GetAppPower
    src/apps/GetPowerCons.cpp
    )

target_link_libraries(
    GetAppPower
    ${COMMON_LIBS}
    )

# build SetPowerLimits
add_executable(
    SetPowerLimits
    src/apps/SetPowerLimits.cpp
    )

target_link_libraries(
    SetPowerLimits
    ${COMMON_LIBS}
    )

# build DEPO_GSS
add_executable(
    DEPO_GSS
    src/apps/dynamic_eco_gss.cpp
    )

target_link_libraries(
    DEPO_GSS
    ${COMMON_LIBS}
    )

# workloads
add_custom_target(
    minibenchmarks
    ALL COMMAND make
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minibenchmarks/openmp/
    )