#!/usr/bin/env bash
set -e
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PBL_DIR="$ROOT_DIR/PBL"

echo "Building C++ clash detection and resolver binaries in $PBL_DIR..."
cd "$PBL_DIR"
make all
echo "C++ binaries built successfully in $PBL_DIR/bin/"
