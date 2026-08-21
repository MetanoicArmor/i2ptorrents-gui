#!/bin/sh
# macOS Homebrew Qt is shipped as frameworks; qtrs/cc need C++17 + framework include flags.
set -e
export PATH="/opt/homebrew/opt/qt@6/bin:/opt/homebrew/bin:/usr/local/opt/qt@6/bin:/usr/local/bin:$PATH"
if command -v pkg-config >/dev/null 2>&1; then
  QT_CFLAGS="$(pkg-config --cflags Qt6Widgets Qt6Gui Qt6Core Qt6UiTools 2>/dev/null || true)"
fi
export CXXFLAGS="-std=c++17 -include arm_acle.h -Wno-error=implicit-function-declaration ${QT_CFLAGS}"
exec cargo "$@"
