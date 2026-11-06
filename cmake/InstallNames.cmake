# Run by `cmake --install` on macOS (the root CMakeLists.txt sets POLYIR_BUILD_DIR and POLYIR_LIBDIR first): each installed dylib is named by its
# installed path, and its references to the other polyir dylibs, which name the build tree, are pointed at the installed copies.
set(lib "${CMAKE_INSTALL_PREFIX}/${POLYIR_LIBDIR}")
foreach(name polyir-translate polyir polyir-vm polyir-lang-targets polyir-cli)
    execute_process(COMMAND install_name_tool -id "${lib}/lib${name}.0.dylib" "${lib}/lib${name}.0.1.0.dylib")
    foreach(other polyir-translate polyir polyir-vm polyir-lang-targets)
        execute_process(COMMAND install_name_tool -change "${POLYIR_BUILD_DIR}/lib${other}.0.dylib" "${lib}/lib${other}.0.dylib" "${lib}/lib${name}.0.1.0.dylib")
    endforeach()
endforeach()
