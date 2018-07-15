#/bin/sh

PROJ_DIR="$(dirname "$0")/.."
cd "${PROJ_DIR}"
clang-tidy -checks='-*,clang-analyzer-*,google-*,modernize-*,misc-*,performance-*,portability-*,readability-*,-google-runtime-references,-readability-implicit-bool-conversion' -p . -header-filter=src/.* src/*.cpp


