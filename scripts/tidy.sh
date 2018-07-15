#/bin/sh

PROJ_DIR="$(dirname "$0")/.."

if [ $# -eq 0 ] ; then
  clang-tidy -format-style=google -p "$PROJ_DIR" -header-filter=src/.* \
    $PROJ_DIR/src/*.cpp
else
  clang-tidy -format-style=google -p "$PROJ_DIR" -header-filter=src/.* \
    "$@"
fi


