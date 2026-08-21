#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/package-common.sh
. "${ROOT}/scripts/package-common.sh"

[ "$(uname -s)" = "Darwin" ] || die "build-macos.sh must run on macOS"

export PATH="/opt/homebrew/opt/qt@6/bin:/opt/homebrew/bin:/usr/local/opt/qt@6/bin:/usr/local/bin:${PATH}"
setup_qt_path
read_version
detect_arch

echo "==> Building ${APP_NAME} ${RELEASE_VERSION} for macOS ${ARCH_SUFFIX}"
make_icons
[ -f "${ROOT}/I2PTorrents.icns" ] || die "I2PTorrents.icns was not created (need sips + iconutil)"

echo "==> Building release binary"
"${ROOT}/scripts/cargo-qt.sh" build --release --features gui
BIN="${ROOT}/target/release/${CARGO_BIN}"
[ -x "${BIN}" ] || die "missing binary ${BIN}"
if command -v strip >/dev/null 2>&1; then
  strip -x "${BIN}" || true
fi

APP_DIR="${ROOT}/dist/${APP_NAME}.app"
MACOS_DIR="${APP_DIR}/Contents/MacOS"
RES_DIR="${APP_DIR}/Contents/Resources"
echo "==> Wrapping ${APP_DIR}"
rm -rf "${APP_DIR}"
mkdir -p "${MACOS_DIR}" "${RES_DIR}"
cp "${BIN}" "${MACOS_DIR}/${APP_NAME}"
chmod +x "${MACOS_DIR}/${APP_NAME}"
cp "${ROOT}/I2PTorrents.icns" "${RES_DIR}/I2PTorrents.icns"
copy_runtime_files "${RES_DIR}"

cat > "${APP_DIR}/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>en</string>
	<key>CFBundleDisplayName</key>
	<string>${APP_DISPLAY_NAME}</string>
	<key>CFBundleExecutable</key>
	<string>${APP_NAME}</string>
	<key>CFBundleIconFile</key>
	<string>I2PTorrents.icns</string>
	<key>CFBundleIdentifier</key>
	<string>${APP_ID}</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>${APP_DISPLAY_NAME}</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>${RELEASE_VERSION}</string>
	<key>CFBundleVersion</key>
	<string>${RELEASE_VERSION}</string>
	<key>LSApplicationCategoryType</key>
	<string>public.app-category.utilities</string>
	<key>LSMinimumSystemVersion</key>
	<string>11.0</string>
	<key>NSHighResolutionCapable</key>
	<true/>
	<key>NSSupportsAutomaticGraphicsSwitching</key>
	<true/>
</dict>
</plist>
PLIST
touch "${APP_DIR}" "${APP_DIR}/Contents/Info.plist"

MACDEPLOYQT="$(command -v macdeployqt || true)"
if [ -z "${MACDEPLOYQT}" ]; then
  for candidate in /opt/homebrew/opt/qt@6/bin/macdeployqt /usr/local/opt/qt@6/bin/macdeployqt; do
    if [ -x "${candidate}" ]; then
      MACDEPLOYQT="${candidate}"
      break
    fi
  done
fi
[ -n "${MACDEPLOYQT}" ] || die "macdeployqt not found (brew install qt@6)"

echo "==> Bundling Qt with macdeployqt"
QT_LIBS=""
if command -v qmake6 >/dev/null 2>&1; then
  QT_LIBS="$(qmake6 -query QT_INSTALL_LIBS 2>/dev/null || true)"
elif command -v qmake >/dev/null 2>&1; then
  QT_LIBS="$(qmake -query QT_INSTALL_LIBS 2>/dev/null || true)"
fi
DEPLOY_ARGS=("${APP_DIR}" -always-overwrite)
if [ -n "${QT_LIBS}" ] && [ -d "${QT_LIBS}" ]; then
  export DYLD_FRAMEWORK_PATH="${QT_LIBS}${DYLD_FRAMEWORK_PATH:+:${DYLD_FRAMEWORK_PATH}}"
  DEPLOY_ARGS+=(-libpath="${QT_LIBS}")
  # Homebrew macdeployqt resolves plugin @rpath against <appdir>/../lib
  ln -sfn "${QT_LIBS}" "${ROOT}/dist/lib"
fi
"${MACDEPLOYQT}" "${DEPLOY_ARGS[@]}"
rm -f "${ROOT}/dist/lib"

if command -v xattr >/dev/null 2>&1; then
  xattr -cr "${APP_DIR}"
fi
if command -v codesign >/dev/null 2>&1; then
  echo "==> Ad-hoc codesign"
  codesign --force --deep --sign - "${APP_DIR}"
fi

ZIP_FILE="${ROOT}/${APP_NAME}-macOS-${ARCH_SUFFIX}-v${RELEASE_VERSION}.zip"
rm -f "${ZIP_FILE}"
ditto -c -k --sequesterRsrc --keepParent "${APP_DIR}" "${ZIP_FILE}"

echo
echo "✔ GUI: ${APP_DIR}"
echo "✔ Packed ${ZIP_FILE}"
