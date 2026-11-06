# ===========================================================================
#  runtime — libpolyir-vm: the bytecode VM that runs `.pbc` (and any Module) in process, with no toolchain: it compiles the
#  HIR to a stack-machine bytecode at load, binds the externs (the `nina_*` runtime, processes, the lifecycle host,
#  `import native` libraries) and interprets it (include/polyir/Vm.hpp). ninai and the Nina glue (nina/) link it.
#  Built from the root CMakeLists.txt.
# ===========================================================================
file(GLOB_RECURSE POLYIR_VM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(POLYIR_VM_INCLUDES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
set(POLYIR_VM_PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(POLYIR_BUILD_STATIC AND NOT TARGET polyir_vm_static)
    polyir_static_library(polyir_vm_static ${POLYIR_VM_SOURCES})
    target_include_directories(polyir_vm_static PUBLIC ${POLYIR_VM_INCLUDES} PRIVATE ${POLYIR_VM_PRIVATE})
    target_link_libraries(polyir_vm_static PUBLIC polyir_static)
endif()
if(POLYIR_ONLY_STATIC)
    return()
endif()

polyir_shared_library(polyir-vm ${POLYIR_VM_SOURCES})
target_include_directories(polyir-vm PUBLIC ${POLYIR_VM_INCLUDES} "$<INSTALL_INTERFACE:include>" PRIVATE ${POLYIR_VM_PRIVATE})
target_link_libraries(polyir-vm PUBLIC polyir)

# `poly`: a .pbc file, or meta-build (tools/metabuild, built in as text by its sync-embed.py), run on the VM.
add_executable(poly tools/Poly.cpp tools/MetaBuildText.cpp)
target_link_libraries(poly PRIVATE polyir-vm)
target_compile_options(poly PRIVATE -Wall -Wextra -Wno-unqualified-std-cast-call)

file(GLOB POLYIR_VM_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
foreach(test_source ${POLYIR_VM_TESTS})
    polyir_test(${test_source} polyir-vm)
endforeach()
