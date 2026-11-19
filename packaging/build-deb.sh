#!/usr/bin/env bash
# Build a .deb package for the Ubuntu/Debian version this runs on.
# Needs the build dependencies plus dpkg-dev:
#   sudo apt install build-essential cmake pkg-config qt6-base-dev libqt6sql6-sqlite libxcb1-dev dpkg-dev
# Output: dist/clipboard-linux_<version>_<distro><release>_<arch>.deb
#
# For other Ubuntu releases use packaging/build-deb-docker.sh.
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-build-deb}"
OUT_DIR="${OUT_DIR:-dist}"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DINSTALL_SYSTEM_FILES=ON -DBUILD_TESTING=OFF
cmake --build "$BUILD_DIR" -j"$(nproc)"
(cd "$BUILD_DIR" && cpack -G DEB)

. /etc/os-release
mkdir -p "$OUT_DIR"
for deb in "$BUILD_DIR"/clipboard-linux_*.deb; do
    name=$(basename "$deb" .deb)              # clipboard-linux_0.1.0_amd64
    target="$OUT_DIR/${name%_*}_${ID}${VERSION_ID}_${name##*_}.deb"
    cp "$deb" "$target"
    echo "Built $target"
done
