cmake_minimum_required(VERSION 3.10)
project(ExchangeSimulator)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.1
)
FetchContent_MakeAvailable(CLI11)

add_executable(exch-sim
    src/main.cpp
    include/external/yaml/Yaml.cpp
)
target_include_directories(exch-sim PRIVATE 
    include/core 
    include/bots 
    include/strategy 
    include/sim 
    include/model
    include/interface
)
target_include_directories(exch-sim PRIVATE strategy)
target_include_directories(exch-sim SYSTEM PRIVATE include/external/pcg include/external/yaml)
target_link_libraries(exch-sim PRIVATE CLI11::CLI11)
target_compile_options(exch-sim PRIVATE 
    -O3
    -march=native
    -mtune=native
    -flto
    -fno-math-errno
    -fno-trapping-math
)
target_link_options(exch-sim PRIVATE -flto)

enable_testing()

find_package(GTest REQUIRED)

add_executable(tests
    tests/test_price_models.cpp
    tests/test_matching.cpp
    tests/test_maker.cpp
    tests/test_taker.cpp
    tests/test_simulator.cpp
    tests/test_simulator_run.cpp
    tests/test_strategy.cpp
    tests/test_sim_builder.cpp
    include/external/yaml/Yaml.cpp
)
target_link_libraries(tests PRIVATE GTest::gtest_main)
target_include_directories(tests PRIVATE 
    include/core 
    include/bots 
    include/strategy 
    include/sim 
    include/model
    include/interface
)
target_include_directories(tests PRIVATE strategy)
target_include_directories(tests SYSTEM PRIVATE include/external/pcg include/external/yaml)
target_link_libraries(tests PRIVATE CLI11::CLI11)

add_test(NAME ALL_TESTS COMMAND tests)