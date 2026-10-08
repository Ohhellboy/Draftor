#!/bin/bash
set -euo pipefail
PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$PROJECT_DIR"
cmake -S . -B tests/build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build tests/build -j 4
ctest --test-dir tests/build -V
TEST_DIR=$(mktemp -d "${TMPDIR:-/tmp}/hanime-s3-restart.XXXXXX")
./tests/build/host_tests --restart-write "$TEST_DIR"
./tests/build/host_tests --restart-read "$TEST_DIR"
cmake -S . -B tests/build-sanitize -DSANITIZE=ON
cmake --build tests/build-sanitize -j 4
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir tests/build-sanitize -V

SAN_TEST_DIR=$(mktemp -d "${TMPDIR:-/tmp}/hanime-s3-restart-san.XXXXXX")
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./tests/build-sanitize/host_tests --restart-write "$SAN_TEST_DIR"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./tests/build-sanitize/host_tests --restart-read "$SAN_TEST_DIR"

META_DIR=$(mktemp -d "${TMPDIR:-/tmp}/hanime-s3-meta-restart.XXXXXX")
./tests/build/creating_054 --restart-write "$META_DIR"
./tests/build/creating_054 --restart-read "$META_DIR"
SAN_META_DIR=$(mktemp -d "${TMPDIR:-/tmp}/hanime-s3-meta-restart-san.XXXXXX")
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./tests/build-sanitize/creating_054 --restart-write "$SAN_META_DIR"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./tests/build-sanitize/creating_054 --restart-read "$SAN_META_DIR"
