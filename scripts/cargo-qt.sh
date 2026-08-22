#!/bin/sh
# macOS Homebrew Qt is shipped as frameworks; qtrs/cc need C++17 + framework include flags.
set -e
export PATH="/opt/homebrew/opt/qt@6/bin:/opt/homebrew/bin:/usr/local/opt/qt@6/bin:/usr/local/bin:$PATH"
if [ -n "${I2P_QT_PREFIX:-}" ] && [ -d "${I2P_QT_PREFIX}/bin" ]; then
  export PATH="${I2P_QT_PREFIX}/bin:${PATH}"
fi
if [ -n "${I2P_QT_PREFIX:-}" ] && [ -d "${I2P_QT_PREFIX}/lib/pkgconfig" ]; then
  export PKG_CONFIG_PATH="${I2P_QT_PREFIX}/lib/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"
fi
if command -v pkg-config >/dev/null 2>&1; then
  QT_CFLAGS="$(pkg-config --cflags Qt6Widgets Qt6Gui Qt6Core Qt6UiTools 2>/dev/null || true)"
fi
ARCH_CFLAGS=""
case "${I2P_MACOS_ARCH:-$(uname -m)}" in
  arm64|aarch64) ARCH_CFLAGS="-include arm_acle.h" ;;
esac
export CXXFLAGS="-std=c++17 ${ARCH_CFLAGS} -Wno-error=implicit-function-declaration ${QT_CFLAGS}"
exec cargo "$@"
