#!/bin/bash
set -e

SCRIPTDIR=$(dirname "$0")
source $SCRIPTDIR/envLinux64.sh

mkdir -p $TEST_DIR
cd $TEST_DIR

export CCACHE_DISABLE=1
export CCACHE_RECACHE=1

$QT_PATH_RUNTIME/bin/qmake $GARLIC_DIR/src/libs_only.pro "CONFIG+=debug gcov"
make -j $DEV_JOBS --silent

# =============================================== 
# execute tests
# ===============================================

$QT_PATH_RUNTIME/bin/qmake $GARLIC_DIR/tests/testsuite.pro -r -spec linux-g++ "CONFIG+=debug gcov"

make -j1 --silent check TESTARGS="-silent"

cd ..
exit 0;
