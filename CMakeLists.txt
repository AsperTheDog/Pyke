cmake_minimum_required(VERSION 3.20)
project(pyke VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler sources
add_executable(pyke
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/analyzer.cpp
    src/generator.cpp
)

target_include_directories(pyke PRIVATE src)

# Tests
enable_testing()

add_executable(test_lexer
    tests/test_lexer.cpp
    src/lexer.cpp
)

target_include_directories(test_lexer PRIVATE src)

add_executable(test_parser
    tests/test_parser.cpp
    src/lexer.cpp
    src/parser.cpp
)

target_include_directories(test_parser PRIVATE src)

add_executable(test_analyzer
    tests/test_analyzer.cpp
    src/lexer.cpp
    src/parser.cpp
    src/analyzer.cpp
)

target_include_directories(test_analyzer PRIVATE src)

add_executable(test_generator
    tests/test_generator.cpp
    src/lexer.cpp
    src/parser.cpp
    src/analyzer.cpp
    src/generator.cpp
)

target_include_directories(test_generator PRIVATE src)

add_test(NAME test_lexer COMMAND test_lexer)
add_test(NAME test_parser COMMAND test_parser)
add_test(NAME test_analyzer COMMAND test_analyzer)
add_test(NAME test_generator COMMAND test_generator)
