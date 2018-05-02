#!/bin/bash

PX4_ROOT=$(dirname $0)/..

$PX4_ROOT/Tools/sitl_run.sh $PX4_ROOT/build/posix_sitl_default/px4 posix-configs/SITL/init/ekf2 none jmavsim iris-nodll $PX4_ROOT $PX4_ROOT/build/posix_sitl_default
