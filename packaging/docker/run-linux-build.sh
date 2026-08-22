#!/usr/bin/env bash
# Build Linux zip + AppImage inside Ubuntu 24.04 (glibc 2.39).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DOCKER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_TAG="${I2PTORRENTS_LINUX_DOCKER_TAG:-i2ptorrents-linux:noble-glibc239}"
DOCKERFILE="${DOCKER_DIR}/Dockerfile.linux-noble-glibc239"

pick_runtime() {
  if [ -n "${I2PTORRENTS_CONTAINER_RUNTIME:-}" ]; then
    printf '%s\n' "${I2PTORRENTS_CONTAINER_RUNTIME}"
    return 0
  fi
  if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
    printf '%s\n' docker
    return 0
  fi
  if command -v podman >/dev/null 2>&1 && podman info >/dev/null 2>&1; then
    printf '%s\n' podman
    return 0
  fi
  return 1
}

if ! RT="$(pick_runtime)"; then
  echo "ERROR: no container runtime (docker / podman)." >&2
  exit 1
fi

echo "==> Runtime: ${RT}"
if [ "${RT}" = docker ] && [ "${I2PTORRENTS_DOCKER_BUILDKIT:-}" != 0 ]; then
  if docker buildx version >/dev/null 2>&1; then
    export DOCKER_BUILDKIT=1
    echo "==> DOCKER_BUILDKIT=1"
  fi
fi

NET_ARGS=()
if [ -n "${I2PTORRENTS_DOCKER_NETWORK:-}" ]; then
  NET_ARGS=(--network "${I2PTORRENTS_DOCKER_NETWORK}")
fi

echo "==> Building image ${IMAGE_TAG}"
"${RT}" build "${NET_ARGS[@]}" -f "${DOCKERFILE}" -t "${IMAGE_TAG}" "${DOCKER_DIR}"

echo "==> Running build-linux.sh in container (mount ${ROOT} -> /src)"
DOCKER_RUN_IT=()
if [ -t 0 ] && [ -t 1 ]; then
  DOCKER_RUN_IT=(-it)
fi
"${RT}" run --rm "${DOCKER_RUN_IT[@]}" "${NET_ARGS[@]}" \
  -e "QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen}" \
  -e "APPIMAGE_EXTRACT_AND_RUN=${APPIMAGE_EXTRACT_AND_RUN:-1}" \
  -v "${ROOT}:/src:rw" \
  -w /src \
  "${IMAGE_TAG}" \
  ./build-linux.sh

echo "==> Done. Artifacts in ${ROOT}/dist/ and ${ROOT}/I2PTorrents-linux-*"
