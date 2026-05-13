cmake_minimum_required(VERSION 3.10)
project(ExchangeSimulator)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(exch-sim
    src/main.cpp
)
target_include_directories(exch-sim PRIVATE 
    include/core 
    include/bots 
    include/strategy 
    include/sim 
    include/model
)
target_include_directories(exch-sim PRIVATE strategy)
target_include_directories(exch-sim SYSTEM PRIVATE include/external/pcg)

enable_testing()

find_package(GTest REQUIRED)

add_executable(tests
    tests/test_price_models.cpp
    tests/test_matching.cpp
    tests/test_maker.cpp
    tests/test_taker.cpp
)
target_link_libraries(tests PRIVATE GTest::gtest_main)
target_include_directories(tests PRIVATE 
    include/core 
    include/bots 
    include/strategy 
    include/sim 
    include/model
)
target_include_directories(tests PRIVATE strategy)
target_include_directories(tests SYSTEM PRIVATE include/external/pcg)

add_test(NAME ALL_TESTS COMMAND tests)