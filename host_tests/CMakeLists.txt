cmake_minimum_required(VERSION 3.22)
project(energy_monitor_host_tests LANGUAGES CXX)

enable_testing()

add_executable(measurement_core_tests
    measurement_core_tests.cpp
    ../components/measurement_core/src/measurement_core.cpp
    ../components/sample_source_sim/src/sample_source_sim.cpp)

target_include_directories(measurement_core_tests PRIVATE
    ../components/measurement_core/include
    ../components/sample_source_sim/include)
target_compile_features(measurement_core_tests PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(measurement_core_tests PRIVATE /W4 /WX)
else()
    target_compile_options(measurement_core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME measurement_core COMMAND measurement_core_tests)
