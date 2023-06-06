# Create kenbuild test game

set(KENBUILD_GAME_SRCS
  # ${CMAKE_CURRENT_LIST_DIR}/src/bstub.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/src/buildres.rc
  ${CMAKE_CURRENT_LIST_DIR}/src/config.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/game.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/kdmsound.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/kdmsound_stub.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/src/kdmsound_sdl2.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/src/kdmsound_xaudio2.cpp
)

set(KENBUILD_GAME_WIN_SRCS
  ${CMAKE_CURRENT_LIST_DIR}/src/startwin_game.cpp
)

set(KENBUILD_GAME_RC
  ${CMAKE_CURRENT_LIST_DIR}/src/gameres.rc
)

set(KENBUILD_GAME_RSRCS
  ${CMAKE_CURRENT_LIST_DIR}/rsrc/build_icon.bmp
  # ${CMAKE_CURRENT_LIST_DIR}/rsrc/build_icon.ico
  # ${CMAKE_CURRENT_LIST_DIR}/rsrc/build-x86.manifest
  ${CMAKE_CURRENT_LIST_DIR}/rsrc/build.bmp
  # ${CMAKE_CURRENT_LIST_DIR}/rsrc/build.manifest
  # ${CMAKE_CURRENT_LIST_DIR}/rsrc/build.png
  ${CMAKE_CURRENT_LIST_DIR}/rsrc/game_icon.ico
  ${CMAKE_CURRENT_LIST_DIR}/rsrc/game_icon.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rsrc/game_icon.bmp
  # ${CMAKE_CURRENT_LIST_DIR}/rsrc/game-x86.manifest
  ${CMAKE_CURRENT_LIST_DIR}/rsrc/game.bmp
  ${CMAKE_CURRENT_LIST_DIR}/rsrc/game.manifest
  # ${CMAKE_CURRENT_LIST_DIR}/rsrc/game.png
)

if(WIN32)
  add_executable(test_game WIN32
    ${KENBUILD_GAME_SRCS}
    ${KENBUILD_GAME_WIN_SRCS}
    # ${KENBUILD_GAME_RC}
    # ${KENBUILD_GAME_RSRCS}
    # ${KENBUILD_GAME_DATA}
  )
else()
  add_executable(test_game
    ${KENBUILD_GAME_SRCS}
    # ${KENBUILD_GAME_RC}
    ${KENBUILD_GAME_RSRCS}
    # ${KENBUILD_GAME_DATA}
  )
endif()

target_include_directories(test_game
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/rsrc
    ${CMAKE_CURRENT_LIST_DIR}/data
    ${CMAKE_SOURCE_DIR}/include
)

if(WIN32)
  set(WIN_COMPILE_OPTIONS
    -DRENDERTYPEWIN
  )
endif()

target_compile_options(test_game
  PRIVATE
    ${WIN_COMPILE_OPTIONS}
    -DUSE_OPENGL=3
    -DUSE_POLYMOST=1
)

add_dependencies(test_game build_engine)

target_link_libraries(test_game
    PUBLIC
      build_engine
)