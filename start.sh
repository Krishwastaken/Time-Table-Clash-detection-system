#!/usr/bin/env bash
# One-command startup script for University Timetable Management System
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

echo "============================================================"
echo "  University Timetable Management & Clash Detection System  "
echo "============================================================"

# 1. Compile C++ Engine if not built
if [ ! -f "PBL/bin/engine_cli" ]; then
  echo "[1/2] Compiling C++ clash detection and resolution engine..."
  make -C PBL
else
  echo "[1/2] C++ engine binary detected at PBL/bin/engine_cli"
fi

# 2. Start the backend server and static frontend
echo "[2/2] Starting backend server and Admin Portal on port 3000..."
node backend/src/server.js
