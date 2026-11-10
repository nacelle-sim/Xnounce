# Third-party code, pinned. Everything is compiled into the plugin; nothing is
# installed or found on the system except OpenGL and threads.
#
# The presets point FETCHCONTENT_BASE_DIR at build/_deps, so the dev, release
# and asan builds share one download.
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

FetchContent_Declare(xpsdk
    URL https://developer.x-plane.com/wp-content/plugins/code-sample-generation/sdk_zip_files/XPSDK430.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9b
    GIT_SHALLOW TRUE)
FetchContent_Declare(imgwindow
    GIT_REPOSITORY https://github.com/slgoldberg/ImgWindow.git
    GIT_TAG ef979d57cb86e50cb7e98c1036cd9fd4805eccc7)   # v2.2.2: XPLM 4.4b3+ Panel Graphics (global coords)
# libogg, libvorbis and doctest are used as plain sources. SOURCE_SUBDIR points
# at a folder that doesn't exist, so FetchContent downloads them and never runs
# their own CMake.
FetchContent_Declare(ogg
    GIT_REPOSITORY https://github.com/xiph/ogg.git
    GIT_TAG v1.3.6
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _xn_no_cmake)
FetchContent_Declare(vorbis
    GIT_REPOSITORY https://github.com/xiph/vorbis.git
    GIT_TAG v1.3.7
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _xn_no_cmake)
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _xn_no_cmake)
FetchContent_MakeAvailable(xpsdk imgui imgwindow ogg vorbis doctest)

set(XPSDK_DIR ${xpsdk_SOURCE_DIR})
if(EXISTS ${XPSDK_DIR}/SDK)
    set(XPSDK_DIR ${XPSDK_DIR}/SDK)
endif()
set(XN_XPLM_INCLUDES ${XPSDK_DIR}/CHeaders/XPLM ${XPSDK_DIR}/CHeaders/Widgets)
set(XN_XPLM_DEFS XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 XPLM303=1 XPLM400=1 XPLM410=1 XPLM420=1 XPLM430=1)
