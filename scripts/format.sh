#!/bin/bash

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
cd "${PROJ_DIR}"

DIFF_CMD=diff
DIFF_PARAMS=-u

if [ -t 1 ] ; then 
  if [ -x /usr/bin/colordiff ] ; then
    PAGER=${PAGER:=less -r}
  else
    PAGER=${PAGER:=less}
  fi
  DIFF_CMD=colordiff
else
  PAGER=cat
fi


format_diff() {
  local f="$1"
  "$DIFF_CMD" $DIFF_PARAMS "$f" <(clang-format -style=google "$f")
}

export DIFF_CMD
export DIFF_PARAMS
export -f format_diff
find src/ \( -name \*.cpp -o -name \*.h \) -print0 \
  | xargs -0 -n1 bash -c 'format_diff "$1"' {} \
  | $PAGER

