# ===========================================================================
#  polyir — C++ and .poly, one CMake project:
#
#    translate/     libpolyir-translate: the translate library, a source's tree written by a target's page of construct rules
#    core/          libpolyir: the typed HIR, PolyIRBuilder, the C++ front end, the LLVM compiler, `.pbc`, the C ABI and polyir
#                   as a translate source; libpolyir-cli (`RunPolyirCli`) and the polyir-check / polyir-hir tools
#    runtime/       libpolyir-vm: the bytecode VM that runs `.pbc`, and the `poly` command
#    targets/       each language's construct rules (<lang>/<Lang>.translate, built into libpolyir), and
#                   libpolyir-lang-targets: the C++ writers for GPUs and WebAssembly (Metal / GLSL / SPIR-V, wasm)
#
#  cmake -B build -G Ninja && ninja -C build && ctest --test-dir build
#
#  Every library, tool and test lands in the build folder itself, so a consumer's LIBRARY_PATH is that one folder.
#  POLYIR_BUILD_STATIC adds the same libraries as static archives (polyir_translate_static, polyir_static, polyir_vm_static,
#  polyir_lang_targets_static, polyir_cli_static); with POLYIR_STATIC_ONLY a parent project that add_subdirectory()s this folder gets only those.
# ===========================================================================
cmake_minimum_required(VERSION 3.20)
project(polyir LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# compile_commands.json, linked at the repository root, lets editors (clangd) find the include paths.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(POLYIR_BUILD_STATIC "Also build the static archives (polyir_translate_static, polyir_static, polyir_vm_static, polyir_lang_targets_static, polyir_cli_static)" OFF)
option(POLYIR_STATIC_ONLY "With POLYIR_BUILD_STATIC: build only the static archives (for a parent project)" OFF)
set(POLYIR_ONLY_STATIC OFF)
if(POLYIR_BUILD_STATIC AND POLYIR_STATIC_ONLY)
    set(POLYIR_ONLY_STATIC ON)
endif()

set(POLYIR_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
if(NOT POLYIR_ONLY_STATIC)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${POLYIR_OUTPUT_DIR}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${POLYIR_OUTPUT_DIR}")
    enable_testing()
    # The example programs (core/examples, the examples of the libraries under review in ideas/*/examples, and the mapping
    # programs of core/tests/programs/mapping), listed for the tests that read them (FrontParseTest, PbcTest, HirJsonWriteTest run in
    # the build folder).
    file(GLOB_RECURSE POLYIR_EXAMPLE_PROGRAMS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/core/examples/*.poly"
         "${CMAKE_CURRENT_SOURCE_DIR}/core/tests/programs/mapping/*.poly")
    file(GLOB_RECURSE POLYIR_IDEAS_PROGRAMS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ideas/*.poly")
    list(FILTER POLYIR_IDEAS_PROGRAMS INCLUDE REGEX "/ideas/[^/]+/examples/")
    list(APPEND POLYIR_EXAMPLE_PROGRAMS ${POLYIR_IDEAS_PROGRAMS})
    list(SORT POLYIR_EXAMPLE_PROGRAMS)
    list(JOIN POLYIR_EXAMPLE_PROGRAMS "\n" POLYIR_EXAMPLE_LIST)
    file(WRITE "${POLYIR_OUTPUT_DIR}/front_examples.txt" "${POLYIR_EXAMPLE_LIST}\n")
endif()

# polyir_static_library(<name> <sources…>): the static archive of a part, position independent, for embedding.
function(polyir_static_library name)
    add_library(${name} STATIC ${ARGN})
    target_compile_definitions(${name} PRIVATE POLYIR_BUILDING)
    target_compile_options(${name} PRIVATE -Wno-unqualified-std-cast-call)
    set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()

# polyir_shared_library(<name> <sources…>): a dylib/so that a program in the build tree finds by its own name (macOS: the
# install name is its absolute path), so nothing has to set DYLD_LIBRARY_PATH or an rpath. Installing rewrites it.
function(polyir_shared_library name)
    add_library(${name} SHARED ${ARGN})
    target_compile_definitions(${name} PRIVATE POLYIR_BUILDING)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wno-unqualified-std-cast-call)
    set_target_properties(${name} PROPERTIES VERSION 0.1.0 SOVERSION 0)
    if(APPLE)
        set_target_properties(${name} PROPERTIES BUILD_WITH_INSTALL_NAME_DIR ON INSTALL_NAME_DIR "${POLYIR_OUTPUT_DIR}")
    endif()
endfunction()

# polyir_test(<source> <libraries…>): one doctest-style program, run in the build folder; POLYIR_SOURCE_DIR is its part's folder.
function(polyir_test source)
    get_filename_component(name ${source} NAME_WE)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ${ARGN})
    target_compile_definitions(${name} PRIVATE POLYIR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wno-unqualified-std-cast-call)
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY "${POLYIR_OUTPUT_DIR}")
endfunction()

add_subdirectory(translate)
add_subdirectory(core)
add_subdirectory(runtime)
add_subdirectory(targets)
if(POLYIR_ONLY_STATIC)
    return()
endif()

# ── installing: `cmake --install build --prefix <dir>` puts the libraries, the headers, the polyir-hir tool and a CMake package in <dir>; a project
#    then uses find_package(polyir REQUIRED) and target_link_libraries(app PRIVATE polyir::polyir) (polyir::polyir-translate, polyir::polyir-vm,
#    polyir::polyir-lang-targets, polyir::polyir-cli).
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(POLYIR_LIBRARIES polyir-translate polyir polyir-vm polyir-lang-targets polyir-cli)
install(TARGETS ${POLYIR_LIBRARIES} EXPORT polyirTargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS polyir-hir polyir-translatec RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
foreach(part translate core runtime targets)
    install(DIRECTORY ${part}/include/polyir DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
endforeach()
install(EXPORT polyirTargets NAMESPACE polyir:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/polyir)
configure_package_config_file(cmake/polyirConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/polyirConfig.cmake" INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/polyir)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/polyirConfigVersion.cmake" VERSION 0.1.0 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/polyirConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/polyirConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/polyir)

# The build tree names each dylib by its absolute path (so nothing needs DYLD_LIBRARY_PATH); the installed ones are named by where they were
# installed, and the ones that depend on libpolyir (and libpolyir-cli on the other two) are pointed at the installed copies.
if(APPLE)
    install(CODE "set(POLYIR_BUILD_DIR \"${POLYIR_OUTPUT_DIR}\")\nset(POLYIR_LIBDIR \"${CMAKE_INSTALL_LIBDIR}\")")
    install(SCRIPT cmake/InstallNames.cmake)
endif()

# The installed library, used the way another project would (core/tests/install_check.sh).
add_test(NAME InstallCheck COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/core/tests/install_check.sh ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/core)
