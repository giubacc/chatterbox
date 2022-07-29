#!/bin/bash

if docker network ls | grep "chatterbox-vscode"; then
  echo "network chatterbox-vscode already exists, nothing to do"
else
  echo "network chatterbox-vscode doesn't exist, creating ..."
  docker network create --driver=bridge --attachable --subnet=10.21.69.0/24 chatterbox-vscode
fi
