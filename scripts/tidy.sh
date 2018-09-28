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

if [ $# -eq 0 ] ; then
  clang-tidy -format-style=google -p "$PROJ_DIR" -header-filter=src/.* \
    $PROJ_DIR/src/*.cpp
else
  clang-tidy -format-style=google -p "$PROJ_DIR" -header-filter=src/.* \
    "$@"
fi


