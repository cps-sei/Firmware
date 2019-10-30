#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
src_path="$SCRIPT_DIR/.."

build_path=${src_path}/build/px4_sitl_senf

# instance number (main is instance 0)
n=1
working_dir="$build_path/instance_$n"
[ ! -d "$working_dir" ] && mkdir -p "$working_dir"

pushd "$working_dir" &>/dev/null
echo "starting instance $n in $(pwd)"
PX4_SIM_MODEL="iris" ../bin/px4 -i $n "$src_path/ROMFS/px4fmu_common" -s etc/init.d-posix/rcS-hil-enf
#PX4_SIM_MODEL="iris" ../bin/px4 "$src_path/ROMFS/px4fmu_common" -s etc/init.d-posix/rcS-hil-enf

# PX4_SIM_MODEL="iris" build/px4_sitl_senf/bin/px4 "/Users/gmoreno/swdev/Firmware"/ROMFS/px4fmu_common -s etc/init.d-posix/rcS-hil-enf -t "/Users/gmoreno/swdev/Firmware"/test_data

popd &>/dev/null
