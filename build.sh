#!/bin/sh

set -x

SOURCE_DIR=`pwd`
BUILD_DIR=${BUILD_DIR:-./build}
INSTALL_DIR=${INSTALL_DIR:-../install}

mkdir -p $BUILD_DIR \
    && cd $BUILD_DIR \
    && cmake \
        -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
        $SOURCE_DIR \
    && make $*