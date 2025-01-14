cmake_minimum_required(VERSION 3.10)
project(Tetrino VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Tetrino main-console.cpp)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)
find_package(SDL2 QUIET)
find_package(SDL2_ttf QUIET)

include(FetchContent)
FetchContent_Declare(
  fonts
  URL                 https://github.com/google/fonts/raw/5c3d8ef085f3084db38936d0dcd39a567dbc1e01/ofl/pressstart2p/PressStart2P-Regular.ttf
  URL_HASH            SHA1=53d0ade1ecbf01bb7237f435c6f524f62bd30b9f
  DOWNLOAD_NO_EXTRACT TRUE
  DOWNLOAD_DIR        ${CMAKE_CURRENT_BINARY_DIR}
  DOWNLOAD_NAME       font.ttf
)

if (SDL2_FOUND)
if (SDL2_TTF_FOUND)
  FetchContent_MakeAvailable(fonts)
  add_executable(TetrinoSDL main-sdl.cpp)
  target_include_directories(TetrinoSDL PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(TetrinoSDL SDL2::Main SDL2::TTF)
endif()
endif()