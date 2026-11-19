#!/usr/bin/env bash
# Build the .deb inside a clean Ubuntu container, so you can make packages
# for releases other than the one you run. Needs Docker.
#   packaging/build-deb-docker.sh            # Ubuntu 24.04 and 26.04
#   packaging/build-deb-docker.sh 24.04      # one release
# Output goes to dist/.
set -euo pipefail

cd "$(dirname "$0")/.."
RELEASES=("${@:-24.04 26.04}")
read -r -a RELEASES <<<"${RELEASES[*]}"

for release in "${RELEASES[@]}"; do
    echo "==> Ubuntu $release"
    docker run --rm -v "$PWD":/src:ro -v "$PWD/dist":/dist "ubuntu:$release" bash -c '
        set -e
        apt-get update -qq
        DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
            build-essential cmake pkg-config qt6-base-dev libqt6sql6-sqlite libxcb1-dev \
            dpkg-dev file >/dev/null
        cp -r /src /tmp/src && rm -rf /tmp/src/build*
        OUT_DIR=/dist /tmp/src/packaging/build-deb.sh
        chown '"$(id -u):$(id -g)"' /dist/*.deb'
done
