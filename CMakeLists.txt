cmake_minimum_required(VERSION 3.16)
project(multithreaded_file_transfer_scheduler_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

set(SIMULATOR_CORE_SOURCES
    src/request.cpp
    src/scheduler.cpp
    src/worker_manager.cpp
    src/csv_utils.cpp
    src/metrics.cpp
    src/simulation_engine.cpp
)

add_library(simulator_core STATIC ${SIMULATOR_CORE_SOURCES})
target_include_directories(simulator_core PUBLIC include)
target_link_libraries(simulator_core PUBLIC Threads::Threads)

add_executable(simulator src/main.cpp)
target_link_libraries(simulator PRIVATE simulator_core)
target_include_directories(simulator PRIVATE include)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(simulator_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(simulator PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(BUILD_TESTING "Build tests" ON)
if (BUILD_TESTING)
    include(CTest)

    add_executable(simulator_tests tests/test_simulator.cpp)
    target_link_libraries(simulator_tests PRIVATE simulator_core)
    target_include_directories(simulator_tests PRIVATE include)

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(simulator_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME simulator_tests COMMAND simulator_tests)
endif()
