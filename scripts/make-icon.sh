#!/bin/sh
# Build icon.png / .ico / .icns from image.png without Python.
set -e
cd "$(dirname "$0")/.."
SRC="${1:-image.png}"
if [ ! -f "${SRC}" ]; then
  echo "ERROR: source image not found: ${SRC}" >&2
  exit 1
fi

cp "${SRC}" icon.png
echo "saved icon.png"

if command -v magick >/dev/null 2>&1; then
  magick icon.png -define icon:auto-resize=256,128,64,48,32,24,16 I2PTorrents.ico
  echo "saved I2PTorrents.ico"
elif command -v convert >/dev/null 2>&1; then
  convert icon.png -define icon:auto-resize=256,128,64,48,32,24,16 I2PTorrents.ico
  echo "saved I2PTorrents.ico"
fi

if command -v sips >/dev/null 2>&1 && command -v iconutil >/dev/null 2>&1; then
  ICONSET="I2PTorrents.iconset"
  rm -rf "${ICONSET}"
  mkdir -p "${ICONSET}"
  for size in 16 32 128 256 512; do
    sips -z "${size}" "${size}" "${SRC}" --out "${ICONSET}/icon_${size}x${size}.png" >/dev/null
    sips -z "$((size * 2))" "$((size * 2))" "${SRC}" --out "${ICONSET}/icon_${size}x${size}@2x.png" >/dev/null
  done
  iconutil -c icns "${ICONSET}" -o I2PTorrents.icns
  rm -rf "${ICONSET}"
  echo "saved I2PTorrents.icns"
fi
