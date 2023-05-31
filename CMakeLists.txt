
cmake_minimum_required(VERSION 3.24)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()

set (CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

project(build_engine LANGUAGES CXX)

option(USE_OPENGL_2 "Enable usage of OpenGL 2.x renderer." TRUE)
option(USE_OPENGL_3 "Enable usage of OpenGL 3.x renderer." FALSE)
option(USE_POLYMOST "Enable usage of Polymost rendering." TRUE)
option(USE_ASM "Enable usage of assembly code." TRUE)

if(WIN32)
  set(BUILD_ENGINE_CXXFLAGS "/nologo /Zl /EHsc /std:c++latest /Zc:__cplusplus /utf-8 /Zc:strictStrings-")
  set(CMAKE_CXX_FLAGS "${BUILD_ENGINE_CXXFLAGS}")
endif()

find_package(fmt CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)

if(WIN32)
  find_package(xaudio2redist CONFIG REQUIRED)
endif()

find_package(unofficial-libsquish CONFIG REQUIRED)

include_directories(include)

add_subdirectory(kenbuild)
add_subdirectory(src)
add_subdirectory(tools)
