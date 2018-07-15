#/bin/sh

CHECKS='-*,clang-analyzer-*,google-*,modernize-*,misc-*,performance-*,portability-*,readability-*,-google-runtime-references,-readability-implicit-bool-conversion'

PROJ_DIR="$(dirname "$0")/.."

if [ $# -eq 0 ] ; then
  clang-tidy -checks="$CHECKS" -p "$PROJ_DIR" -header-filter=src/.* \
    $PROJ_DIR/src/*.cpp
else
  clang-tidy -checks="$CHECKS" -p "$PROJ_DIR" -header-filter=src/.* \
    "$@"
fi


