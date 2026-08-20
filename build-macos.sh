#!/usr/bin/env bash
set -euo pipefail

APP_NAME="I2PTorrents"
VENV_DIR=".venv"
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
  x86_64) ARCH_SUFFIX="x64" ;;
  arm64)  ARCH_SUFFIX="arm64" ;;
  *)      ARCH_SUFFIX="${ARCH}" ;;
esac

echo "==> Building ${APP_NAME} ${RELEASE_VERSION} for macOS ${ARCH_SUFFIX}"

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

echo "==> Wrapping dist/${APP_NAME}.app"
rm -rf "dist/${APP_NAME}.app"
mkdir -p "dist/${APP_NAME}.app/Contents/MacOS" "dist/${APP_NAME}.app/Contents/Resources"
cp -R "dist/${APP_NAME}" "dist/${APP_NAME}.app/Contents/Resources/${APP_NAME}"
if [ -f "I2PTorrents.icns" ]; then
  cp "I2PTorrents.icns" "dist/${APP_NAME}.app/Contents/Resources/I2PTorrents.icns"
else
  echo "WARNING: I2PTorrents.icns not found, using icon.png"
  cp "icon.png" "dist/${APP_NAME}.app/Contents/Resources/I2PTorrents.icns"
fi
printf '%s\n' '#!/bin/sh' "exec \"\$(dirname \"\$0\")/../Resources/${APP_NAME}/${APP_NAME}\" \"\$@\"" \
  > "dist/${APP_NAME}.app/Contents/MacOS/${APP_NAME}"
chmod +x "dist/${APP_NAME}.app/Contents/MacOS/${APP_NAME}"
chmod +x "dist/${APP_NAME}.app/Contents/Resources/${APP_NAME}/${APP_NAME}"

cat > "dist/${APP_NAME}.app/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDisplayName</key>
	<string>I2P Torrents</string>
	<key>CFBundleExecutable</key>
	<string>${APP_NAME}</string>
	<key>CFBundleIconFile</key>
	<string>I2PTorrents.icns</string>
	<key>CFBundleIdentifier</key>
	<string>org.metanoicarmor.i2ptorrents</string>
	<key>CFBundleName</key>
	<string>I2P Torrents</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>${RELEASE_VERSION}</string>
	<key>CFBundleVersion</key>
	<string>${RELEASE_VERSION}</string>
	<key>LSMinimumSystemVersion</key>
	<string>11.0</string>
	<key>NSHighResolutionCapable</key>
	<true/>
</dict>
</plist>
PLIST

ZIP_FILE="${APP_NAME}-macOS-${ARCH_SUFFIX}-v${RELEASE_VERSION}.zip"
rm -f "${ZIP_FILE}"
ditto -c -k --sequesterRsrc --keepParent "dist/${APP_NAME}.app" "${ZIP_FILE}"

echo
echo "✔ GUI: dist/${APP_NAME}.app"
echo "✔ Packed ${ZIP_FILE}"
