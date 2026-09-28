cmake_minimum_required(VERSION 3.20)
project(R-Type CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTING "Build unit tests" ON)

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
  target_include_directories(asio SYSTEM INTERFACE
                             "${asio_SOURCE_DIR}/asio/include")

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

# --- GoogleTest -------------------------------------------------------------

if(BUILD_TESTING)
  enable_testing()

  # Windows: use the shared CRT so gtest matches the app's runtime settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

  CPMAddPackage(
    NAME googletest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
  )

  include(GoogleTest)
endif()

# ---------------------------------------------------------------------------
# Shared build settings
# ---------------------------------------------------------------------------

# Warning flags live on an INTERFACE target so they only apply to our own
# targets (never to raylib/asio/googletest) and work for header-only libs too.
add_library(rtype_warnings INTERFACE)
target_compile_options(rtype_warnings INTERFACE
  -Wall -Wextra -Wpedantic -pedantic-errors
)

# Applies platform-specific link settings to one of our executables.
function(rtype_configure_executable target)
  target_link_libraries(${target} PRIVATE rtype_warnings)

  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Raylib/GLFW needs these system libs on Windows
    target_link_libraries(${target} PRIVATE winmm gdi32)

    # Statically link the mingw runtime so the .exe doesn't need extra DLLs
    target_link_options(${target} PRIVATE
      -static-libgcc -static-libstdc++ -static
    )
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Core library (everything except main.cpp, so tests can link against it)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE RTYPE_CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/*.cpp"
)
list(FILTER RTYPE_CORE_SOURCES EXCLUDE REGEX "/src/main\\.cpp$")

if(RTYPE_CORE_SOURCES)
  add_library(rtype_core STATIC ${RTYPE_CORE_SOURCES})
  target_link_libraries(rtype_core PRIVATE rtype_warnings)
  set(_core_scope PUBLIC)
else()
  # Nothing besides main.cpp yet: header-only placeholder so the
  # target structure is already in place once you add sources.
  add_library(rtype_core INTERFACE)
  set(_core_scope INTERFACE)
endif()

target_include_directories(rtype_core ${_core_scope} "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(rtype_core ${_core_scope} raylib asio)

# ---------------------------------------------------------------------------
# Application target
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE rtype_core)
rtype_configure_executable(${PROJECT_NAME})

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
  file(GLOB_RECURSE RTYPE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tests/*.cpp"
  )

  if(RTYPE_TEST_SOURCES)
    add_executable(rtype_tests ${RTYPE_TEST_SOURCES})
    target_link_libraries(rtype_tests PRIVATE rtype_core GTest::gtest_main)
    rtype_configure_executable(rtype_tests)

    gtest_discover_tests(rtype_tests)
  endif()
endif()
