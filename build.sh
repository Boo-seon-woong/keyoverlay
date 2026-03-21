#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLCHAIN_DIR="$ROOT_DIR/.toolchain"
TOOLCHAIN_VERSION="20260311"
TOOLCHAIN_NAME="llvm-mingw-${TOOLCHAIN_VERSION}-ucrt-ubuntu-22.04-x86_64"
TOOLCHAIN_ARCHIVE="$TOOLCHAIN_DIR/llvm-mingw.tar.xz"
TOOLCHAIN_URL="https://github.com/mstorsjo/llvm-mingw/releases/download/${TOOLCHAIN_VERSION}/${TOOLCHAIN_NAME}.tar.xz"
TOOLCHAIN_ROOT="$TOOLCHAIN_DIR/$TOOLCHAIN_NAME"
BIN_DIR="$TOOLCHAIN_ROOT/bin"
OUT_DIR="$ROOT_DIR/dist"
OUT_EXE="$OUT_DIR/SecureKeyOverlay.exe"

mkdir -p "$TOOLCHAIN_DIR" "$OUT_DIR"

if [[ ! -d "$TOOLCHAIN_ROOT" ]]; then
  if [[ ! -f "$TOOLCHAIN_ARCHIVE" ]]; then
    python3 - <<'PY'
import urllib.request
url = "https://github.com/mstorsjo/llvm-mingw/releases/download/20260311/llvm-mingw-20260311-ucrt-ubuntu-22.04-x86_64.tar.xz"
out = ".toolchain/llvm-mingw.tar.xz"
urllib.request.urlretrieve(url, out)
print("Downloaded toolchain archive:", out)
PY
  fi

  python3 - <<'PY'
import tarfile
with tarfile.open(".toolchain/llvm-mingw.tar.xz", "r:xz") as t:
    t.extractall(".toolchain")
print("Extracted toolchain to .toolchain/")
PY
fi

"$BIN_DIR/x86_64-w64-mingw32-g++" \
  -std=c++17 \
  -O2 \
  -municode \
  -mwindows \
  -static \
  "$ROOT_DIR/main.cpp" \
  -o "$OUT_EXE" \
  -lcomctl32 -lcomdlg32 -lshell32 -lgdi32 -luser32 -lole32

echo "Build complete: $OUT_EXE"
