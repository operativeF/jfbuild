
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

target_link_libraries(kextract
  PUBLIC
    msvcrt.lib
)
