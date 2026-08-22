#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${ROOT}/assets/fonts"
INTER_VERSION="4.1"
ARCHIVE="${DEST}/.inter-${INTER_VERSION}.zip"

FONTS=(
  Inter-Regular.otf
  Inter-Medium.otf
  Inter-SemiBold.otf
  Inter-Bold.otf
)

die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

download_inter() {
  require_cmd curl
  require_cmd unzip
  mkdir -p "${DEST}"
  echo "==> Downloading Inter ${INTER_VERSION}"
  curl -fsSL -o "${ARCHIVE}" \
    "https://github.com/rsms/inter/releases/download/v${INTER_VERSION}/Inter-${INTER_VERSION}.zip"
  tmp="$(mktemp -d)"
  trap 'rm -rf "${tmp}"' EXIT
  unzip -q "${ARCHIVE}" -d "${tmp}"
  for font in "${FONTS[@]}"; do
    src="$(find "${tmp}" -name "${font}" -print -quit)"
    [ -n "${src}" ] || die "missing ${font} in Inter archive"
    cp -f "${src}" "${DEST}/${font}"
    echo "    ${font}"
  done
  if [ -f "${tmp}/LICENSE.txt" ]; then
    cp -f "${tmp}/LICENSE.txt" "${DEST}/Inter-OFL.txt"
  elif [ -f "${tmp}/OFL.txt" ]; then
    cp -f "${tmp}/OFL.txt" "${DEST}/Inter-OFL.txt"
  fi
  rm -f "${ARCHIVE}"
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing command: $1"
}

mkdir -p "${DEST}"
missing=0
for font in "${FONTS[@]}"; do
  if [ ! -f "${DEST}/${font}" ]; then
    missing=1
    break
  fi
done

if [ "${missing}" -eq 0 ]; then
  echo "==> Inter UI fonts already present in ${DEST}"
  exit 0
fi

download_inter
rm -f "${DEST}"/SF-Pro-*.otf "${DEST}"/SF-Pro-*.ttf 2>/dev/null || true
echo "✔ Inter UI fonts ready (SIL Open Font License)"
