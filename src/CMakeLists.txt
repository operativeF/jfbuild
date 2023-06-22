
set(ENGINE_BASE_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/a-c.cpp
  ${CMAKE_CURRENT_LIST_DIR}/asmprot.cpp
  ${CMAKE_CURRENT_LIST_DIR}/baselayer.cpp
  ${CMAKE_SOURCE_DIR}/kenbuild/src/bstub.cpp
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
  ${CMAKE_CURRENT_LIST_DIR}/screencapture.cpp
  ${CMAKE_CURRENT_LIST_DIR}/scriptfile.cpp
)

set(ENGINE_BASE_SDL_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/sdlayer2.cpp
)

set(WIN_ENGINE_BASE_SRCS
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

if(WIN32)
  list(APPEND BUILD_ENGINE_SRCS ${WIN_ENGINE_BASE_SRCS})
else()
  list(APPEND BUILD_ENGINE_SRCS ${ENGINE_BASE_SDL_SRCS})
endif()

add_library(build_engine STATIC
  ${BUILD_ENGINE_SRCS}
)

if(WIN32)
  set(WINDOWS_LINK_LIBRARIES
    Microsoft::XApoBase
    Microsoft::XAudio2Redist
    ws2_32.lib
    $<$<CONFIG:Debug>:msvcrtd.lib>
    $<$<CONFIG:Release>:msvcrt.lib>
    $<$<CONFIG:RelWithDebInfo>:msvcrt.lib>
    comctl32.lib
    uxtheme.lib
    xinput.lib
    Xinput9_1_0.lib
  )
endif()

target_link_libraries(build_engine
  PUBLIC
    unofficial::libsquish::squish
    ${WINDOWS_LINK_LIBRARIES}
    fmt::fmt
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

set(BUILD_EDITOR_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/startwin_editor.cpp
  ${CMAKE_CURRENT_LIST_DIR}/winlayer.cpp
)

add_executable(build_editor WIN32
  ${BUILD_EDITOR_SRCS}
)

target_compile_definitions(build_editor
  PRIVATE
    -DRENDERTYPEWIN
)

target_link_libraries(build_editor
  PRIVATE
    ${WINDOWS_LINK_LIBRARIES}
    build_engine
    fmt::fmt
)
