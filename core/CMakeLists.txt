# ===========================================================================
#  core — libpolyir, polyir as a C++ library: the typed HIR, a PolyIRBuilder that makes it, the C++ front end, the
#  compiler from it to LLVM IR text (and so to native code through clang), `.pbc` bytes, polyir as a source of the translate
#  library (src/translate, over ../translate), and a C ABI
#  (include/polyir/polyir.h), so that ninac (C++) and Nina itself (`import cpp`) load the same library.
#  libpolyir-cli is the command line as a function (`RunPolyirCli`, src/cli/Cli.cpp): it also runs the VM (../runtime)
#  and the GPU writers (../targets), so it is a library of its own, which polyir-check and the nina front door link.
#  Built from the root CMakeLists.txt (one project for the whole repository).
# ===========================================================================
file(GLOB_RECURSE POLYIR_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(POLYIR_CLI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/cli/Cli.cpp")
list(REMOVE_ITEM POLYIR_CORE_SOURCES ${POLYIR_CLI_SOURCES})
set(POLYIR_CORE_INCLUDES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")

# The targets built into the library (docs/translate.md): each targets/<lang>/<Lang>.translate page and its runtime/ files, as
# text in a generated TargetTexts.cpp, beside its compiled rules (below). A new folder under targets/ is picked up by the glob.
get_filename_component(POLYIR_TARGETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../targets" ABSOLUTE)
file(GLOB_RECURSE POLYIR_TARGET_FILES CONFIGURE_DEPENDS "${POLYIR_TARGETS_DIR}/*.translate" "${POLYIR_TARGETS_DIR}/*/runtime/*" "${POLYIR_TARGETS_DIR}/*.canon")
set(POLYIR_TARGET_TEXTS "${CMAKE_CURRENT_BINARY_DIR}/generated/TargetTexts.cpp")
add_custom_command(OUTPUT ${POLYIR_TARGET_TEXTS}
    COMMAND ${CMAKE_COMMAND} -DTARGETS_DIR=${POLYIR_TARGETS_DIR} -DOUTPUT=${POLYIR_TARGET_TEXTS} -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/EmbedTargets.cmake
    DEPENDS ${POLYIR_TARGET_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/EmbedTargets.cmake
    COMMENT "Building the targets' .translate pages into libpolyir")
list(APPEND POLYIR_CORE_SOURCES ${POLYIR_TARGET_TEXTS})

# Each page, and polyir's canon, compiled ahead by polyir-translatec (../translate): a page with a problem, or one that does not
# mention a form, stops the build. A built-in target uses its compiled rules and never reads its text.
file(GLOB POLYIR_TARGET_PAGES CONFIGURE_DEPENDS "${POLYIR_TARGETS_DIR}/*/*.translate")
set(POLYIR_CANON "${POLYIR_TARGETS_DIR}/Polyir.canon")
set(POLYIR_COMPILED "")
polyir_translate_page(POLYIR_COMPILED ${POLYIR_CANON} FUNCTION CompiledSchemaPolyir SHOWN targets/Polyir.canon)
foreach(page ${POLYIR_TARGET_PAGES})
    get_filename_component(stem ${page} NAME_WE)
    file(RELATIVE_PATH shown "${POLYIR_TARGETS_DIR}/.." "${page}")
    polyir_translate_page(POLYIR_COMPILED ${page} FUNCTION CompiledPage${stem} SHOWN ${shown} CANON ${POLYIR_CANON} DEPENDS ${POLYIR_TARGET_PAGES})
endforeach()
list(APPEND POLYIR_CORE_SOURCES ${POLYIR_COMPILED})

# The static archives, for a consumer that links the compiler into its own binary (ninac's polyir lowering, `nina`).
if(POLYIR_BUILD_STATIC AND NOT TARGET polyir_static)
    polyir_static_library(polyir_static ${POLYIR_CORE_SOURCES})
    target_include_directories(polyir_static PUBLIC ${POLYIR_CORE_INCLUDES})
    target_link_libraries(polyir_static PUBLIC polyir_translate_static)
    polyir_static_library(polyir_cli_static ${POLYIR_CLI_SOURCES})
    target_link_libraries(polyir_cli_static PUBLIC polyir_vm_static polyir_lang_targets_static polyir_static)
endif()
if(POLYIR_ONLY_STATIC)
    return()
endif()

polyir_shared_library(polyir ${POLYIR_CORE_SOURCES})
target_include_directories(polyir PUBLIC ${POLYIR_CORE_INCLUDES} "$<INSTALL_INTERFACE:include>")
target_link_libraries(polyir PUBLIC polyir-translate)

polyir_shared_library(polyir-cli ${POLYIR_CLI_SOURCES})
target_link_libraries(polyir-cli PUBLIC polyir-vm polyir-lang-targets polyir)

# One test program per area. CliTest drives the whole command line; NativeGcTest compares the native collector with the VM's.
file(GLOB POLYIR_CORE_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
foreach(test_source ${POLYIR_CORE_TESTS})
    polyir_test(${test_source} polyir)
endforeach()
target_link_libraries(CliTest PRIVATE polyir-cli)
target_link_libraries(BuilderTest PRIVATE polyir-cli)
target_link_libraries(GenericsTest PRIVATE polyir-cli)
target_link_libraries(TranslateTest PRIVATE polyir-cli)
target_link_libraries(TranslateSpansTest PRIVATE polyir-cli)
target_link_libraries(NativeGcTest PRIVATE polyir-vm)

# Negative programs with the Nina CLI's answers (tests/data/negative): warnings, every error headline, the kernel rules, the JSON.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME NegativeCases COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/negative_cases.py ${POLYIR_OUTPUT_DIR})
endif()

# The one-page example (examples/Gcd.cpp).
add_executable(polyir_gcd_example examples/Gcd.cpp)
target_link_libraries(polyir_gcd_example PRIVATE polyir)

# polyir-hir: reads HIR JSON (`polyir-check --emit-hir-json`, or another front end's), prints it or compiles it to LLVM IR (or, through
# libpolyir-lang-targets, to a GPU shader).
add_executable(polyir-hir tools/PolyirHir.cpp)
target_link_libraries(polyir-hir PRIVATE polyir-lang-targets polyir)

# polyir-check: the command line over a .poly or .pbc file (RunPolyirCli).
add_executable(polyir-check tools/PolyirCheck.cpp)
target_link_libraries(polyir-check PRIVATE polyir-cli)

