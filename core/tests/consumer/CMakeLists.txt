# A project that only uses the installed library: find_package, build a program with PolyIRBuilder, compile it to LLVM IR.
cmake_minimum_required(VERSION 3.20)
project(polyir_consumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(polyir REQUIRED)
add_executable(consumer Consumer.cpp)
target_link_libraries(consumer PRIVATE polyir::polyir)
