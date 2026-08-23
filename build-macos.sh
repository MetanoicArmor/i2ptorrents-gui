#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/package-common.sh
. "${ROOT}/scripts/package-common.sh"

[ "$(uname -s)" = "Darwin" ] || die "build-macos.sh must run on macOS"

# MACOS_ARCHS: auto (default) | arm64 | x64 | all
MACOS_ARCHS="${MACOS_ARCHS:-auto}"

export PATH="/opt/homebrew/opt/qt@6/bin:/opt/homebrew/bin:/usr/local/opt/qt@6/bin:/usr/local/bin:${PATH}"
setup_qt_path
read_version
make_icons
[ -f "${ROOT}/I2PTorrents.icns" ] || die "I2PTorrents.icns was not created (need sips + iconutil)"

qt_core_binary() {
  local prefix="$1"
  local core="${prefix}/lib/QtCore.framework/QtCore"
  [ -f "${core}" ] || return 1
  printf '%s' "${core}"
}

qt_cpu_arch() {
  local core
  core="$(qt_core_binary "$1")" || return 1
  case "$(file -b "${core}")" in
    *arm64*) printf 'arm64' ;;
    *x86_64*) printf 'x86_64' ;;
    *) return 1 ;;
  esac
}

find_qt_prefix_for_cpu() {
  local want_cpu="$1"
  local candidate core cpu
  for candidate in /opt/homebrew /usr/local; do
    core="$(qt_core_binary "${candidate}")" || continue
    cpu="$(qt_cpu_arch "${candidate}")" || continue
    if [ "${cpu}" = "${want_cpu}" ]; then
      printf '%s' "${candidate}"
      return 0
    fi
  done
  return 1
}

resolve_macos_builds() {
  HOST_CPU="$(uname -m)"
  case "${HOST_CPU}" in
    arm64|aarch64) HOST_CPU=arm64 ;;
    x86_64) HOST_CPU=x86_64 ;;
    *) die "unsupported macOS host CPU: ${HOST_CPU}" ;;
  esac

  BUILD_SPECS=()
  add_build() {
    local suffix="$1"
    local cpu="$2"
    local qt_prefix="$3"
    BUILD_SPECS+=("${suffix}|${cpu}|${qt_prefix}")
  }

  want_arm64=0
  want_x64=0
  case "${MACOS_ARCHS}" in
    auto)
      [ "${HOST_CPU}" = "arm64" ] && want_arm64=1
      [ "${HOST_CPU}" = "x86_64" ] && want_x64=1
      if [ "${HOST_CPU}" = "arm64" ]; then
        want_x64=1
      elif [ "${HOST_CPU}" = "x86_64" ]; then
        want_arm64=1
      fi
      ;;
    all)
      want_arm64=1
      want_x64=1
      ;;
    arm64|aarch64)
      want_arm64=1
      ;;
    x64|x86_64|intel)
      want_x64=1
      ;;
    *)
      die "unknown MACOS_ARCHS=${MACOS_ARCHS} (use auto, all, arm64, or x64)"
      ;;
  esac

  if [ "${want_arm64}" -eq 1 ]; then
    if qt_prefix="$(find_qt_prefix_for_cpu arm64)"; then
      add_build arm64 arm64 "${qt_prefix}"
    elif [ "${MACOS_ARCHS}" = "arm64" ] || [ "${MACOS_ARCHS}" = "all" ]; then
      die "Qt for arm64 not found (brew install qt@6 under /opt/homebrew)"
    else
      echo "==> Skipping arm64 build: Qt for arm64 not found"
    fi
  fi

  if [ "${want_x64}" -eq 1 ]; then
    if qt_prefix="$(find_qt_prefix_for_cpu x86_64)"; then
      add_build x64 x86_64 "${qt_prefix}"
    elif [ "${MACOS_ARCHS}" = "x64" ] || [ "${MACOS_ARCHS}" = "all" ] || [ "${MACOS_ARCHS}" = "intel" ]; then
      die "Qt for x86_64 not found. On Apple Silicon install Intel Homebrew Qt, e.g. arch -x86_64 /usr/local/bin/brew install qt@6"
    else
      echo "==> Skipping x64 build: Qt for x86_64 not found (/usr/local via Rosetta Homebrew)"
    fi
  fi

  [ "${#BUILD_SPECS[@]}" -gt 0 ] || die "no macOS builds selected (check MACOS_ARCHS and Qt installs)"
}

build_macos_release() {
  local arch_suffix="$1"
  local macos_arch="$2"
  local qt_prefix="$3"

  echo
  echo "==> Building ${APP_NAME} ${RELEASE_VERSION} for macOS ${arch_suffix} (Qt: ${qt_prefix})"

  export I2P_QT_PREFIX="${qt_prefix}"
  export I2P_MACOS_ARCH="${macos_arch}"

  local build_dir="${ROOT}/build-${arch_suffix}"
  rm -rf "${build_dir}"
  local osx_arch=""
  if [ "${macos_arch}" != "${HOST_CPU}" ]; then
    osx_arch="${macos_arch}"
  fi
  cmake_configure_release "${build_dir}" "${qt_prefix}" "${osx_arch}"
  cmake_build_release "${build_dir}"
  local bin
  bin="$(cmake_release_binary "${build_dir}")"
  [ -x "${bin}" ] || die "missing binary ${bin}"
  if command -v strip >/dev/null 2>&1; then
    strip -x "${bin}" || true
  fi

  local app_dir="${ROOT}/dist/${APP_NAME}-${arch_suffix}.app"
  local macos_dir="${app_dir}/Contents/MacOS"
  local res_dir="${app_dir}/Contents/Resources"
  echo "==> Wrapping ${app_dir}"
  rm -rf "${app_dir}"
  mkdir -p "${macos_dir}" "${res_dir}"
  cp "${bin}" "${macos_dir}/${APP_NAME}"
  chmod +x "${macos_dir}/${APP_NAME}"
  cp "${ROOT}/I2PTorrents.icns" "${res_dir}/I2PTorrents.icns"
  copy_runtime_files "${res_dir}"

  cat > "${app_dir}/Contents/Info.plist" <<PLIST
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
  touch "${app_dir}" "${app_dir}/Contents/Info.plist"

  local macdeployqt=""
  for candidate in \
    "${qt_prefix}/bin/macdeployqt" \
    "${qt_prefix}/opt/qt@6/bin/macdeployqt" \
    "$(command -v macdeployqt || true)"; do
    [ -n "${candidate}" ] || continue
    if [ -x "${candidate}" ]; then
      macdeployqt="${candidate}"
      break
    fi
  done
  [ -n "${macdeployqt}" ] || die "macdeployqt not found for ${qt_prefix} (brew install qt@6)"

  echo "==> Bundling Qt with macdeployqt"
  local qt_libs=""
  if [ -x "${qt_prefix}/bin/qmake6" ]; then
    qt_libs="$("${qt_prefix}/bin/qmake6" -query QT_INSTALL_LIBS 2>/dev/null || true)"
  elif [ -x "${qt_prefix}/bin/qmake" ]; then
    qt_libs="$("${qt_prefix}/bin/qmake" -query QT_INSTALL_LIBS 2>/dev/null || true)"
  fi
  # Do not ask macdeployqt to codesign: Homebrew dylibs (e.g. libbrotli) often
  # fail there. We ad-hoc sign the finished bundle ourselves below.
  local deploy_args=("${app_dir}" -always-overwrite)
  if [ -n "${qt_libs}" ] && [ -d "${qt_libs}" ]; then
    export DYLD_FRAMEWORK_PATH="${qt_libs}${DYLD_FRAMEWORK_PATH:+:${DYLD_FRAMEWORK_PATH}}"
    deploy_args+=(-libpath="${qt_libs}")
  fi
  local deploy_log
  deploy_log="$(mktemp "${TMPDIR:-/tmp}/macdeployqt.XXXXXX")"
  if ! "${macdeployqt}" "${deploy_args[@]}" >"${deploy_log}" 2>&1; then
    echo "WARNING: macdeployqt exited non-zero for ${arch_suffix}; log follows:" >&2
    cat "${deploy_log}" >&2
  elif grep -Eqi 'ERROR:|codesign' "${deploy_log}"; then
    echo "WARNING: macdeployqt reported codesign/deploy noise for ${arch_suffix} (continuing):" >&2
    grep -Ei 'ERROR:|codesign' "${deploy_log}" >&2 || true
  fi
  rm -f "${deploy_log}"
  [ -d "${app_dir}/Contents/Frameworks" ] \
    || die "macdeployqt did not create Frameworks in ${app_dir}"
  [ -x "${macos_dir}/${APP_NAME}" ] \
    || die "missing executable after macdeployqt: ${macos_dir}/${APP_NAME}"

  if command -v xattr >/dev/null 2>&1; then
    xattr -cr "${app_dir}" || echo "WARNING: xattr -cr failed for ${app_dir}" >&2
  fi
  if command -v codesign >/dev/null 2>&1; then
    echo "==> Ad-hoc codesign"
    if ! codesign --force --deep --sign - "${app_dir}"; then
      echo "WARNING: ad-hoc codesign failed for ${app_dir} (zip will still be packed)" >&2
    fi
  fi

  local zip_name="${APP_NAME}-macOS-${arch_suffix}-v${RELEASE_VERSION}.zip"
  local zip_file="${ROOT}/${zip_name}"
  echo "==> Packing ${zip_file}"
  rm -f "${zip_file}"
  # Pack from dist/ so ditto resolves a short relative path (avoids
  # "Cannot get the real path" on some macOS/Finder states).
  (
    cd "${ROOT}/dist"
    [ -d "${APP_NAME}-${arch_suffix}.app" ] \
      || die "app bundle missing before zip: ${ROOT}/dist/${APP_NAME}-${arch_suffix}.app"
    ditto -c -k --sequesterRsrc --keepParent \
      "${APP_NAME}-${arch_suffix}.app" \
      "${zip_file}"
  ) || die "ditto failed packing ${zip_file}"
  [ -f "${zip_file}" ] || die "zip was not created: ${zip_file}"
  [ -s "${zip_file}" ] || die "zip is empty: ${zip_file}"

  echo "✔ GUI: ${app_dir}"
  echo "✔ Packed ${zip_file} ($(du -h "${zip_file}" | awk '{print $1}'))"
}

resolve_macos_builds
mkdir -p "${ROOT}/dist"

PRIMARY_APP=""
PRIMARY_ZIP=""
BUILT_ZIPS=()
for spec in "${BUILD_SPECS[@]}"; do
  IFS='|' read -r arch_suffix macos_arch qt_prefix <<< "${spec}"
  build_macos_release "${arch_suffix}" "${macos_arch}" "${qt_prefix}"
  PRIMARY_APP="${ROOT}/dist/${APP_NAME}-${arch_suffix}.app"
  PRIMARY_ZIP="${ROOT}/${APP_NAME}-macOS-${arch_suffix}-v${RELEASE_VERSION}.zip"
  BUILT_ZIPS+=("${PRIMARY_ZIP}")
done

if [ "${#BUILD_SPECS[@]}" -eq 1 ]; then
  ln -sfn "$(basename "${PRIMARY_APP}")" "${ROOT}/dist/${APP_NAME}.app"
else
  rm -f "${ROOT}/dist/${APP_NAME}.app"
fi

echo
echo "Built ${#BUILD_SPECS[@]} macOS package(s):"
for zip_file in "${BUILT_ZIPS[@]}"; do
  if [ -f "${zip_file}" ]; then
    echo "  ✔ ${zip_file} ($(du -h "${zip_file}" | awk '{print $1}'))"
  else
    echo "  ✖ missing ${zip_file}" >&2
    die "expected zip missing after build: ${zip_file}"
  fi
done

echo
echo "Done."

