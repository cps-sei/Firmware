#! /bin/bash

if [ -z ${PX4_DOCKER_REPO+x} ]; then
	echo "guessing PX4_DOCKER_REPO based on input";
	if [[ $@ =~ .*px4_fmu.* ]]; then
		# nuttx-px4fmu-v{1,2,3,4,5}
		PX4_DOCKER_REPO="px4io/px4-dev-nuttx:2019-07-29"
	elif [[ $@ =~ .*navio2.* ]] || [[ $@ =~ .*raspberry.* ]] || [[ $@ =~ .*bebop.* ]]; then
		# posix_rpi_cross, posix_bebop_default
		PX4_DOCKER_REPO="px4io/px4-dev-raspi:2019-07-29"
	elif [[ $@ =~ .*eagle.* ]] || [[ $@ =~ .*excelsior.* ]]; then
		# eagle, excelsior
		PX4_DOCKER_REPO="lorenzmeier/px4-dev-snapdragon:2018-09-12"
	elif [[ $@ =~ .*ocpoc.* ]]; then
		# aerotennaocpoc_ubuntu
		PX4_DOCKER_REPO="px4io/px4-dev-armhf:2019-07-29"
	elif [[ $@ =~ .*clang.* ]] || [[ $@ =~ .*scan-build.* ]]; then
		# clang tools
		PX4_DOCKER_REPO="px4io/px4-dev-clang:2019-07-29"
	elif [[ $@ =~ .*tests* ]]; then
		# run all tests with simulation
		PX4_DOCKER_REPO="px4io/px4-dev-simulation:2019-07-29"
	elif [[ $@ =~ .*sitl* ]]; then
		# run all tests with simulation
		PX4_DOCKER_REPO="px4io/px4-dev-simulation:2019-07-29"
	fi
else
	echo "PX4_DOCKER_REPO is set to '$PX4_DOCKER_REPO'";
fi

# otherwise default to nuttx
if [ -z ${PX4_DOCKER_REPO+x} ]; then
	PX4_DOCKER_REPO="px4io/px4-dev-nuttx:2019-07-29"
fi

# docker hygiene

#Delete all stopped containers (including data-only containers)
#docker rm $(docker ps -a -q)

#Delete all 'untagged/dangling' (<none>) images
#docker rmi $(docker images -q -f dangling=true)

echo "PX4_DOCKER_REPO: $PX4_DOCKER_REPO";

PWD=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
SRC_DIR=$PWD/../

CCACHE_DIR=${HOME}/.ccache
mkdir -p "${CCACHE_DIR}"

DEFAULTPUB=
#"--publish 14570:14570/udp"

HAS_EXTRA_ARGS=0
EXTRA_ARGS=""
for a in "$@"
do
    if [ "$a" == "--" ]; then
	HAS_EXTRA_ARGS=1
	break
    fi
done
if [ $HAS_EXTRA_ARGS == 1 ]; then
    DEFAULTPUB=""
    for a in "$@"
    do
	if [ "$a" == "--" ]; then
	    shift
	    break;
	fi
	EXTRA_ARGS="$EXTRA_ARGS $a"
	shift
    done
fi

docker run -it --rm -w "${SRC_DIR}" \
	--env=AWS_ACCESS_KEY_ID \
	--env=AWS_SECRET_ACCESS_KEY \
	--env=BRANCH_NAME \
	--env=CCACHE_DIR="${CCACHE_DIR}" \
	--env=CI \
	--env=CODECOV_TOKEN \
	--env=COVERALLS_REPO_TOKEN \
	--env=LOCAL_USER_ID="$(id -u)" \
	--env=PX4_ASAN \
	--env=PX4_MSAN \
	--env=PX4_TSAN \
	--env=PX4_UBSAN \
	--env=TRAVIS_BRANCH \
	--env=TRAVIS_BUILD_ID \
	$DEFAULTPUB \
	--volume=${CCACHE_DIR}:${CCACHE_DIR}:rw \
	--volume=${SRC_DIR}:${SRC_DIR}:rw \
	-e DISPLAY=host.docker.internal:0 \
	$EXTRA_ARGS \
	${PX4_DOCKER_REPO} /bin/bash -c "$1 $2 $3"
