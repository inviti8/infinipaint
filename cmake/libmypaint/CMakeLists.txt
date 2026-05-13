# CMake wrapper for libmypaint.
#
# Upstream libmypaint is autotools-only; this wrapper builds it as a static
# library against MSVC / Clang / GCC by providing a stub config.h and running
# the upstream codegen script (generate.py) at configure time. json-c is
# expected to come from Conan via the parent project.
#
# See PHASE1.md §4 and the decisions log entry from 2026-05-07 for why
# libmypaint is vendored here rather than packaged via Conan.

cmake_minimum_required(VERSION 3.22.0)

# Wrapper lives in cmake/libmypaint/ so the deps/libmypaint git submodule
# stays pristine (untouched by us — easier upgrades).
set(LIBMYPAINT_SRC_DIR "${CMAKE_SOURCE_DIR}/deps/libmypaint")
set(LIBMYPAINT_WRAPPER_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(LIBMYPAINT_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${LIBMYPAINT_GEN_DIR}")

if(NOT EXISTS "${LIBMYPAINT_SRC_DIR}/mypaint.h")
    message(FATAL_ERROR
        "libmypaint sources not found at ${LIBMYPAINT_SRC_DIR}. "
        "Run: git submodule update --init deps/libmypaint")
endif()

# --- Generated config.h (autotools substitute) ---
configure_file(
    "${LIBMYPAINT_WRAPPER_DIR}/config.h.in"
    "${LIBMYPAINT_GEN_DIR}/config.h"
    @ONLY
)

# --- Generated brush settings headers (run upstream generate.py) ---
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(LIBMYPAINT_GEN_PUBLIC  "${LIBMYPAINT_GEN_DIR}/mypaint-brush-settings-gen.h")
set(LIBMYPAINT_GEN_PRIVATE "${LIBMYPAINT_GEN_DIR}/brushsettings-gen.h")

add_custom_command(
    OUTPUT "${LIBMYPAINT_GEN_PUBLIC}" "${LIBMYPAINT_GEN_PRIVATE}"
    COMMAND ${Python3_EXECUTABLE}
            "${LIBMYPAINT_SRC_DIR}/generate.py"
            "${LIBMYPAINT_GEN_PUBLIC}"
            "${LIBMYPAINT_GEN_PRIVATE}"
    DEPENDS "${LIBMYPAINT_SRC_DIR}/generate.py"
            "${LIBMYPAINT_SRC_DIR}/brushsettings.json"
    WORKING_DIRECTORY "${LIBMYPAINT_SRC_DIR}"
    COMMENT "Generating libmypaint brush settings headers"
    VERBATIM
)

# --- Static library ---
add_library(mypaint STATIC
    "${LIBMYPAINT_SRC_DIR}/brushmodes.c"
    "${LIBMYPAINT_SRC_DIR}/fifo.c"
    "${LIBMYPAINT_SRC_DIR}/helpers.c"
    "${LIBMYPAINT_SRC_DIR}/mypaint-brush.c"
    "${LIBMYPAINT_SRC_DIR}/mypaint-brush-settings.c"
    "${LIBMYPAINT_SRC_DIR}/mypaint-fixed-tiled-surface.c"
    "${LIBMYPAINT_SRC_DIR}/mypaint-mapping.c"
    "${LIBMYPAINT_SRC_DIR}/mypaint-matrix.c"
    "${LIBMYPAINT_SRC_DIR}/mypaint-rectangle.c"
    "${LIBMYPAINT_SRC_DIR}/mypaint-surface.c"
    "${LIBMYPAINT_SRC_DIR}/mypaint-symmetry.c"
    "${LIBMYPAINT_SRC_DIR}/mypaint-tiled-surface.c"
    "${LIBMYPAINT_SRC_DIR}/mypaint.c"
    "${LIBMYPAINT_SRC_DIR}/operationqueue.c"
    "${LIBMYPAINT_SRC_DIR}/rng-double.c"
    "${LIBMYPAINT_SRC_DIR}/tilemap.c"
    "${LIBMYPAINT_GEN_PUBLIC}"
    "${LIBMYPAINT_GEN_PRIVATE}"
)

set_target_properties(mypaint PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)

target_include_directories(mypaint
    PUBLIC  "${LIBMYPAINT_SRC_DIR}"
            "${LIBMYPAINT_GEN_DIR}"
    PRIVATE "${LIBMYPAINT_SRC_DIR}"
)

# HAVE_CONFIG_H tells some libmypaint .c files to include "config.h" with
# autotools semantics. Always set when building via CMake.
target_compile_definitions(mypaint PRIVATE HAVE_CONFIG_H)

# json-c — pulled in by mypaint-brush.c for serializing brush JSON. Required.
find_package(json-c CONFIG REQUIRED)
target_link_libraries(mypaint PRIVATE json-c::json-c)

if(MSVC)
    # Upstream uses C99 idioms (snprintf, designated initializers, etc.) that
    # MSVC tolerates but warns about loudly. Quiet the C runtime deprecation
    # noise for libmypaint's own translation units only.
    target_compile_definitions(mypaint PRIVATE _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
    target_compile_options(mypaint PRIVATE /wd4244 /wd4267 /wd4305 /wd4101 /wd4146)
else()
    target_compile_options(mypaint PRIVATE -Wno-implicit-function-declaration -Wno-unused-function)
endif()

if(NOT WIN32)
    target_link_libraries(mypaint PUBLIC m)
endif()
