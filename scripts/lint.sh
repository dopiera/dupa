#/bin/sh

PROJ_DIR="$(dirname "$0")/.."
cd "${PROJ_DIR}"
./third_party/cpplint.py --filter='-legal,-runtime/references,-build/include,-build/c++11,-whitespace/indent,-whitespace/comments' src/*cpp src/*.h


