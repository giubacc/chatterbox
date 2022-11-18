#!/bin/bash

image=ghcr.io/giubacc/chatterbox-opensuse:latest
host_path=$(realpath ${1})

podman run \
	-it \
	--network=host \
	-v $host_path:/monitor \
	$image -p /monitor
