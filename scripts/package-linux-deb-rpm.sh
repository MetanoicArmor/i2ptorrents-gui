#!/usr/bin/env bash
# Build .deb and .rpm from the staged Linux onedir (dist/I2PTorrents).
# Requires: dpkg-deb; for .rpm also rpmbuild (Debian/Ubuntu: apt install rpm).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/package-common.sh
. "${ROOT}/scripts/package-common.sh"

STAGE="${1:-}"
LINUX_ARCH="${2:-}"
[ -n "${STAGE}" ] && [ -d "${STAGE}" ] || die "usage: $0 <stage-dir> <linux-arch>"
[ -n "${LINUX_ARCH}" ] || die "usage: $0 <stage-dir> <linux-arch>"

read_version

case "${LINUX_ARCH}" in
  x86_64) DEB_ARCH="amd64"; RPM_ARCH="x86_64" ;;
  aarch64) DEB_ARCH="arm64"; RPM_ARCH="aarch64" ;;
  armhf|armv7l) DEB_ARCH="armhf"; RPM_ARCH="armv7hl" ;;
  *) DEB_ARCH="${LINUX_ARCH}"; RPM_ARCH="${LINUX_ARCH}" ;;
esac

PKG_NAME="i2ptorrents"
OPT_DIR="/opt/${APP_NAME}"
WORK="${ROOT}/dist/pkg-native-${LINUX_ARCH}"
rm -rf "${WORK}"
mkdir -p "${WORK}/root${OPT_DIR}" \
         "${WORK}/root/usr/bin" \
         "${WORK}/root/usr/share/applications" \
         "${WORK}/root/usr/share/icons/hicolor/512x512/apps" \
         "${WORK}/root/usr/share/doc/${PKG_NAME}"

cp -a "${STAGE}/." "${WORK}/root${OPT_DIR}/"
ICON_SRC="${ROOT}/icon.png"
[ -f "${ICON_SRC}" ] || ICON_SRC="${ROOT}/image.png"
cp "${ICON_SRC}" "${WORK}/root/usr/share/icons/hicolor/512x512/apps/i2ptorrents.png"
cp "${ROOT}/LICENSE" "${WORK}/root/usr/share/doc/${PKG_NAME}/copyright"
cp "${ROOT}/AUTHORS" "${WORK}/root/usr/share/doc/${PKG_NAME}/AUTHORS"
printf '%s\n' "${RELEASE_VERSION}" > "${WORK}/root/usr/share/doc/${PKG_NAME}/version"

cat > "${WORK}/root/usr/share/applications/i2ptorrents.desktop" <<EOF
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

cat > "${WORK}/root/usr/bin/${APP_NAME}" <<EOF
#!/bin/sh
exec "${OPT_DIR}/${APP_NAME}" "\$@"
EOF
chmod +x "${WORK}/root/usr/bin/${APP_NAME}"
chmod +x "${WORK}/root${OPT_DIR}/${APP_NAME}" || true

mkdir -p "${ROOT}/dist"

# --- .deb ---
if command -v dpkg-deb >/dev/null 2>&1; then
  DEB_ROOT="${WORK}/deb"
  rm -rf "${DEB_ROOT}"
  mkdir -p "${DEB_ROOT}/DEBIAN"
  cp -a "${WORK}/root/." "${DEB_ROOT}/"
  SIZE_KB="$(du -sk "${DEB_ROOT}" | awk '{print $1}')"
  cat > "${DEB_ROOT}/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${RELEASE_VERSION}
Section: net
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: Vade <noreply@users.noreply.github.com>
Installed-Size: ${SIZE_KB}
Depends: libc6
Recommends: libgl1, libegl1, libxkbcommon0, libdbus-1-3, libfontconfig1, libfreetype6
Description: Desktop GUI for i2pd torrents
 Cross-platform Qt 6 client for the built-in i2pd torrent tunnel.
EOF
  DEB_OUT="${ROOT}/dist/${APP_NAME}-${RELEASE_VERSION}-linux-${LINUX_ARCH}.deb"
  dpkg-deb --build --root-owner-group "${DEB_ROOT}" "${DEB_OUT}"
  echo "✔ Built ${DEB_OUT}"
else
  echo "WARNING: dpkg-deb not found; skipping .deb" >&2
fi

# --- .rpm (Fedora/RHEL) ---
if command -v rpmbuild >/dev/null 2>&1; then
  RPM_TOP="${WORK}/rpm"
  rm -rf "${RPM_TOP}"
  mkdir -p "${RPM_TOP}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
  SPEC="${RPM_TOP}/SPECS/${PKG_NAME}.spec"
  cat > "${SPEC}" <<EOF
Name:           ${PKG_NAME}
Version:        ${RELEASE_VERSION}
Release:        1%{?dist}
Summary:        Desktop GUI for i2pd torrents
License:        BSD-3-Clause
URL:            https://github.com/MetanoicArmor/i2ptorrents-gui
BuildArch:      ${RPM_ARCH}
AutoReqProv:    no

%description
Cross-platform Qt 6 client for the built-in i2pd torrent tunnel.
Bundles its own Qt libraries under ${OPT_DIR}.

%prep

%build

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
cp -a "${WORK}/root/." %{buildroot}/

%files
${OPT_DIR}
/usr/bin/${APP_NAME}
/usr/share/applications/i2ptorrents.desktop
/usr/share/icons/hicolor/512x512/apps/i2ptorrents.png
/usr/share/doc/${PKG_NAME}

%changelog
* $(LC_ALL=C date -u +'%a %b %d %Y') Vade <noreply@users.noreply.github.com> - ${RELEASE_VERSION}-1
- Automated CI package
EOF
  rpmbuild -bb \
    --define "_topdir ${RPM_TOP}" \
    --define "_build_id_links none" \
    "${SPEC}"
  RPM_BUILT="$(find "${RPM_TOP}/RPMS" -type f -name '*.rpm' | head -n 1)"
  [ -n "${RPM_BUILT}" ] || die "rpmbuild produced no package"
  RPM_OUT="${ROOT}/dist/${APP_NAME}-${RELEASE_VERSION}-linux-${LINUX_ARCH}.rpm"
  cp -f "${RPM_BUILT}" "${RPM_OUT}"
  echo "✔ Built ${RPM_OUT}"
else
  echo "WARNING: rpmbuild not found; skipping .rpm (install rpm/rpm-build)" >&2
fi
