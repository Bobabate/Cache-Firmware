#!/usr/bin/env bash
set -euo pipefail

export FIRMWARE_VERSION="v1.17.1-C1"
exec ./build.sh build-firmware Cache_mesh_pocket_room_server
