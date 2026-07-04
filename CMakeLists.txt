cmake_minimum_required(VERSION 3.20)
project(GlueScript LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "LLVM VERSION: ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM libs: ${LLVM_LIBRARY_DIRS}")

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})


add_executable(glue
        src/main.cpp
        src/lexer/lexer.cpp
        src/parser/parser.cpp
        src/AST/AST.cpp
        src/semantic/semantic.cpp
        src/codegen/codegen.h
        src/codegen/codegen.cpp
)

llvm_map_components_to_libnames(llvm_libs core orcjit native irreader support)

target_link_libraries(glue PRIVATE ${llvm_libs})

target_compile_options(glue PRIVATE -O2 -march=native)