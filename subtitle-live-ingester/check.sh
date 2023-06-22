#!/bin/sh
set -eu
$SIGNALS_PATH/scripts/reformat.sh
make -j
LD_LIBRARY_PATH=$EXTRA/lib bin/unittests.exe && echo OK
