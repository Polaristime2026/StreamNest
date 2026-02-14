#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$ROOT_DIR/public/wasm"
CPP_DIR="$ROOT_DIR/cpp"

EMXX="${EMXX:-em++}"
FFMPEG_WASM_ROOT="${FFMPEG_WASM_ROOT:-}"

if [[ -z "$FFMPEG_WASM_ROOT" ]]; then
  echo "[build_wasm] 请先设置 FFMPEG_WASM_ROOT，例如:"
  echo "  export FFMPEG_WASM_ROOT=/abs/path/to/ffmpeg-wasm-prefix"
  exit 1
fi

mkdir -p "$OUT_DIR"

SRC=(
  "$CPP_DIR/src/player_api.cpp"
  "$CPP_DIR/src/ring_buffer.cpp"
  "$CPP_DIR/src/ffmpeg_wrap.cpp"
)

INCLUDE=(
  -I"$CPP_DIR/include"
  -I"$FFMPEG_WASM_ROOT/include"
)

LIBDIR=(
  -L"$FFMPEG_WASM_ROOT/lib"
)

LIBS=(
  -lavformat
  -lavcodec
  -lavutil
  -lswresample
  -lswscale
  -lz
  -lm
)

EXPORTED_FUNCTIONS='["_malloc","_free","_player_init","_player_feed","_player_decode_step","_player_poll_video_frame","_player_poll_audio","_player_seek_ms","_player_flush","_player_close","_player_get_duration_sec"]'
EXPORTED_RUNTIME='["ccall","cwrap","HEAPU8","HEAPU32","HEAP32","HEAPF32","HEAPF64","_malloc","_free"]'

"$EMXX" \
  "${SRC[@]}" \
  "${INCLUDE[@]}" \
  "${LIBDIR[@]}" \
  "${LIBS[@]}" \
  -O3 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s FORCE_FILESYSTEM=1 \
  -s FETCH=1 \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s ENVIRONMENT=web \
  -s EXPORTED_FUNCTIONS="$EXPORTED_FUNCTIONS" \
  -s EXPORTED_RUNTIME_METHODS="$EXPORTED_RUNTIME" \
  -o "$OUT_DIR/player.js"

echo "[build_wasm] done: $OUT_DIR/player.js + player.wasm"
