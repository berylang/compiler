cmake_minimum_required(VERSION 3.20)
project(BeryCompiler)
set(CMAKE_CXX_STANDARD 17)

set(BRE_SOURCES
    src/bre/src/bery_runtime.cpp
    src/bre/src/bery_alloc.cpp
    src/bre/src/bery_gc_roots.cpp
    src/bre/src/bery_type_registry.cpp
    src/bre/src/bery_gc.cpp
    src/bre/src/bery_string.cpp
    src/bre/src/bery_array.cpp
    src/bre/src/bery_native.cpp
)

add_library(bre STATIC ${BRE_SOURCES})
target_include_directories(bre PRIVATE src)

add_executable(bery
    src/main.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/parser/parser_conditional.cpp
    src/parser/parser_controlflow.cpp
    src/parser/parser_errors.cpp
    src/parser/parser_functions.cpp
    src/parser/parser_expression.cpp
    src/parser/parser_loops.cpp
    src/parser/parser_oop.cpp
    src/parser/parser_statements.cpp
    src/sema/sema.cpp
    src/sema/sema_declarations.cpp
    src/sema/sema_controlflow.cpp
    src/sema/symboltable.cpp
    src/sema/typechecker.cpp
    src/codegen/codegen.cpp
    src/codegen/irgen/declarations.cpp
    src/codegen/irgen/expressions.cpp
    src/codegen/irgen/controlflow.cpp
    src/codegen/irgen/helpers.cpp
    src/codegen/irgen/bre.cpp
    src/codegen/irgen/gc.cpp
    src/importer/mangler.cpp
    src/importer/importer.cpp
)

target_include_directories(bery PRIVATE src)
target_link_libraries(bery PRIVATE bre)

install(TARGETS bre ARCHIVE DESTINATION lib)
install(TARGETS bery RUNTIME DESTINATION bin)