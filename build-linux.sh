#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/package-common.sh
. "${ROOT}/scripts/package-common.sh"

collect_deps() {
  local file="$1"
  local libdir="$2"
  [ -f "${file}" ] || return 0
  command -v ldd >/dev/null 2>&1 || die "ldd not found"
  local lib base dest_lib
  while read -r lib; do
    [ -e "${lib}" ] || continue
    base="$(basename "${lib}")"
    case "${base}" in
      ld-linux*|linux-vdso*|libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|libresolv.so*|libmvec.so*|libutil.so*|libanl.so*)
        continue
        ;;
      libGL.so*|libGLdispatch.so*|libGLX.so*|libEGL.so*|libOpenGL.so*|libdrm.so*|libnvidia*|libcuda*|libva.so*|libgbm.so*)
        continue
        ;;
    esac
    dest_lib="${libdir}/${base}"
    if [ ! -e "${dest_lib}" ]; then
      cp -L "${lib}" "${dest_lib}"
      collect_deps "${dest_lib}" "${libdir}"
    fi
  done < <(ldd "${file}" | awk '/=> \// {print $3} /^\t\// {print $1}')
}

bundle_linux_qt() {
  local dest="$1"
  local binary="$2"
  local plugins_src=""
  if command -v qmake6 >/dev/null 2>&1; then
    plugins_src="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
  elif command -v qmake >/dev/null 2>&1; then
    plugins_src="$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
  fi
  [ -n "${plugins_src}" ] && [ -d "${plugins_src}" ] || \
    die "Qt plugin directory not found (install qt6 and qmake6)"

  echo "==> Bundling Qt libraries and plugins"
  mkdir -p "${dest}/lib" "${dest}/plugins"
  collect_deps "${binary}" "${dest}/lib"
  local group plugin
  for group in platforms imageformats styles iconengines platformthemes \
               platforminputcontexts xcbglintegrations wayland-shell-integration \
               wayland-graphics-integration-client; do
    if [ -d "${plugins_src}/${group}" ]; then
      mkdir -p "${dest}/plugins/${group}"
      while IFS= read -r -d '' plugin; do
        cp -L "${plugin}" "${dest}/plugins/${group}/"
        collect_deps "${plugin}" "${dest}/lib"
      done < <(find "${plugins_src}/${group}" -maxdepth 1 -type f -name '*.so*' -print0)
    fi
  done
}

write_linux_launcher() {
  local dir="$1"
  local bin_name="$2"
  local prefix="$3"
  cat > "${dir}/${APP_NAME}" <<EOF
#!/bin/sh
HERE="\$(CDPATH= cd -- "\$(dirname "\$0")" && pwd)"
export LD_LIBRARY_PATH="\$HERE/${prefix}/lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="\$HERE/${prefix}/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="\$HERE/${prefix}/plugins/platforms"
exec "\$HERE/${bin_name}" "\$@"
EOF
  chmod +x "${dir}/${APP_NAME}"
}

write_qt_conf() {
  local dir="$1"
  local prefix="$2"
  local libs="$3"
  local plugins="$4"
  cat > "${dir}/qt.conf" <<EOF
[Paths]
Prefix=${prefix}
Libraries=${libs}
Plugins=${plugins}
EOF
}

download_file() {
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
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | awk '{print $1}'
  else
    openssl dgst -sha256 "$1" | awk '{print $NF}'
  fi
}

pack_appimage() {
  local appdir="$1"
  local linux_arch="$2"
  local host_arch="$3"
  local APPIMAGETOOL_VERSION="1.9.1"
  local tool="appimagetool-${host_arch}.AppImage"
  local sha actual output
  case "${host_arch}" in
    x86_64)  sha="ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0" ;;
    aarch64) sha="f0837e7448a0c1e4e650a93bb3e85802546e60654ef287576f46c71c126a9158" ;;
    armv7l)  sha="42b61cba5495d8aaf418a5c9a015a49b85ad92efabcbd3c341f1540440e4e23d" ;;
    *)
      echo "WARNING: no pinned appimagetool for ${host_arch}; skipping AppImage" >&2
      return 0
      ;;
  esac

  if [ ! -f "${ROOT}/${tool}" ]; then
    echo "==> Downloading appimagetool ${APPIMAGETOOL_VERSION}"
    if ! download_file \
      "https://github.com/AppImage/appimagetool/releases/download/${APPIMAGETOOL_VERSION}/${tool}" \
      "${ROOT}/${tool}"; then
      echo "WARNING: could not download appimagetool; skipping AppImage" >&2
      return 0
    fi
  fi
  actual="$(file_sha256 "${ROOT}/${tool}")"
  if [ "${actual}" != "${sha}" ]; then
    echo "WARNING: SHA256 mismatch for ${tool}, re-downloading" >&2
    rm -f "${ROOT}/${tool}"
    if ! download_file \
      "https://github.com/AppImage/appimagetool/releases/download/${APPIMAGETOOL_VERSION}/${tool}" \
      "${ROOT}/${tool}"; then
      echo "WARNING: could not download appimagetool; skipping AppImage" >&2
      return 0
    fi
    actual="$(file_sha256 "${ROOT}/${tool}")"
    if [ "${actual}" != "${sha}" ]; then
      die "SHA256 mismatch for ${tool} (expected ${sha}, got ${actual})"
    fi
  fi
  chmod +x "${ROOT}/${tool}"
  mkdir -p "${ROOT}/dist"
  output="${ROOT}/dist/${APP_NAME}-linux-${linux_arch}-v${RELEASE_VERSION}.AppImage"
  export APPIMAGE_EXTRACT_AND_RUN=1
  "${ROOT}/${tool}" "${appdir}" "${output}"
  echo "✔ Built ${output}"
}

[ "$(uname -s)" = "Linux" ] || die "build-linux.sh must run on Linux"

setup_qt_path
read_version
detect_arch
case "${HOST_ARCH}" in
  x86_64)  LINUX_ARCH="x86_64" ;;
  aarch64) LINUX_ARCH="aarch64" ;;
  armv7l)  LINUX_ARCH="armhf" ;;
  *)       LINUX_ARCH="${HOST_ARCH}" ;;
esac

echo "==> Building ${APP_NAME} ${RELEASE_VERSION} for Linux ${LINUX_ARCH}"
make_icons

echo "==> Building release binary"
require_cmd cargo
cargo build --release --features gui
BIN="${ROOT}/target/release/${CARGO_BIN}"
[ -x "${BIN}" ] || die "missing binary ${BIN}"

STAGE="${ROOT}/dist/${APP_NAME}"
echo "==> Staging ${STAGE}"
rm -rf "${STAGE}"
mkdir -p "${STAGE}/lib" "${STAGE}/plugins"
cp "${BIN}" "${STAGE}/${APP_NAME}.bin"
chmod +x "${STAGE}/${APP_NAME}.bin"
if command -v strip >/dev/null 2>&1; then
  strip "${STAGE}/${APP_NAME}.bin" || true
fi
copy_runtime_files "${STAGE}"
bundle_linux_qt "${STAGE}" "${STAGE}/${APP_NAME}.bin"
write_linux_launcher "${STAGE}" "${APP_NAME}.bin" "."
write_qt_conf "${STAGE}" "." "lib" "plugins"

ZIP_FILE="${ROOT}/${APP_NAME}-linux-${LINUX_ARCH}-v${RELEASE_VERSION}.zip"
echo "==> Packing ${ZIP_FILE}"
rm -f "${ZIP_FILE}"
if command -v zip >/dev/null 2>&1; then
  (cd "${ROOT}/dist" && zip -r "${ZIP_FILE}" "${APP_NAME}")
else
  tar -C "${ROOT}/dist" -czf "${ZIP_FILE%.zip}.tar.gz" "${APP_NAME}"
  ZIP_FILE="${ZIP_FILE%.zip}.tar.gz"
fi
echo "✔ Packed ${ZIP_FILE}"

APPDIR="${ROOT}/${APP_NAME}.AppDir"
echo "==> Preparing AppDir"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin" \
         "${APPDIR}/usr/lib" \
         "${APPDIR}/usr/plugins" \
         "${APPDIR}/usr/share/applications" \
         "${APPDIR}/usr/share/icons/hicolor/512x512/apps"
cp "${STAGE}/${APP_NAME}.bin" "${APPDIR}/usr/bin/${APP_NAME}.bin"
chmod +x "${APPDIR}/usr/bin/${APP_NAME}.bin"
cp -a "${STAGE}/lib/." "${APPDIR}/usr/lib/"
cp -a "${STAGE}/plugins/." "${APPDIR}/usr/plugins/"
copy_runtime_files "${APPDIR}/usr/bin"
write_linux_launcher "${APPDIR}/usr/bin" "${APP_NAME}.bin" ".."
write_qt_conf "${APPDIR}/usr/bin" ".." "lib" "plugins"
ICON_SRC="${ROOT}/icon.png"
[ -f "${ICON_SRC}" ] || ICON_SRC="${ROOT}/image.png"
cp "${ICON_SRC}" "${APPDIR}/usr/share/icons/hicolor/512x512/apps/i2ptorrents.png"
cp "${ICON_SRC}" "${APPDIR}/i2ptorrents.png"

cat > "${APPDIR}/usr/share/applications/i2ptorrents.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=${APP_DISPLAY_NAME}
Comment=Desktop GUI for i2pd torrents
Exec=${APP_NAME}
Icon=i2ptorrents
Terminal=false
Categories=Network;FileTransfer;
StartupWMClass=${APP_NAME}
EOF
cp "${APPDIR}/usr/share/applications/i2ptorrents.desktop" "${APPDIR}/i2ptorrents.desktop"

cat > "${APPDIR}/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/usr/bin/I2PTorrents" "$@"
EOF
chmod +x "${APPDIR}/AppRun" "${APPDIR}/usr/bin/${APP_NAME}"

pack_appimage "${APPDIR}" "${LINUX_ARCH}" "${HOST_ARCH}"
echo "✔ GUI onedir: ${STAGE}/"
