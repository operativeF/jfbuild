# Create kenbuild test game

set(KENBUILD_GAME_SRCS
  # ${CMAKE_CURRENT_LIST_DIR}/src/bstub.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/src/buildres.rc
  ${CMAKE_CURRENT_LIST_DIR}/src/config.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/game.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/startwin_game.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/kdmsound.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/kdmsound_stub.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/src/kdmsound_sdl2.cpp
  # ${CMAKE_CURRENT_LIST_DIR}/src/kdmsound_xaudio2.cpp
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
  # ${CMAKE_CURRENT_LIST_DIR}/rsrc/game_icon.ico
  # ${CMAKE_CURRENT_LIST_DIR}/rsrc/game_icon.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rsrc/game_icon.bmp
  # ${CMAKE_CURRENT_LIST_DIR}/rsrc/game-x86.manifest
  ${CMAKE_CURRENT_LIST_DIR}/rsrc/game.bmp
  ${CMAKE_CURRENT_LIST_DIR}/rsrc/game.manifest
  # ${CMAKE_CURRENT_LIST_DIR}/rsrc/game.png
)

set(KENBUILD_GAME_DATA
  ${CMAKE_CURRENT_LIST_DIR}/data/ascboard.map
  ${CMAKE_CURRENT_LIST_DIR}/data/boards.map
  ${CMAKE_CURRENT_LIST_DIR}/data/evilal.map
  ${CMAKE_CURRENT_LIST_DIR}/data/kensig.map
  ${CMAKE_CURRENT_LIST_DIR}/data/nsnoal.map
  ${CMAKE_CURRENT_LIST_DIR}/data/nukeland.map
  ${CMAKE_CURRENT_LIST_DIR}/data/stuff.dat
)

add_executable(test_game WIN32
  ${KENBUILD_GAME_SRCS}
  # ${KENBUILD_GAME_RC}
  # ${KENBUILD_GAME_RSRCS}
  # ${KENBUILD_GAME_DATA}
)

target_include_directories(test_game
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/rsrc
    ${CMAKE_CURRENT_LIST_DIR}/data
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(test_game
  PRIVATE
    -DRENDERTYPEWIN
)

add_dependencies(test_game build_engine)

target_link_libraries(test_game
    PUBLIC
      build_engine
)