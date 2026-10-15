#!/usr/bin/env bash
# Print SHA256 for GitHub release assets + icon.png for packaging bumps.
set -euo pipefail

REPO="MetanoicArmor/i2ptorrents-gui"
TAG="${1:-}"

hash_cmd() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum
  else
    shasum -a 256
  fi
}

if [ -z "${TAG}" ]; then
  TAG="$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" \
    | python3 -c "import sys,json; print(json.load(sys.stdin).get('tag_name',''))")"
fi

TAG="${TAG#v}"
if [ -z "${TAG}" ]; then
  echo "ERROR: could not resolve release tag" >&2
  exit 1
fi

BASE="https://github.com/${REPO}/releases/download/v${TAG}"
FILES="
I2PTorrents-macOS-arm64-v${TAG}.zip
I2PTorrents-macOS-x64-v${TAG}.zip
I2PTorrents-windows-x64-v${TAG}.zip
I2PTorrents-windows-arm64-v${TAG}.zip
I2PTorrents-linux-x86_64-v${TAG}.zip
I2PTorrents-linux-aarch64-v${TAG}.zip
I2PTorrents-linux-x86_64-v${TAG}.AppImage
I2PTorrents-linux-aarch64-v${TAG}.AppImage
I2PTorrents-${TAG}-linux-x86_64.deb
I2PTorrents-${TAG}-linux-aarch64.deb
I2PTorrents-${TAG}-linux-x86_64.rpm
I2PTorrents-${TAG}-linux-aarch64.rpm
"

echo "# Release v${TAG}"
echo "# --- SHA256 ---"
MAC_ARM="REPLACE_AFTER_RELEASE"
MAC_X64="REPLACE_AFTER_RELEASE"
WIN_X64="REPLACE_AFTER_RELEASE"
WIN_ARM="REPLACE_AFTER_RELEASE"
APP_X64="REPLACE_AFTER_RELEASE"
APP_ARM="REPLACE_AFTER_RELEASE"

for f in ${FILES}; do
  url="${BASE}/${f}"
  if ! sum="$(curl -fsSL "${url}" | hash_cmd | awk '{print $1}')"; then
    echo "SKIP (missing): ${f}" >&2
    continue
  fi
  printf '%s  %s\n' "${sum}" "${f}"
  case "${f}" in
    I2PTorrents-macOS-arm64-*) MAC_ARM="${sum}" ;;
    I2PTorrents-macOS-x64-*) MAC_X64="${sum}" ;;
    I2PTorrents-windows-x64-*) WIN_X64="${sum}" ;;
    I2PTorrents-windows-arm64-*) WIN_ARM="${sum}" ;;
    I2PTorrents-linux-x86_64-*.AppImage) APP_X64="${sum}" ;;
    I2PTorrents-linux-aarch64-*.AppImage) APP_ARM="${sum}" ;;
  esac
done

icon_sum="MISSING"
for icon_url in \
  "https://github.com/${REPO}/releases/download/v${TAG}/icon.png" \
  "https://github.com/${REPO}/raw/v${TAG}/icon.png" \
  "https://raw.githubusercontent.com/${REPO}/master/icon.png"
do
  if icon_sum="$(curl -fsSL "${icon_url}" | hash_cmd | awk '{print $1}')"; then
    printf '%s  icon.png (%s)\n' "${icon_sum}" "${icon_url}"
    break
  fi
  icon_sum="MISSING"
done

echo
echo "# --- Homebrew cask ---"
echo "  version \"${TAG}\""
echo "  # arm64 sha256 \"${MAC_ARM}\""
if [ "${MAC_X64}" != "REPLACE_AFTER_RELEASE" ]; then
  echo "  # x64   sha256 \"${MAC_X64}\""
fi
echo "# --- winget InstallerSha256 ---"
echo "  # x64   ${WIN_X64}"
echo "  # arm64 ${WIN_ARM}"
echo "# --- AUR sha256sums ---"
echo "  # x86_64 AppImage ${APP_X64}"
echo "  # aarch64 AppImage ${APP_ARM}"
echo "  # icon.png ${icon_sum}"
echo "# --- Flatpak AppImage + icon ---"
echo "  # AppImage ${APP_X64}"
echo "  # icon ${icon_sum}"
