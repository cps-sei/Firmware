#!/bin/bash
if [ `whoami` != "root" ]; then
    echo This script must be run with sudo
    exit 1
fi

set +m
./build/posix_rpi_ureboot/px4 posix-configs/rpi/px4-hil.config
set -m
