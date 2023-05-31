
set(ENGINE_BASE_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/a-c.cpp
  ${CMAKE_CURRENT_LIST_DIR}/asmprot.cpp
  ${CMAKE_CURRENT_LIST_DIR}/baselayer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/build.cpp
  ${CMAKE_CURRENT_LIST_DIR}/cache1d.cpp
  ${CMAKE_CURRENT_LIST_DIR}/compat.cpp
  ${CMAKE_CURRENT_LIST_DIR}/config.cpp
  ${CMAKE_CURRENT_LIST_DIR}/crc32.cpp
  ${CMAKE_CURRENT_LIST_DIR}/defs.cpp
  ${CMAKE_CURRENT_LIST_DIR}/engine.cpp
  ${CMAKE_CURRENT_LIST_DIR}/kplib.cpp
  ${CMAKE_CURRENT_LIST_DIR}/mmulti.cpp
  ${CMAKE_CURRENT_LIST_DIR}/osd.cpp
  ${CMAKE_CURRENT_LIST_DIR}/pragmas.cpp
  ${CMAKE_CURRENT_LIST_DIR}/scriptfile.cpp
  ${CMAKE_CURRENT_LIST_DIR}/winlayer.cpp
)

set(ASM_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/a.masm
)

set(GLBUILD_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/glbuild.cpp
)

set(POLYMOST_BUILD_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/polymost.cpp
)

set(OPENGL_ENGINE_BUILD_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/hightile.cpp
  ${CMAKE_CURRENT_LIST_DIR}/mdsprite.cpp
  ${CMAKE_CURRENT_LIST_DIR}/polymosttex.cpp
  ${CMAKE_CURRENT_LIST_DIR}/polymosttexcache.cpp
  ${CMAKE_CURRENT_LIST_DIR}/polymosttexcompress.cc
  ${CMAKE_CURRENT_LIST_DIR}/rg_etc1.cpp
)

# TODO: Add to own, separate directory.
set(EDITOR_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/startwin_editor.cpp
  ${CMAKE_CURRENT_LIST_DIR}/config.cpp
)

set(MACOS_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/EditorStartupWinController.m
)

set(BUILD_ENGINE_SRCS ${ENGINE_BASE_SRCS})

if(USE_OPENGL_2 OR USE_OPENGL_3)
  list(APPEND BUILD_ENGINE_SRCS
    ${OPENGL_ENGINE_BUILD_SRCS}
    ${GLBUILD_SRCS})
endif()

if(USE_POLYMOST)
  list(APPEND BUILD_ENGINE_SRCS ${POLYMOST_BUILD_SRCS})
endif()

if(USE_ASM)
  list(APPEND BUILD_ENGINE_SRCS ${ASM_SRCS})
endif()

add_library(build_engine STATIC
  ${BUILD_ENGINE_SRCS}
)

target_link_libraries(build_engine
  PUBLIC
    unofficial::libsquish::squish
    Microsoft::XApoBase
    Microsoft::XAudio2Redist
    ws2_32.lib
    msvcrt.lib
    comctl32.lib
    uxtheme.lib
    xinput.lib
    Xinput9_1_0.lib
)

target_include_directories(build_engine
  PUBLIC
    unofficial::libsquish::squish
    ${CMAKE_CURRENT_LIST_DIR}
)

# TODO: Propagate these options?
target_compile_definitions(build_engine
  PUBLIC
  -DUSE_OPENGL=3
  -DUSE_POLYMOST=1
)
