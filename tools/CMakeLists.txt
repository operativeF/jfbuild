
# set(BIN2C_SRCS
#   ${CMAKE_CURRENT_LIST_DIR}/bin2c.cpp
# )

# add_executable(bin2c
#   ${BIN2C_SRCS}
# )

# target_link_libraries(bin2c
#   PRIVATE
#     msvcrt.lib
#     fmt::fmt-header-only
# )

add_executable(kextract
  kextract.cpp
  ${CMAKE_SOURCE_DIR}/src/compat.cpp
)

if(WIN32)
  set(kextract_libs
    $<$<CONFIG:Debug>:msvcrtd.lib>
    $<$<CONFIG:Release>:msvcrt.lib>
    $<$<CONFIG:RelWithDebInfo>:msvcrtd.lib>)
endif()

target_link_libraries(kextract
  PUBLIC
    ${kextract_libs}
)
