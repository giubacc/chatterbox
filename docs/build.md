# Build

- [Build](#build)
  - [Build requirements](#build-requirements)
  - [How to build](#how-to-build)
    - [Build using a Docker builder image](#build-using-a-docker-builder-image)

## Build requirements

Currently, `chatterbox` can be solely built under Linux.

If you wish to build the `chatterbox` binary directly on your Linux
distribution, ensure your system provides all the required dependencies.
You can figure out what are the needed packages for your system by inspecting the
[builder Dockerfile](../scripts/Dockerfile-leap.builder) for OpenSuse Tumbleweed.

## How to build

After you have checked out all the `chatterbox`'s submodules with:

```shell
git submodule update --init --recursive
```

Build the `chatterbox` binary, alongside with all its dependencies, with:

```shell
cd scripts
./build.sh build-all
```

### Build using a Docker builder image

It is possible to use a Docker builder image to compile the binary
of `chatterbox`.

- `scripts/Dockerfile-leap.builder`
- `scripts/Dockerfile-ubuntu.builder`

To build `chatterbox` with the Docker builder image, your system needs
to provide Docker.

To create the `chatterbox` builder image:

```shell
cd scripts
./build.sh builder-create
```

```shell
$ docker images

REPOSITORY                              TAG
localhost/chatterbox-ubuntu-builder     latest
```

Using the image, it is possible to build the `chatterbox` binary
for the target system:

```shell
cd scripts
./build.sh builder-build
```
