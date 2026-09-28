cmake_minimum_required(VERSION 3.20)
project(R-Type CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Dependencies via CPM
# ---------------------------------------------------------------------------

# Keep downloaded sources outside the build dir so a clean rebuild
# doesn't re-fetch anything (only recompiles). devenv sets CPM_SOURCE_CACHE;
# this fallback only applies when building outside devenv.
if(NOT DEFINED CPM_SOURCE_CACHE AND NOT DEFINED ENV{CPM_SOURCE_CACHE})
  set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/.cache/CPM" CACHE PATH "")
endif()

include(cmake/CPM.cmake)

# --- raylib ---------------------------------------------------------------

# Trim raylib down to just the library - no bundled examples/games
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Builds raylib with Wayland or X11
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
  set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
endif()

CPMAddPackage(
  NAME raylib
  GITHUB_REPOSITORY raysan5/raylib
  GIT_TAG 6.0
  GIT_SHALLOW TRUE
)

# --- asio (standalone, header-only) ----------------------------------------

# No CMake support upstream, so download only and wrap the headers
# in an INTERFACE target.
CPMAddPackage(
  NAME asio
  GITHUB_REPOSITORY chriskohlhoff/asio
  GIT_TAG asio-1-38-2
  GIT_SHALLOW TRUE
  DOWNLOAD_ONLY YES
)

if(asio_ADDED)
  add_library(asio INTERFACE)

  # SYSTEM: keeps -Wpedantic -pedantic-errors from tripping on Asio's headers
  target_include_directories(asio SYSTEM INTERFACE "${asio_SOURCE_DIR}/asio/include")

  # Use standalone Asio (no Boost)
  target_compile_definitions(asio INTERFACE ASIO_STANDALONE)

  find_package(Threads REQUIRED)
  target_link_libraries(asio INTERFACE Threads::Threads)

  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Winsock, required by Asio on Windows
    target_link_libraries(asio INTERFACE ws2_32 mswsock)
    # Target Windows 10+ APIs (silences Asio's _WIN32_WINNT warning)
    target_compile_definitions(asio INTERFACE _WIN32_WINNT=0x0A00)
  endif()
endif()

# ---------------------------------------------------------------------------
# Application target
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib asio)

target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall -Wextra -Wpedantic -pedantic-errors
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  # Raylib/GLFW needs these system libs on Windows
  target_link_libraries(${PROJECT_NAME} PRIVATE winmm gdi32)

  # Statically link the mingw runtime so the .exe doesn't need extra DLLs
  target_link_options(${PROJECT_NAME} PRIVATE
    -static-libgcc -static-libstdc++ -static
  )
endif()
