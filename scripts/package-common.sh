#!/bin/sh
# Shared helpers for build-macos.sh / build-linux.sh. Source, do not execute.

APP_NAME="I2PTorrents"
APP_DISPLAY_NAME="I2P Torrents"
APP_ID="org.metanoicarmor.i2ptorrents"
CARGO_BIN="i2ptorrents-gui"

die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing command: $1"
}

read_version() {
  VERSION_FILE="${ROOT}/VERSION"
  [ -f "${VERSION_FILE}" ] || die "VERSION file not found: ${VERSION_FILE}"
  RELEASE_VERSION="$(tr -d '\r\n' < "${VERSION_FILE}")"
  [ -n "${RELEASE_VERSION}" ] || die "VERSION file is empty: ${VERSION_FILE}"
}

detect_arch() {
  HOST_ARCH="$(uname -m)"
  case "${HOST_ARCH}" in
    x86_64|amd64) ARCH_SUFFIX="x64" ;;
    arm64|aarch64) ARCH_SUFFIX="arm64" ;;
    armv7l) ARCH_SUFFIX="armhf" ;;
    *) ARCH_SUFFIX="${HOST_ARCH}" ;;
  esac
}

setup_qt_path() {
  QT_PREFIX=""
  if command -v qmake6 >/dev/null 2>&1; then
    QT_PREFIX="$(qmake6 -query QT_INSTALL_PREFIX 2>/dev/null || true)"
  elif command -v qmake >/dev/null 2>&1; then
    QT_PREFIX="$(qmake -query QT_INSTALL_PREFIX 2>/dev/null || true)"
  fi
  for candidate in \
    /opt/homebrew/opt/qt@6 \
    /usr/local/opt/qt@6 \
    "${QT_PREFIX}"
  do
    [ -n "${candidate}" ] || continue
    if [ -d "${candidate}/bin" ]; then
      export PATH="${candidate}/bin:${PATH}"
      break
    fi
  done
}

copy_runtime_files() {
  dest="$1"
  mkdir -p "${dest}"
  cp "${ROOT}/image.png" "${dest}/image.png"
  if [ -f "${ROOT}/icon.png" ]; then
    cp "${ROOT}/icon.png" "${dest}/icon.png"
  else
    cp "${ROOT}/image.png" "${dest}/icon.png"
  fi
  cp "${ROOT}/VERSION" "${ROOT}/AUTHORS" "${ROOT}/LICENSE" "${dest}/"
  if [ -f "${ROOT}/assets/ton_donation_qr.png" ]; then
    cp "${ROOT}/assets/ton_donation_qr.png" "${dest}/ton_donation_qr.png"
  elif [ -f "${ROOT}/ton_donation_qr.png" ]; then
    cp "${ROOT}/ton_donation_qr.png" "${dest}/ton_donation_qr.png"
  fi
  if [ -d "${ROOT}/assets/fonts" ]; then
    mkdir -p "${dest}/fonts"
    for font in "${ROOT}/assets/fonts"/Inter-*.otf "${ROOT}/assets/fonts"/Inter-*.ttf; do
      [ -f "${font}" ] || continue
      cp "${font}" "${dest}/fonts/"
    done
    if [ -f "${ROOT}/assets/fonts/Inter-OFL.txt" ]; then
      cp "${ROOT}/assets/fonts/Inter-OFL.txt" "${dest}/fonts/"
    fi
  fi
}

make_icons() {
  echo "==> Generating icons from image.png"
  "${ROOT}/scripts/make-icon.sh"
}
