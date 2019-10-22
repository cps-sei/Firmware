#!/bin/bash

SRC_DIR=$(cd `dirname $0`/..; pwd)

$SRC_DIR/Tools/hitl_run.sh $SRC_DIR/build/px4_sitl_default/bin/px4 none jmavsim none $SRC_DIR $SRC_DIR/build/px4_sitl_default
