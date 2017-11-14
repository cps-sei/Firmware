#!/bin/sh
cd `dirname $0`/jMAVSim/out/production
java -Djava.ext.dirs= -jar jmavsim_run.jar $1 $2
read -p "press key..." -n1 -s
cat > /dev/null

