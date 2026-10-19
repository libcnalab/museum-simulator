# =============================================================================
# cna-3d-museum -- a walkable 3D museum that showcases the CNA framework.
# =============================================================================
# CNA is consumed the way cna-template consumes it: as a sibling checkout added
# with add_subdirectory() (CNA exports no CMake package). See README.md.
cmake_minimum_required(VERSION 3.23)

project(CnaMuseum
    VERSION 0.1.0
    DESCRIPTION "An interactive 3D museum demonstrating the CNA (C++ XNA 4.0) framework"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- CNA and its siblings ------------------------------------------------------
# CNA resolves sharp-runtime (and easy-gl / meta-gl for the GL renderers)
# relative to its own source root, so all of them sit side by side:
#   <parent>/cna  <parent>/sharp-runtime  <parent>/easy-gl  <parent>/meta-gl
set(CNA_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cna" CACHE PATH
    "Path to the CNA framework checkout")
if(NOT EXISTS "${CNA_ROOT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "cna-3d-museum: CNA not found at '${CNA_ROOT_DIR}'.\n"
        "Clone it next to this project (see README.md, \"Build requirements\") or pass\n"
        "-DCNA_ROOT_DIR=/path/to/cna.")
endif()

# The renderer is CNA's own cache variable; nothing here is renderer-specific.
# Unset, CNA picks its platform default (OPENGLES3 on Linux). The museum uses
# only GraphicsCapability queries, so any 3D-capable renderer works.

# The museum needs neither CNA's tests, examples, networking nor video playback.
set(CNA_BUILD_TESTS    OFF CACHE BOOL   "Build CNA's own tests")
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL   "Build CNA's example applications")
set(CNA_BUILD_C_API    OFF CACHE BOOL   "Build the CNA C API")
# The Native C API room is a C17 client of CNA's exported C ABI (src/CApi/NativeCProbe.c). It needs CNA's C API library,
# so turning it on turns that on too; turned off, the room says it was not built.
option(MUSEUM_C_API "Build the museum's C17 client of CNA's native C ABI (needs CNA_BUILD_C_API)" ON)
if(MUSEUM_C_API AND NOT EMSCRIPTEN)
    set(CNA_BUILD_C_API ON CACHE BOOL "Build the CNA C API" FORCE)
endif()
# Networking and GamerServices (Microsoft.Xna.Framework.Net / GamerServices) back the "Net and gamer services" wing. They pull in
# ENet and CNA's offline profile service; turn them off for a smaller build (those rooms then say so).
option(MUSEUM_NETWORKING "Demonstrate Microsoft.Xna.Framework.Net and GamerServices" ON)
set(CNA_ENABLE_NET     ${MUSEUM_NETWORKING} CACHE BOOL "Build CNA networking" FORCE)
set(CNA_ENABLE_GAMER_SERVICES ${MUSEUM_NETWORKING} CACHE BOOL "Build CNA GamerServices" FORCE)
# Video playback (VideoPlayer) needs FFmpeg's libraries. AUTO uses them when they are installed; the Video room says so when not.
set(MUSEUM_VIDEO "AUTO" CACHE STRING "Enable FFmpeg video playback (OFF, AUTO or ON)")
set(CNA_ENABLE_VIDEO "${MUSEUM_VIDEO}" CACHE STRING "Enable FFmpeg video playback" FORCE)
# CNA features the museum demonstrates: the CNAEXT graphics extensions (DebugDraw, CRT/ASCII/depth effects,
# shader packages), device/sensor extensions, and CNA's engine statistics counters. All three are optional:
# the exhibits that need them check for them and say so in the room when they are missing.
set(CNA_CNAEXT      ON    CACHE BOOL   "Build CNA's graphics extensions")
set(CNA_DEVICES     ON    CACHE BOOL   "Build CNA's device extensions")
# FULL compiles the CPU zones, markers and event timeline in as well as the counters; the museum starts at runtime level STATS (see
# MuseumGame) so nothing is recorded until a room asks (Instrumenting Your Own Code, The Inspector).
# The museum's diagnostics rooms show the most at FULL, which is the default; OFF and STATS are honest choices too
# (those rooms then say what their level lacks), so the cache value is not forced.
set(CNA_DIAGNOSTICS FULL CACHE STRING "CNA diagnostics level (OFF|STATS|FULL)")
# CNA's optional Inspector (an agent that serves the diagnostics over an authenticated loopback socket, and a client) is a separate module that is
# off in CNA's own default build. The museum links it for the Inspector room; the agent is never started until a visitor presses its button.
option(MUSEUM_INSPECTOR "Link CNA's optional Inspector agent and client for the Inspector room" ON)
set(CNA_BUILD_INSPECTOR ${MUSEUM_INSPECTOR} CACHE BOOL "Build CNA's Inspector agent" FORCE)
set(EASYGL_BUILD_TESTS    OFF CACHE BOOL "Build easy-gl tests")
set(EASYGL_BUILD_EXAMPLES OFF CACHE BOOL "Build easy-gl examples")
set(METAGL_BUILD_TESTS    OFF CACHE BOOL "Build meta-gl tests")

# CNA caches its vendored SDL build in its own checkout by default. Keep it in
# this build tree instead, keyed by toolchain, so the CNA checkout stays
# read-only and a rebuild of the museum never rebuilds SDL.
if(NOT DEFINED CNA_SDL_PREBUILT_ROOT)
    set(CNA_SDL_PREBUILT_ROOT
        "${CMAKE_BINARY_DIR}/cna-sdl-prebuilt-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}"
        CACHE PATH "Persistent CNA SDL build cache, kept outside the CNA checkout")
endif()

# Link only the sharp-runtime components CNA itself needs.
include("${CNA_ROOT_DIR}/cmake/SharpRuntimeConsumption.cmake" OPTIONAL)
if(DEFINED CNA_SHARP_RUNTIME_DEFAULT_COMPONENTS AND NOT DEFINED SHARP_RUNTIME_COMPONENTS)
    set(SHARP_RUNTIME_COMPONENTS "${CNA_SHARP_RUNTIME_DEFAULT_COMPONENTS}"
        CACHE STRING "Sharp Runtime components required by CNA")
endif()

add_subdirectory("${CNA_ROOT_DIR}" cna EXCLUDE_FROM_ALL)

# CMake leaves a subproject's old CTestTestfile.cmake behind when a build tree that once had
# CNA_BUILD_TESTS=ON is reconfigured with it OFF. The museum's top-level CTest file still descends
# into cna/, so that stale file can register obsolete tests (or stale PRE_TEST discovery includes
# can recurse into themselves) before CTest reaches the museum tests. An absent child test file is
# the correct representation of this configuration; a later CNA_BUILD_TESTS=ON configure generates
# it again.
if(NOT CNA_BUILD_TESTS)
    file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/cna/CTestTestfile.cmake")
endif()

# CNA discovers SDL inside its subdirectory. Imported package targets are directory-scoped, so
# create the Emscripten SDL target in the application's directory as well before linking the
# final browser executable.
if(EMSCRIPTEN)
    find_package(SDL3 REQUIRED CONFIG)
endif()

# --- the application -------------------------------------------------------------
# Everything except main() is a static library so tests link the code the
# application runs.
file(GLOB_RECURSE MUSEUM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM MUSEUM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Main.cpp")

if(MUSEUM_C_API AND NOT EMSCRIPTEN)
    list(APPEND MUSEUM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/CApi/NativeCProbe.c")
endif()
add_library(museum_core STATIC ${MUSEUM_SOURCES})
target_include_directories(museum_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(museum_core PUBLIC CNA)
if(MUSEUM_C_API AND NOT EMSCRIPTEN AND TARGET cna_c_api)
    target_link_libraries(museum_core PUBLIC cna_c_api)
    target_compile_definitions(museum_core PUBLIC MUSEUM_C_API=1)
endif()
if(MUSEUM_NETWORKING AND TARGET CNA_Net AND TARGET CNA_GamerServices)
    target_link_libraries(museum_core PUBLIC CNA_Net CNA_GamerServices)
    target_compile_definitions(museum_core PUBLIC MUSEUM_NETWORKING=1)
endif()
# Microsoft.Xna.Framework.Design (the math type converters) is a separate CNA library.
if(TARGET cna_design)
    target_link_libraries(museum_core PUBLIC cna_design)
    target_compile_definitions(museum_core PUBLIC MUSEUM_DESIGN=1)
endif()
# Microsoft.Phone.Shell (PhoneApplicationService) is a small separate CNA library for the application lifetime events of a phone app.
if(TARGET cna_phone)
    target_link_libraries(museum_core PUBLIC cna_phone)
    target_compile_definitions(museum_core PUBLIC MUSEUM_PHONE=1)
endif()
if(MUSEUM_INSPECTOR AND TARGET cna_inspector)
    target_link_libraries(museum_core PUBLIC cna_inspector)
    target_compile_definitions(museum_core PUBLIC MUSEUM_INSPECTOR=1)
endif()
# CNA's content pipeline (IntermediateSerializer and the importers) is a build-time library; the Content Pipeline wing runs it in the game.
option(MUSEUM_PIPELINE "Link CNA's content pipeline library for the Pipeline in the Game room" ON)
if(MUSEUM_PIPELINE AND TARGET cna_content_pipeline)
    target_link_libraries(museum_core PUBLIC cna_content_pipeline)
    target_compile_definitions(museum_core PUBLIC MUSEUM_PIPELINE=1)
endif()
# The CNA revision this build was configured against, shown by the History rooms (read, never typed in).
execute_process(COMMAND git -C "${CNA_ROOT_DIR}" describe --always --dirty
                OUTPUT_VARIABLE MUSEUM_CNA_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT MUSEUM_CNA_REVISION)
    set(MUSEUM_CNA_REVISION "unknown")
endif()
if(EMSCRIPTEN)
    set(_museum_source_content_dir "/Content")
else()
    set(_museum_source_content_dir "${CMAKE_CURRENT_SOURCE_DIR}/Content")
endif()
target_compile_definitions(museum_core PUBLIC
    MUSEUM_CNA_REVISION="${MUSEUM_CNA_REVISION}"
    MUSEUM_VERSION="${PROJECT_VERSION}"
    MUSEUM_SOURCE_CONTENT_DIR="${_museum_source_content_dir}")

# Warnings apply to the museum's own code only. CNA's and sharp-runtime's public
# headers are not warning-clean under -Wextra/-Wpedantic, so their include
# directories are passed as -isystem (GCC and Clang then silence them).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(museum_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
        -Wno-sign-conversion -Wno-overloaded-virtual -Wno-missing-field-initializers)
    file(GLOB _museum_system_includes
        "${CNA_ROOT_DIR}/modules/*/include"
        "${CNA_ROOT_DIR}/modules/renderers/*/include"
        "${CNA_ROOT_DIR}/../sharp-runtime/modules/*/include")
    foreach(_dir IN LISTS _museum_system_includes)
        target_compile_options(museum_core PRIVATE "SHELL:-isystem ${_dir}")
    endforeach()
endif()

add_executable(cna-3d-museum src/Main.cpp)
target_link_libraries(cna-3d-museum PRIVATE museum_core)

# Execution evidence: gcov line counters in the museum's own code only (CNA stays as built), unoptimised so every
# source line keeps its own count. tools/execution_coverage.py runs one process per room and records which lines ran.
option(MUSEUM_COVERAGE "Instrument the museum's own sources with gcov line counters (execution evidence)" OFF)
if(MUSEUM_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "MUSEUM_COVERAGE needs GCC (gcov line counters)")
    endif()
    target_compile_options(museum_core PRIVATE -O0 -g --coverage)
    target_compile_options(cna-3d-museum PRIVATE -O0 -g --coverage)
    target_link_options(museum_core PUBLIC --coverage)
endif()
if(EMSCRIPTEN)
    target_link_libraries(cna-3d-museum PRIVATE SDL3::SDL3-static CNA::EmscriptenAsyncify)
    set_target_properties(cna-3d-museum PROPERTIES
        SUFFIX ".html"
        LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
    target_link_options(cna-3d-museum PRIVATE
        -sALLOW_MEMORY_GROWTH=1
        -sSTACK_SIZE=4194304
        -sINITIAL_MEMORY=268435456
        -sFORCE_FILESYSTEM=1
        -sEXIT_RUNTIME=1
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/Content@/Content"
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/Content/GamerServices@/GamerServices"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
    cna_apply_emscripten_renderer_link_contract(cna-3d-museum)
endif()
if(TARGET SDL3::SDL3main)
    target_link_libraries(cna-3d-museum PRIVATE SDL3::SDL3main)
endif()
if(COMMAND cna_copy_renderer_runtime)
    cna_copy_renderer_runtime(cna-3d-museum)
endif()

if(NOT EMSCRIPTEN)
    # Put Content/ next to the executable so the build tree is self-contained. Web builds preload it
    # into Emscripten's virtual filesystem as /Content before main() starts.
    add_custom_command(TARGET cna-3d-museum POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Content" "$<TARGET_FILE_DIR:cna-3d-museum>/Content"
        COMMENT "Copying Content next to cna-3d-museum")

    # CNA's offline achievement catalog is read from the title directory (next to the executable), not from Content/.
    add_custom_command(TARGET cna-3d-museum POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Content/GamerServices" "$<TARGET_FILE_DIR:cna-3d-museum>/GamerServices"
        COMMENT "Copying GamerServices/Achievements.json next to cna-3d-museum")
endif()

# Run the Content/Pipeline sources through CNA's own content compiler, once as .cnb (CNA's format) and once as
# .xnb (XNA's), next to the executable. The Content Pipeline rooms load the results and compare them with the
# loose source files. If the tool is not available the rooms say so instead of failing. When cross-compiling the tool is a
# foreign executable: it only runs here if CMAKE_CROSSCOMPILING_EMULATOR is set (for example to wine); otherwise the step is
# skipped and Content/Compiled can be copied from a native build tree (the files are the same on every platform). Emscripten's
# Node emulator does not expose host paths in its virtual filesystem, so browser builds exercise the in-game pipeline but do
# not run the cross-built command-line compiler as a post-build host tool.
if(TARGET cna_content_tool AND NOT EMSCRIPTEN AND
   (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR))
    add_dependencies(cna-3d-museum cna_content_tool)
    foreach(_fmt cnb xnb)
        add_custom_command(TARGET cna-3d-museum POST_BUILD
            COMMAND "$<TARGET_FILE:cna_content_tool>" build "${CMAKE_CURRENT_SOURCE_DIR}/Content/Pipeline"
                -o "$<TARGET_FILE_DIR:cna-3d-museum>/Content/Compiled/${_fmt}" --format ${_fmt} --quiet
            COMMENT "Compiling Content/Pipeline to ${_fmt} with cna-content")
    endforeach()
endif()

if(WIN32 AND COMMAND cna_copy_sdl_runtime)
    cna_copy_sdl_runtime(cna-3d-museum)
endif()

# --- tests ---------------------------------------------------------------------------------------
# Structural tests of the museum's data (no window or graphics device needed): ctest --test-dir build
include(CTest)
add_executable(museum_tests EXCLUDE_FROM_ALL tests/museum_tests.cpp)
target_link_libraries(museum_tests PRIVATE museum_core)
add_test(NAME museum_structure COMMAND museum_tests)

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME museum_renderer_log_validator
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_validate_renderer_log.py")
    add_test(NAME museum_api_inventory_unit
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_generate_api_inventory.py")
    add_test(NAME museum_api_coverage_audit_unit
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_audit_api_coverage.py")
    add_test(NAME museum_generated_docs_unit
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gen_docs_check.py")
    add_test(NAME museum_check_steps_unit
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_check_steps.py")
    add_test(NAME museum_coverage_json
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_coverage_json.py"
            --bin "$<TARGET_FILE:cna-3d-museum>")
endif()
