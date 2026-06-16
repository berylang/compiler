cmake_minimum_required(VERSION 3.20)
project(BeryCompiler)
set(CMAKE_CXX_STANDARD 17)

add_executable(bery
    src/main.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/sema/sema.cpp
    src/sema/symboltable.cpp
    src/sema/typechecker.cpp
    src/codegen/codegen.cpp
    src/codegen/irgen/declarations.cpp
    src/codegen/irgen/expressions.cpp
    src/codegen/irgen/controlflow.cpp
    src/importer/mangler.cpp
    src/importer/importer.cpp
)

target_include_directories(bery PRIVATE src)
