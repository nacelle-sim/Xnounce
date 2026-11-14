cmake_minimum_required(VERSION 3.21)
project(Xnounce VERSION 1.0.0 LANGUAGES C CXX)

# -----------------------------------------------------------------------------
# Xnounce: automatic cabin announcements from Fenix-format Airline Packs.
#
# Targets
#   xn_core    src/core            pure C++: packs, selection, purser, settings
#   xn_audio   src/audio (pure)    decoder, decode cache, muffle, PA sound, hearing, chime
#   xn_ui      src/ui (pure)       the panel, ImGui only
#   Xnounce    the plugin          the libraries plus everything that needs XPLM
#   xn_tests, xn_ui_tests          unit tests and the headless UI test
#
# The pure libraries are built without the X-Plane SDK on their include path,
# so they cannot start to depend on it by accident (docs/ARCHITECTURE.md § 1).
#
# Output (an X-Plane "fat plugin"):
#   <build>/Xnounce/{lin_x64,win_x64,mac_x64}/Xnounce.xpl
#
# Day-to-day use goes through the presets: see docs/DEVELOPING.md.
# -----------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

option(XN_BUILD_TESTS "Build the unit tests and the headless UI test" ON)
option(XN_STATIC_RUNTIME "Linux: link libstdc++/libgcc statically (release builds)" OFF)
option(XN_PANEL_GRAPHICS "Render via Panel Graphics on XP 12.4.4+ (dynamic, OpenGL fallback)" ON)
# A release candidate carries its suffix in the version it reports (CI passes -rc.N from a v1.0.0-rc.N tag).
set(XN_VERSION_SUFFIX "" CACHE STRING "Appended to the version the plugin reports, for example -rc.1")
set(XN_VERSION "${PROJECT_VERSION}${XN_VERSION_SUFFIX}")
set(XN_SANITIZE "" CACHE STRING "Sanitizers for a test build, for example: address,undefined")
set(XN_LOCAL_DATA "" CACHE PATH
    "Folder with local test data (packs/, traces/, golden/). Empty: the tests that need it skip")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(XnDependencies)
include(XnVorbis)
find_package(Threads REQUIRED)

if(XN_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=${XN_SANITIZE} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${XN_SANITIZE})
endif()

# Our own code is built with warnings on; third-party code with them off.
function(xn_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
    endif()
endfunction()

# ---- xn_core ----------------------------------------------------------------
# data/airlines.tsv is embedded as one string literal: keep it under MSVC's
# 16 KB limit for a literal.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/data/airlines.tsv XN_AIRLINES_TSV)
string(LENGTH "${XN_AIRLINES_TSV}" XN_AIRLINES_LEN)
if(XN_AIRLINES_LEN GREATER 16000)
    message(FATAL_ERROR "data/airlines.tsv is ${XN_AIRLINES_LEN} bytes; split the embedding in airlines_data.cpp.in")
endif()
configure_file(src/core/airlines_data.cpp.in ${CMAKE_BINARY_DIR}/generated/airlines_data.cpp @ONLY)

add_library(xn_core STATIC
    src/core/aircraft_type.cpp
    src/core/airlines.cpp
    src/core/announcement.cpp
    src/core/cabin_crew/action_tracker.cpp
    src/core/cabin_crew/cabin_crew.cpp
    src/core/camera.cpp
    src/core/files.cpp
    src/core/ini.cpp
    src/core/guard.cpp
    src/core/log.cpp
    src/core/pack_library.cpp
    src/core/pack_resolver.cpp
    src/core/purser/arrival.cpp
    src/core/purser/boarding.cpp
    src/core/purser/departure.cpp
    src/core/purser/flight.cpp
    src/core/purser/labels.cpp
    src/core/purser/purser.cpp
    src/core/purser/queue.cpp
    src/core/purser/signals.cpp
    src/core/recorder.cpp
    src/core/refuel.cpp
    src/core/selector.cpp
    src/core/settings.cpp
    src/core/simbrief.cpp
    src/core/sound_file.cpp
    src/core/strings.cpp
    ${CMAKE_BINARY_DIR}/generated/airlines_data.cpp)
target_include_directories(xn_core PUBLIC src)
target_link_libraries(xn_core PUBLIC Threads::Threads)
xn_warnings(xn_core)

# ---- xn_audio: the part of audio that needs no X-Plane ------------------------
add_library(xn_audio STATIC
    src/audio/chime.cpp
    src/audio/decode_cache.cpp
    src/audio/decoder.cpp
    src/audio/fmod_events.cpp
    src/audio/hearing.cpp
    src/audio/muffle.cpp
    src/audio/pa_effects.cpp
    src/audio/pa_power.cpp)
target_link_libraries(xn_audio PUBLIC xn_core PRIVATE xn_vorbis)
xn_warnings(xn_audio)

# ---- xn_aircraft: the part of the aircraft modules that needs no X-Plane -------
# Types, settings, signal decoding and recognition rules. The modules
# themselves (which read datarefs) are part of the plugin target.
add_library(xn_aircraft STATIC
    src/aircraft/options.cpp
    src/aircraft/trace_compat.cpp
    src/aircraft/toliss/toliss_options.cpp
    src/aircraft/toliss/toliss_signals.cpp
    src/aircraft/toliss/toliss_types.cpp
    src/aircraft/zibo737/zibo737_crew.cpp
    src/aircraft/zibo737/zibo737_identity.cpp
    src/aircraft/zibo737/zibo737_options.cpp
    src/aircraft/zibo737/zibo737_signals.cpp
    src/aircraft/zibo737/zibo737_types.cpp)
target_link_libraries(xn_aircraft PUBLIC xn_core)
xn_warnings(xn_aircraft)

# ---- xn_ui: the panel (Dear ImGui only) ----------------------------------------
# Built twice. The plugin gets the plain build. The headless UI test gets a
# build with assertions always on (-UNDEBUG), so a release build still tests
# with ImGui's own checks (docs/ARCHITECTURE.md § 6).
set(XN_IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
set(XN_UI_SOURCES
    src/ui/theme.cpp
    src/ui/fonts_b612.cpp
    src/ui/widgets.cpp
    src/ui/panel.cpp
    src/ui/tab_status.cpp
    src/ui/tab_timeline.cpp
    src/ui/tab_packs.cpp
    src/ui/tab_settings.cpp
    src/ui/tab_log.cpp)

function(xn_add_ui suffix)
    add_library(xn_imgui${suffix} STATIC ${XN_IMGUI_SOURCES} ${ARGN})
    target_include_directories(xn_imgui${suffix} PUBLIC ${imgui_SOURCE_DIR})
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(xn_imgui${suffix} PRIVATE -w)
    endif()
    add_library(xn_ui${suffix} STATIC ${XN_UI_SOURCES})
    target_include_directories(xn_ui${suffix} PRIVATE fonts)
    target_link_libraries(xn_ui${suffix} PUBLIC xn_core xn_imgui${suffix})
    xn_warnings(xn_ui${suffix})
endfunction()
xn_add_ui("")

# ---- xn_app: the services between the parts, without X-Plane -------------------
add_library(xn_app STATIC
    src/app/flight_plan_service.cpp
    src/app/pack_service.cpp
    src/app/settings_store.cpp
    src/app/ui_presenter.cpp)
target_link_libraries(xn_app PUBLIC xn_core xn_audio xn_aircraft)
xn_warnings(xn_app)

# ---- the plugin -------------------------------------------------------------------
add_library(Xnounce SHARED
    src/plugin.cpp
    src/app/app.cpp
    src/app/commands.cpp
    src/aircraft/registry.cpp
    src/aircraft/xp_common.cpp
    src/aircraft/toliss/toliss_module.cpp
    src/aircraft/zibo737/zibo737_actions.cpp
    src/aircraft/zibo737/zibo737_module.cpp
    src/sim/fmod_studio.cpp
    src/sim/loaded_aircraft.cpp
    src/sim/simbrief_source.cpp
    src/audio/player.cpp
    src/ui/fonts.cpp
    src/ui/window.cpp
    ${imgwindow_SOURCE_DIR}/ImgWindow.cpp
    ${imgwindow_SOURCE_DIR}/ImgFontAtlas.cpp)
target_include_directories(Xnounce PRIVATE ${imgwindow_SOURCE_DIR} fonts ${XN_XPLM_INCLUDES})
target_link_libraries(Xnounce PRIVATE xn_app xn_aircraft xn_core xn_audio xn_ui xn_imgui)
target_compile_definitions(Xnounce PRIVATE ${XN_XPLM_DEFS} XN_VERSION="${XN_VERSION}")
if(XN_PANEL_GRAPHICS)
    target_compile_definitions(Xnounce PRIVATE IMGWINDOW_USE_PANEL_GRAPHICS)
endif()
xn_warnings(Xnounce)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(
        ${imgwindow_SOURCE_DIR}/ImgWindow.cpp ${imgwindow_SOURCE_DIR}/ImgFontAtlas.cpp
        PROPERTIES COMPILE_OPTIONS "-w")
endif()

if(WIN32)
    target_compile_definitions(Xnounce PRIVATE IBM=1 WIN32_LEAN_AND_MEAN NOMINMAX _USE_MATH_DEFINES)
    target_link_libraries(Xnounce PRIVATE ${XPSDK_DIR}/Libraries/Win/XPLM_64.lib opengl32)
    # MSVC has no <GL/glext.h>. ImgWindow needs only GL 1.1, so an empty shim
    # satisfies its include.
    target_include_directories(Xnounce PRIVATE compat/win)
    set(XN_PLATFORM_DIR win_x64)
elseif(APPLE)
    target_compile_definitions(Xnounce PRIVATE APL=1 GL_SILENCE_DEPRECATION)
    find_library(XPLM_FRAMEWORK XPLM PATHS ${XPSDK_DIR}/Libraries/Mac NO_DEFAULT_PATH)
    find_library(OPENGL_FRAMEWORK OpenGL)
    target_link_libraries(Xnounce PRIVATE ${XPLM_FRAMEWORK} ${OPENGL_FRAMEWORK})
    set(XN_PLATFORM_DIR mac_x64)
else()
    target_compile_definitions(Xnounce PRIVATE LIN=1)
    find_package(OpenGL REQUIRED)
    target_link_libraries(Xnounce PRIVATE OpenGL::GL ${CMAKE_DL_LIBS})
    # X-Plane resolves the XPLM symbols when it loads the plugin. Only the
    # XPlugin* entry points are exported.
    target_link_options(Xnounce PRIVATE -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/exports.map
        $<$<CONFIG:Release>:-s>)
    # Release builds link the C++ runtime statically, because a user's distro
    # often ships an older libstdc++ than the build machine. It needs the
    # libstdc++ static archive, so it is an option that CI turns on.
    if(XN_STATIC_RUNTIME)
        target_link_options(Xnounce PRIVATE -static-libstdc++ -static-libgcc)
    endif()
    set(XN_PLATFORM_DIR lin_x64)
endif()

set_target_properties(Xnounce PROPERTIES
    PREFIX ""
    OUTPUT_NAME Xnounce
    SUFFIX ".xpl"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Xnounce/${XN_PLATFORM_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Xnounce/${XN_PLATFORM_DIR})

# ---- API reference -----------------------------------------------------------------
# "apidocs" builds the HTML reference into build/apidocs when Doxygen is
# installed. It is not part of "all".
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    add_custom_target(apidocs
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "API reference: build/apidocs/html/index.html"
        VERBATIM)
endif()

# ---- tests -------------------------------------------------------------------------
if(XN_BUILD_TESTS)
    enable_testing()

    set(XN_TEST_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    set(XN_TEST_TMP_DIR ${CMAKE_BINARY_DIR}/test-tmp)
    file(TO_CMAKE_PATH "${XN_LOCAL_DATA}" XN_LOCAL_DATA_DIR)
    configure_file(tests/support/paths_config.h.in ${CMAKE_BINARY_DIR}/generated/paths_config.h @ONLY)

    # The purser replay: the xn_replay command and the golden stories.
    add_library(xn_replay_lib STATIC tools/replay/replay.cpp)
    target_include_directories(xn_replay_lib PUBLIC tools/replay)
    target_link_libraries(xn_replay_lib PUBLIC xn_aircraft xn_core)
    xn_warnings(xn_replay_lib)
    add_executable(xn_replay tools/replay/main.cpp)
    target_link_libraries(xn_replay PRIVATE xn_replay_lib)
    xn_warnings(xn_replay)

    # Unit tests: pure C++, no X-Plane.
    add_executable(xn_tests
        tests/test_main.cpp
        tests/core/test_cabin_crew.cpp
        tests/core/test_core.cpp
        tests/core/test_packs.cpp
        tests/core/test_purser.cpp
        tests/core/test_settings.cpp
        tests/core/test_simbrief.cpp
        tests/audio/test_audio.cpp
        tests/audio/test_hearing.cpp
        tests/audio/test_fmod_events.cpp
        tests/audio/test_pa_sound.cpp
        tests/aircraft/test_toliss.cpp
        tests/aircraft/test_zibo737.cpp
        tests/aircraft/test_zibo737_crew.cpp
        tests/aircraft/test_zibo737_flights.cpp
        tests/app/test_settings_store.cpp
        tests/app/test_services.cpp)
    target_include_directories(xn_tests PRIVATE tests ${CMAKE_BINARY_DIR}/generated ${doctest_SOURCE_DIR})
    target_link_libraries(xn_tests PRIVATE xn_replay_lib xn_app xn_aircraft xn_core xn_audio)
    # DOCTEST_CONFIG_USE_STD_HEADERS: doctest otherwise forward-declares
    # std::ostream, and MSVC then can't print a std::string_view in a failed
    # CHECK (C2027 "undefined type basic_ostream").
    target_compile_definitions(xn_tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
    xn_warnings(xn_tests)

    # The replay of the developer's recorded flights is its own CTest entry,
    # registered only when local data is configured.
    add_test(NAME unit COMMAND xn_tests "--test-case-exclude=*recorded traces*")
    if(XN_LOCAL_DATA)
        add_test(NAME recorded-flights COMMAND xn_tests "--test-case=*recorded traces*")
    endif()

    # Headless UI test: every tab, many frames, ImGui's assertions on.
    xn_add_ui(_checked ${imgui_SOURCE_DIR}/backends/imgui_impl_null.cpp)
    target_include_directories(xn_imgui_checked PUBLIC ${imgui_SOURCE_DIR}/backends)
    # MSVC accepts -U as well and warns D9025 about overriding /DNDEBUG.
    target_compile_options(xn_imgui_checked PUBLIC -UNDEBUG)
    add_executable(xn_ui_tests tests/ui/test_ui.cpp)
    target_include_directories(xn_ui_tests PRIVATE fonts)
    target_link_libraries(xn_ui_tests PRIVATE xn_ui_checked xn_aircraft)
    add_test(NAME ui COMMAND xn_ui_tests)

    # The UI benchmark (tools/ui_bench): frame times with large data. Not a test; run it by hand.
    xn_add_ui(_perf ${imgui_SOURCE_DIR}/backends/imgui_impl_null.cpp)
    target_include_directories(xn_imgui_perf PUBLIC ${imgui_SOURCE_DIR}/backends)
    set_target_properties(xn_imgui_perf xn_ui_perf PROPERTIES EXCLUDE_FROM_ALL ON)
    add_executable(xn_ui_bench EXCLUDE_FROM_ALL tools/ui_bench/ui_bench.cpp)
    target_include_directories(xn_ui_bench PRIVATE fonts)
    target_link_libraries(xn_ui_bench PRIVATE xn_ui_perf xn_aircraft)
endif()
