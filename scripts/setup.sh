#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "==> Installing system dependencies..."
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  git \
  libcurl4-openssl-dev \
  python3

echo "==> Cloning rpi-rgb-led-matrix..."
mkdir -p third_party
if [ ! -d third_party/rpi-rgb-led-matrix/.git ]; then
  git clone --depth 1 https://github.com/hzeller/rpi-rgb-led-matrix.git \
    third_party/rpi-rgb-led-matrix
fi

echo "==> Downloading nlohmann/json..."
mkdir -p third_party/json/include/nlohmann
if [ ! -f third_party/json/include/nlohmann/json.hpp ]; then
  curl -fsSL \
    https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp \
    -o third_party/json/include/nlohmann/json.hpp
fi

echo "==> Building..."
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

echo ""
echo "Done. Run with:"
echo "  sudo ./build/heathrow-flights --led-rows=64 --led-cols=64 --led-gpio-mapping=adafruit-hat"
