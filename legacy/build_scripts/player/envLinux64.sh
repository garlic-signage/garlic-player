#!/bin/bash

set -e
SCRIPTDIR=$(dirname "$0")
source $SCRIPTDIR/env.sh

export QT_PATH_RUNTIME=$QT_BASE_PATH/$QT_VERSION/gcc_64
export SHADOW_BUILD_DIR="gcc_64-$SHADOW_BUILD_DIR"
export TEST_DIR=tests

echo 
echo ========== enter shadowbuild dir 
echo 
mkdir -p $SHADOW_BUILD_DIR
cd $SHADOW_BUILD_DIR
