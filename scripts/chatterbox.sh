#!/bin/bash

image=${CHBOX_IMAGE:-"ghcr.io/giubacc/chatterbox:latest"}
host_path=$(realpath ${HOST_PATH:-"."})

usage() {
  cat << EOF
env variables
  CHBOX_IMAGE         Specify the chatterbox image to use.
  HOST_PATH           Specify the host's path that is mounted on container's /monitor directory.

examples
  HOST_PATH=/tmp/test chatterbox.sh -m                    Monitor host's /tmp/test for new scenarios.
  HOST_PATH=/tmp/test chatterbox.sh -i scenario.json      Process host's /tmp/test/scenario.json
EOF
}

if [ "$#" == 0 ]; then
  usage && exit 1
fi

podman run \
	-it \
	--network=host \
	-v $host_path:/monitor \
	$image $@ -p /monitor
