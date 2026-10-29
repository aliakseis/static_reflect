cmake_minimum_required(VERSION 3.15)
project(static_reflect LANGUAGES CXX)

add_library(static_reflect INTERFACE)
target_include_directories(static_reflect INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(static_reflect INTERFACE cxx_std_17)

option(STATIC_REFLECT_BUILD_EXAMPLE "Build the example" ON)
option(STATIC_REFLECT_BUILD_TESTS "Build tests" ON)

if(STATIC_REFLECT_BUILD_EXAMPLE)
  add_executable(static_reflect_example examples/example.cpp)
  target_link_libraries(static_reflect_example PRIVATE static_reflect)
endif()

if(STATIC_REFLECT_BUILD_TESTS)
  enable_testing()
  add_executable(static_reflect_tests tests/test_reflect.cpp)
  target_link_libraries(static_reflect_tests PRIVATE static_reflect)
  add_test(NAME static_reflect_tests COMMAND static_reflect_tests)
endif()
