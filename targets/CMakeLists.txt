# ===========================================================================
#  targets — libpolyir-lang-targets, the C++ writers from polyir for GPUs and WebAssembly: gpu/ (Metal Shading Language, GLSL
#  stages and Vulkan compute, SPIR-V written directly; include/polyir/Kernels.hpp, and the C ABI's kernel part,
#  include/polyir/polyir_kernels.h) and wasm/ (a WebAssembly binary written straight from the HIR; include/polyir/Wasm.hpp).
#  ninac links it for `@Kernel` and the wasm target. Beside them, each source language's construct rules (<lang>/<Lang>.translate)
#  are built into libpolyir by core/CMakeLists.txt. Built from the root CMakeLists.txt.
# ===========================================================================
file(GLOB POLYIR_TRANSLATE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/gpu/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/wasm/*.cpp")
set(POLYIR_TRANSLATE_INCLUDES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
set(POLYIR_TRANSLATE_PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(POLYIR_BUILD_STATIC AND NOT TARGET polyir_lang_targets_static)
    polyir_static_library(polyir_lang_targets_static ${POLYIR_TRANSLATE_SOURCES})
    target_include_directories(polyir_lang_targets_static PUBLIC ${POLYIR_TRANSLATE_INCLUDES} PRIVATE ${POLYIR_TRANSLATE_PRIVATE})
    target_link_libraries(polyir_lang_targets_static PUBLIC polyir_static)
endif()
if(POLYIR_ONLY_STATIC)
    return()
endif()

polyir_shared_library(polyir-lang-targets ${POLYIR_TRANSLATE_SOURCES})
target_include_directories(polyir-lang-targets PUBLIC ${POLYIR_TRANSLATE_INCLUDES} "$<INSTALL_INTERFACE:include>" PRIVATE ${POLYIR_TRANSLATE_PRIVATE})
target_link_libraries(polyir-lang-targets PUBLIC polyir)

file(GLOB POLYIR_TRANSLATE_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
foreach(test_source ${POLYIR_TRANSLATE_TESTS})
    polyir_test(${test_source} polyir-lang-targets)
endforeach()
