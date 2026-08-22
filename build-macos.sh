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

ensure_rust_target() {
  local target="$1"
  if ! rustup target list --installed | grep -qx "${target}"; then
    echo "==> Installing Rust target ${target}"
    rustup target add "${target}"
  fi
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
    local target="$3"
    local qt_prefix="$4"
    BUILD_SPECS+=("${suffix}|${cpu}|${target}|${qt_prefix}")
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
      if [ "${HOST_CPU}" = "arm64" ]; then
        add_build arm64 arm64 "" "${qt_prefix}"
      else
        ensure_rust_target aarch64-apple-darwin
        add_build arm64 arm64 aarch64-apple-darwin "${qt_prefix}"
      fi
    elif [ "${MACOS_ARCHS}" = "arm64" ] || [ "${MACOS_ARCHS}" = "all" ]; then
      die "Qt for arm64 not found (brew install qt@6 under /opt/homebrew)"
    else
      echo "==> Skipping arm64 build: Qt for arm64 not found"
    fi
  fi

  if [ "${want_x64}" -eq 1 ]; then
    if qt_prefix="$(find_qt_prefix_for_cpu x86_64)"; then
      if [ "${HOST_CPU}" = "x86_64" ]; then
        add_build x64 x86_64 "" "${qt_prefix}"
      else
        ensure_rust_target x86_64-apple-darwin
        add_build x64 x86_64 x86_64-apple-darwin "${qt_prefix}"
      fi
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
  local rust_target="$3"
  local qt_prefix="$4"

  echo
  echo "==> Building ${APP_NAME} ${RELEASE_VERSION} for macOS ${arch_suffix} (Qt: ${qt_prefix})"

  export I2P_QT_PREFIX="${qt_prefix}"
  export I2P_MACOS_ARCH="${macos_arch}"

  local cargo_args=(build --release --features gui)
  if [ -n "${rust_target}" ]; then
    cargo_args+=(--target "${rust_target}")
  fi
  "${ROOT}/scripts/cargo-qt.sh" "${cargo_args[@]}"

  local bin
  if [ -n "${rust_target}" ]; then
    bin="${ROOT}/target/${rust_target}/release/${CARGO_BIN}"
  else
    bin="${ROOT}/target/release/${CARGO_BIN}"
  fi
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
  local deploy_args=("${app_dir}" -always-overwrite)
  if [ -n "${qt_libs}" ] && [ -d "${qt_libs}" ]; then
    export DYLD_FRAMEWORK_PATH="${qt_libs}${DYLD_FRAMEWORK_PATH:+:${DYLD_FRAMEWORK_PATH}}"
    deploy_args+=(-libpath="${qt_libs}")
    ln -sfn "${qt_libs}" "${ROOT}/dist/lib"
  fi
  "${macdeployqt}" "${deploy_args[@]}"
  rm -f "${ROOT}/dist/lib"

  if command -v xattr >/dev/null 2>&1; then
    xattr -cr "${app_dir}"
  fi
  if command -v codesign >/dev/null 2>&1; then
    echo "==> Ad-hoc codesign"
    codesign --force --deep --sign - "${app_dir}"
  fi

  local zip_file="${ROOT}/${APP_NAME}-macOS-${arch_suffix}-v${RELEASE_VERSION}.zip"
  rm -f "${zip_file}"
  ditto -c -k --sequesterRsrc --keepParent "${app_dir}" "${zip_file}"

  echo "✔ GUI: ${app_dir}"
  echo "✔ Packed ${zip_file}"
}

resolve_macos_builds

PRIMARY_APP=""
PRIMARY_ZIP=""
for spec in "${BUILD_SPECS[@]}"; do
  IFS='|' read -r arch_suffix macos_arch rust_target qt_prefix <<< "${spec}"
  build_macos_release "${arch_suffix}" "${macos_arch}" "${rust_target}" "${qt_prefix}"
  PRIMARY_APP="${ROOT}/dist/${APP_NAME}-${arch_suffix}.app"
  PRIMARY_ZIP="${ROOT}/${APP_NAME}-macOS-${arch_suffix}-v${RELEASE_VERSION}.zip"
done

if [ "${#BUILD_SPECS[@]}" -eq 1 ]; then
  ln -sfn "$(basename "${PRIMARY_APP}")" "${ROOT}/dist/${APP_NAME}.app"
else
  rm -f "${ROOT}/dist/${APP_NAME}.app"
  echo
  echo "Built ${#BUILD_SPECS[@]} macOS packages."
fi

echo
echo "Done."
