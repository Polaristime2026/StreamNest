cmake_minimum_required(VERSION 3.20)
project(wasm_player_demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED FFMPEG_WASM_ROOT)
  message(FATAL_ERROR "Please set -DFFMPEG_WASM_ROOT=/path/to/ffmpeg-wasm-prefix")
endif()

add_executable(player
  src/player_api.cpp
  src/ring_buffer.cpp
  src/ffmpeg_wrap.cpp
)

target_include_directories(player PRIVATE
  include
  ${FFMPEG_WASM_ROOT}/include
)

target_link_directories(player PRIVATE ${FFMPEG_WASM_ROOT}/lib)

target_link_libraries(player PRIVATE
  avformat
  avcodec
  avutil
  swresample
  swscale
  z
  m
)

if(EMSCRIPTEN)
  set_target_properties(player PROPERTIES
    OUTPUT_NAME "player"
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../public/wasm
  )

  target_compile_options(player PRIVATE -O3)

  target_link_options(player PRIVATE
    -O3
    -sALLOW_MEMORY_GROWTH=1
    -sFORCE_FILESYSTEM=1
    -sFETCH=1
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sENVIRONMENT=web
    "-sEXPORTED_FUNCTIONS=[\"_malloc\",\"_free\",\"_player_init\",\"_player_feed\",\"_player_decode_step\",\"_player_poll_video_frame\",\"_player_poll_audio\",\"_player_seek_ms\",\"_player_flush\",\"_player_close\",\"_player_get_duration_sec\"]"
    "-sEXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\",\"HEAPU8\",\"HEAPU32\",\"HEAP32\",\"HEAPF32\",\"HEAPF64\",\"_malloc\",\"_free\"]"
  )
endif()
