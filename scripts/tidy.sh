#!/bin/sh

# (C) Copyright 2018 Marek Dopiera
#
# This file is part of dupa.
#
# dupa is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# dupa is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with dupa. If not, see http://www.gnu.org/licenses/.

PROJ_DIR="$(dirname "$0")/.."

log() {
  echo "$@" > /dev/stderr
}

fail() {
  log "$@"
  exit 1
}

COMPILE_DB=$(find "$PROJ_DIR" -name compile_commands.json | xargs ls -t -1 | head -1)

[ -n "$COMPILE_DB" ] || fail "Could not find a `compile_commands.json` file. Make sure you build the project first"

COMPILE_DIR=$(dirname "$COMPILE_DB")

if [ $# -eq 0 ] ; then
  # Run clang-tidy in parallel on all .cpp files
  # Use number of CPU cores for parallel jobs
  JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
  find "$PROJ_DIR/src" -name "*.cpp" -print0 | \
    xargs -0 -P "$JOBS" -I {} \
    clang-tidy -format-style=google -p "$COMPILE_DB" -header-filter=src/.* {}
else
  # When specific files are provided, also run in parallel
  JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
  printf '%s\0' "$@" | \
    xargs -0 -P "$JOBS" -I {} \
    clang-tidy -format-style=google -p "$COMPILE_DIR" -header-filter=src/.* {}
fi


