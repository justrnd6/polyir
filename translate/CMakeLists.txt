# ===========================================================================
#  translate — libpolyir-translate, the translate library (core/docs/translate.md): a source language's tree written in a
#  target language by a page of construct rules. It knows no AST: a source hands in its canon (a Schema) and its tree
#  (a SourceTree, include/polyir/translate/Source.hpp). It needs only the standard library (and polyir's header-only Std.hpp).
#  polyir is one source (core/src/translate); nina-lite is another. Built from the root CMakeLists.txt.
#
#  polyir-translatec compiles a page, or a .canon file, into C++ ahead (tools/TranslateC.cpp): polyir_translate_page() runs it
#  at build time, and a problem in the page stops the build.
# ===========================================================================
file(GLOB TRANSLATE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
get_filename_component(TRANSLATE_STD_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../core/include" ABSOLUTE)
set(TRANSLATE_INCLUDES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<BUILD_INTERFACE:${TRANSLATE_STD_INCLUDE}>")

if(POLYIR_BUILD_STATIC AND NOT TARGET polyir_translate_static)
    polyir_static_library(polyir_translate_static ${TRANSLATE_SOURCES})
    target_include_directories(polyir_translate_static PUBLIC ${TRANSLATE_INCLUDES})
endif()

if(NOT POLYIR_ONLY_STATIC)
    polyir_shared_library(polyir-translate ${TRANSLATE_SOURCES})
    target_include_directories(polyir-translate PUBLIC ${TRANSLATE_INCLUDES} "$<INSTALL_INTERFACE:include>")
endif()

# polyir-translatec, built first: it runs while the libraries that hold compiled pages are built.
if(NOT TARGET polyir-translatec)
    add_executable(polyir-translatec tools/TranslateC.cpp)
    target_compile_options(polyir-translatec PRIVATE -Wall -Wextra -Wno-unqualified-std-cast-call)

    if(POLYIR_ONLY_STATIC)
        target_link_libraries(polyir-translatec PRIVATE polyir_translate_static)
    else()
        target_link_libraries(polyir-translatec PRIVATE polyir-translate)
    endif()
endif()

# polyir_translate_page(<sources var> <page or .canon> FUNCTION <name> [SHOWN <path>] [CANON <source.canon>] [DEPENDS <files>…]):
# compiles the page (or canon) into a generated <name>.cpp, defining `const TranslateRules& <name>()` (or `const Schema&`),
# and appends it to <sources var>. A page is checked by its source's canon (CANON, or a .canon beside it) first; DEPENDS names
# what else it reads (its base page). Another project calls it after add_subdirectory()ing polyir.
function(polyir_translate_page sources input)
    cmake_parse_arguments(PAGE "" "FUNCTION;SHOWN;CANON" "DEPENDS" ${ARGN})
    set(output "${CMAKE_CURRENT_BINARY_DIR}/compiled/${PAGE_FUNCTION}.cpp")
    set(arguments ${input} --function ${PAGE_FUNCTION} --out ${output})

    if(PAGE_SHOWN)
        list(APPEND arguments --file ${PAGE_SHOWN})
    endif()

    if(PAGE_CANON)
        list(APPEND arguments --canon ${PAGE_CANON})
    endif()

    add_custom_command(OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/compiled"
        COMMAND polyir-translatec ${arguments}
        DEPENDS polyir-translatec ${input} ${PAGE_CANON} ${PAGE_DEPENDS}
        COMMENT "Compiling ${input}")
    set(${sources} ${${sources}} ${output} PARENT_SCOPE)
endfunction()

if(POLYIR_ONLY_STATIC)
    return()
endif()

# The library's own tests link only the library: a made-up source proves it needs no polyir (SourceTreeTest).
file(GLOB TRANSLATE_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
foreach(test_source ${TRANSLATE_TESTS})
    polyir_test(${test_source} polyir-translate)
endforeach()

# TranslateBench: how fast polyir's corpus is translated (a program run by hand, not a test). It checks .poly files, so it
# links libpolyir.
add_executable(TranslateBench tools/TranslateBench.cpp)
target_link_libraries(TranslateBench PRIVATE polyir)
