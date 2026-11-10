# xn_vorbis: the Ogg Vorbis decoder as a static library (PIC, hidden symbols,
# warnings off: it is third-party code).
add_library(xn_vorbis STATIC
    ${ogg_SOURCE_DIR}/src/bitwise.c
    ${ogg_SOURCE_DIR}/src/framing.c
    ${vorbis_SOURCE_DIR}/lib/analysis.c
    ${vorbis_SOURCE_DIR}/lib/bitrate.c
    ${vorbis_SOURCE_DIR}/lib/block.c
    ${vorbis_SOURCE_DIR}/lib/codebook.c
    ${vorbis_SOURCE_DIR}/lib/envelope.c
    ${vorbis_SOURCE_DIR}/lib/floor0.c
    ${vorbis_SOURCE_DIR}/lib/floor1.c
    ${vorbis_SOURCE_DIR}/lib/info.c
    ${vorbis_SOURCE_DIR}/lib/lookup.c
    ${vorbis_SOURCE_DIR}/lib/lpc.c
    ${vorbis_SOURCE_DIR}/lib/lsp.c
    ${vorbis_SOURCE_DIR}/lib/mapping0.c
    ${vorbis_SOURCE_DIR}/lib/mdct.c
    ${vorbis_SOURCE_DIR}/lib/psy.c
    ${vorbis_SOURCE_DIR}/lib/registry.c
    ${vorbis_SOURCE_DIR}/lib/res0.c
    ${vorbis_SOURCE_DIR}/lib/sharedbook.c
    ${vorbis_SOURCE_DIR}/lib/smallft.c
    ${vorbis_SOURCE_DIR}/lib/synthesis.c
    ${vorbis_SOURCE_DIR}/lib/vorbisfile.c
    ${vorbis_SOURCE_DIR}/lib/window.c)
target_include_directories(xn_vorbis PUBLIC
    ${ogg_SOURCE_DIR}/include
    ${vorbis_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/compat/ogg)
target_include_directories(xn_vorbis PRIVATE ${vorbis_SOURCE_DIR}/lib)
if(MSVC)
    target_compile_definitions(xn_vorbis PRIVATE _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
    target_compile_options(xn_vorbis PRIVATE /W0)
else()
    target_compile_options(xn_vorbis PRIVATE -w)
    if(NOT APPLE)
        target_link_libraries(xn_vorbis PUBLIC m)
    endif()
endif()
