#/bin/bash

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
  "$DIFF_CMD" $DIFF_PARAMS "$f" <(clang-format "$f")
}

export DIFF_CMD
export DIFF_PARAMS
export -f format_diff
find src/ \( -name \*.cpp -o -name \*.h \) -print0 \
  | xargs -0 -n1 bash -c 'format_diff "$1"' {} \
  | $PAGER

