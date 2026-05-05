cmake_minimum_required(VERSION 3.10)
project(ExchangeSimulator)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(exch-sim 
    src/main.cpp
)
target_include_directories(exch-sim PRIVATE include)

enable_testing()

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(tests 
    tests/test_strategy.cpp
    tests/test_price_models.cpp 
)
target_link_libraries(tests PRIVATE GTest::gtest_main)
target_include_directories(tests PRIVATE include)

add_test(NAME ALL_TESTS COMMAND tests)
