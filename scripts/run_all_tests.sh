#!/usr/bin/env bash
set -e

# Terminal Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo -e "\n${BOLD}${BLUE}================================================================${NC}"
echo -e "${BOLD}${BLUE}   UNIVERSITY TIMETABLE MANAGEMENT SYSTEM — TEST SUITE   ${NC}"
echo -e "${BOLD}${BLUE}================================================================${NC}\n"

# 1. C++ Engine Tests
echo -e "${BOLD}[1/3] Running Native C++ Clash & Resolver Unit Tests...${NC}"
make -C "${ROOT_DIR}/PBL" test
echo -e "${GREEN}✓ All C++ Engine & Resolver tests passed successfully.${NC}\n"

# 2. Backend & Integration Tests
echo -e "${BOLD}[2/3] Running Backend API & Integration Test Suites...${NC}"
npm test --prefix "${ROOT_DIR}/backend"
echo -e "${GREEN}✓ All Backend & Integration tests passed successfully.${NC}\n"

# 3. Frontend Authentication & Client Tests
echo -e "${BOLD}[3/3] Running Admin Portal Frontend Test Suites...${NC}"
node "${ROOT_DIR}/adminfrontend/tests/auth.test.js"
node "${ROOT_DIR}/adminfrontend/tests/live_auth.test.js"
echo -e "${GREEN}✓ All Frontend Auth & Client tests passed successfully.${NC}\n"

echo -e "${BOLD}${GREEN}================================================================${NC}"
echo -e "${BOLD}${GREEN}   ALL SYSTEM TEST SUITES PASSED (C++, BACKEND, FRONTEND)       ${NC}"
echo -e "${BOLD}${GREEN}================================================================${NC}\n"
