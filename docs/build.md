# Build

- [Build](#build)
  - [Build requirements](#build-requirements)
  - [How to build](#how-to-build)
    - [Build using the OpenSuse Leap Docker builder image](#build-using-the-opensuse-leap-docker-builder-image)

## Build requirements

Currently, `chatterbox` can be solely built under Linux.

If you wish to build the `chatterbox` binary directly on your Linux
distribution, ensure your system provides all the required dependencies.
You can figure out what are the needed packages for your system by inspecting the
[builder Dockerfile](../scripts/Dockerfile.builder) for OpenSuse Tumbleweed.

## How to build

After you have checked out all the `chatterbox`'s submodules with:

```shell
git submodule init
git submodule update
```

Build the `chatterbox` binary, alongside with all its dependencies, with:

```shell
cd scripts
./build.sh build-all
```

### Build using the OpenSuse Leap Docker builder image

It is possible to use a Docker builder image to compile the binary
of `chatterbox`.

- `scripts/Dockerfile.builder`

To build `chatterbox` with the Docker builder image, your system needs
to provide Podman or Docker.

To create the `chatterbox` builder image:

```shell
cd scripts
./build.sh builder-create
```

```shell
$ docker images

REPOSITORY                                  TAG
localhost/chatterbox-builder                latest
```

Using this image, it is possible to build the `chatterbox` binary
for OpenSuse Leap:

```shell
cd scripts
./build.sh builder-build
```
