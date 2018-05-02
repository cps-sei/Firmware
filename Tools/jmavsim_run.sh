#! /bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR/jMAVSim"

udp_port=14560
extra_args=
baudrate=921600
device=
ip="127.0.0.1"
window=
while getopts ":b:d:p:qr:i:w" opt; do
	case $opt in
		b)
			baudrate=$OPTARG
			;;
		d)
			device="$OPTARG"
			;;
		i)
			ip="$OPTARG"
			;;
		p)
			udp_port=$OPTARG
			;;
		q)
			extra_args="$extra_args -qgc"
			;;
		r)
			extra_args="$extra_args -r $OPTARG"
			;;
		w)
		    window="1"
		    ;;
		\?)
			echo "Invalid option: -$OPTARG" >&2
			exit 1
			;;
	esac
done

if [ "$device" == "" ]; then
	device="-udp $ip:$udp_port"
else
	device="-serial $device $baudrate"
fi

ant create_run_jar copy_res
if [ "$window" == "" ]; then
    cd out/production

    java -XX:GCTimeRatio=20 -Djava.ext.dirs= -jar jmavsim_run.jar $device $extra_args
    ret=$?
    if [ $ret -ne 0 -a $ret -ne 130 ]; then # 130 is Ctrl-C
	# if the start of java fails, it's probably because the GC option is not
	# understood. Try starting without it
	java -Djava.ext.dirs= -jar jmavsim_run.jar $device $extra_args
    fi
else
    #gnome-terminal -e "java -Djava.ext.dirs= -jar jmavsim_run.jar $device $extra_args"
    gnome-terminal -e "../runjmavsim.sh $device $extra_args"
fi

