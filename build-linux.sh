#!/usr/bin/env bash
set -euo pipefail

APP_NAME="I2PTorrents"
APPDIR="${APP_NAME}.AppDir"
VENV_DIR=".venv"
APPIMAGETOOL_VERSION="1.9.1"
cd "$(dirname "${BASH_SOURCE[0]}")"

VERSION_FILE="VERSION"
if [ ! -f "${VERSION_FILE}" ]; then
  echo "ERROR: VERSION file not found: ${VERSION_FILE}" >&2
  exit 1
fi
RELEASE_VERSION="$(tr -d '\r\n' < "${VERSION_FILE}")"
if [ -z "${RELEASE_VERSION}" ]; then
  echo "ERROR: VERSION file is empty: ${VERSION_FILE}" >&2
  exit 1
fi

ARCH="$(uname -m)"
case "${ARCH}" in
  x86_64)  ARCH_SUFFIX="x86_64" ;;
  aarch64) ARCH_SUFFIX="aarch64" ;;
  armv7l)  ARCH_SUFFIX="armhf" ;;
  *)       ARCH_SUFFIX="${ARCH}" ;;
esac

echo "==> Building ${APP_NAME} ${RELEASE_VERSION} for Linux ${ARCH_SUFFIX}"

if [ -n "${I2PTORRENTS_PYTHON:-}" ]; then
  PYTHON_BIN="${I2PTORRENTS_PYTHON}"
elif command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN="python3"
else
  echo "ERROR: python3 not found" >&2
  exit 1
fi

if [ ! -x "${VENV_DIR}/bin/python" ]; then
  echo "==> Creating ${VENV_DIR}"
  "${PYTHON_BIN}" -m venv "${VENV_DIR}"
fi
PYTHON_CMD="${VENV_DIR}/bin/python"

echo "==> Installing build dependencies"
"${PYTHON_CMD}" -m pip install -U pip
"${PYTHON_CMD}" -m pip install -e . pyinstaller pillow

echo "==> Generating icons from image.png"
"${PYTHON_CMD}" make_icon.py

echo "==> Building onedir (PyInstaller)"
rm -rf "dist/${APP_NAME}" "build/${APP_NAME}"
"${PYTHON_CMD}" -m PyInstaller --clean -y I2PTorrents.spec

chmod +x "dist/${APP_NAME}/${APP_NAME}"

echo "==> Packing portable zip"
ZIP_FILE="${APP_NAME}-linux-${ARCH_SUFFIX}-v${RELEASE_VERSION}.zip"
rm -f "${ZIP_FILE}"
"${PYTHON_CMD}" - "dist/${APP_NAME}" "${ZIP_FILE}" <<'PY'
import os
import sys
import zipfile

src, dst = sys.argv[1], sys.argv[2]
prefix = os.path.basename(src)
with zipfile.ZipFile(dst, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    for root, _dirs, files in os.walk(src):
        for name in files:
            path = os.path.join(root, name)
            arcname = os.path.join(prefix, os.path.relpath(path, src))
            zf.write(path, arcname=arcname)
PY
echo "✔ Packed ${ZIP_FILE}"

echo "==> Preparing AppDir"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin" \
         "${APPDIR}/usr/share/applications" \
         "${APPDIR}/usr/share/icons/hicolor/512x512/apps"
cp "dist/${APP_NAME}/${APP_NAME}" "${APPDIR}/usr/bin/${APP_NAME}"
cp -r "dist/${APP_NAME}/_internal" "${APPDIR}/usr/bin/_internal"
cp icon.png "${APPDIR}/usr/share/icons/hicolor/512x512/apps/i2ptorrents.png"

cat > "${APPDIR}/usr/share/applications/i2ptorrents.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=I2P Torrents
Comment=Desktop GUI for i2pd torrents
Exec=${APP_NAME}
Icon=i2ptorrents
Terminal=false
Categories=Network;FileTransfer;
StartupWMClass=I2PTorrents
EOF

cp "${APPDIR}/usr/share/applications/i2ptorrents.desktop" "${APPDIR}/i2ptorrents.desktop"
cp icon.png "${APPDIR}/i2ptorrents.png"

cat > "${APPDIR}/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/bin/_internal:${LD_LIBRARY_PATH:-}"
exec "$HERE/usr/bin/I2PTorrents" "$@"
EOF
chmod +x "${APPDIR}/AppRun" "${APPDIR}/usr/bin/${APP_NAME}"

APPIMAGETOOL="appimagetool-${ARCH}.AppImage"
case "${ARCH}" in
  x86_64)  APPIMAGETOOL_SHA256="ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0" ;;
  aarch64) APPIMAGETOOL_SHA256="f0837e7448a0c1e4e650a93bb3e85802546e60654ef287576f46c71c126a9158" ;;
  armv7l)  APPIMAGETOOL_SHA256="42b61cba5495d8aaf418a5c9a015a49b85ad92efabcbd3c341f1540440e4e23d" ;;
  *)
    echo "WARNING: no pinned appimagetool for ${ARCH}; skipping AppImage" >&2
    echo "✔ GUI onedir: dist/${APP_NAME}/"
    exit 0
    ;;
esac

download() {
  local url="$1"
  local dest="$2"
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL -o "${dest}" "${url}"
  elif command -v wget >/dev/null 2>&1; then
    wget -q -O "${dest}" "${url}"
  else
    return 1
  fi
}

file_sha256() {
  "${PYTHON_CMD}" - "$1" <<'PY'
import hashlib
import sys
path = sys.argv[1]
digest = hashlib.sha256()
with open(path, "rb") as handle:
    for chunk in iter(lambda: handle.read(1024 * 1024), b""):
        digest.update(chunk)
print(digest.hexdigest())
PY
}

if [ ! -f "${APPIMAGETOOL}" ]; then
  echo "==> Downloading appimagetool ${APPIMAGETOOL_VERSION}"
  if ! download \
    "https://github.com/AppImage/appimagetool/releases/download/${APPIMAGETOOL_VERSION}/${APPIMAGETOOL}" \
    "${APPIMAGETOOL}"; then
    echo "WARNING: could not download appimagetool; skipping AppImage" >&2
    echo "✔ GUI onedir: dist/${APP_NAME}/"
    exit 0
  fi
fi

ACTUAL_SHA256="$(file_sha256 "${APPIMAGETOOL}")"
if [ "${ACTUAL_SHA256}" != "${APPIMAGETOOL_SHA256}" ]; then
  echo "WARNING: SHA256 mismatch for ${APPIMAGETOOL}, re-downloading" >&2
  rm -f "${APPIMAGETOOL}"
  if ! download \
    "https://github.com/AppImage/appimagetool/releases/download/${APPIMAGETOOL_VERSION}/${APPIMAGETOOL}" \
    "${APPIMAGETOOL}"; then
    echo "WARNING: could not download appimagetool; skipping AppImage" >&2
    echo "✔ GUI onedir: dist/${APP_NAME}/"
    exit 0
  fi
  ACTUAL_SHA256="$(file_sha256 "${APPIMAGETOOL}")"
  if [ "${ACTUAL_SHA256}" != "${APPIMAGETOOL_SHA256}" ]; then
    echo "ERROR: SHA256 mismatch for downloaded ${APPIMAGETOOL}" >&2
    echo "Expected: ${APPIMAGETOOL_SHA256}" >&2
    echo "Actual:   ${ACTUAL_SHA256}" >&2
    exit 1
  fi
fi
chmod +x "${APPIMAGETOOL}"

mkdir -p dist
OUTPUT_FILE="dist/${APP_NAME}-linux-${ARCH_SUFFIX}-v${RELEASE_VERSION}.AppImage"
export APPIMAGE_EXTRACT_AND_RUN=1
"./${APPIMAGETOOL}" "${APPDIR}" "${OUTPUT_FILE}"
echo "✔ Built ${OUTPUT_FILE}"
echo "✔ GUI onedir: dist/${APP_NAME}/"
