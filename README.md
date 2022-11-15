# chatterbox

In nutshell, `chatterbox` is a tool to define conversations with a generic
rest endpoint.  
You can write json files describing scenarios for conversations between the
client (chatterbox) and a restful server.  
In polling mode, you can drop the files into `chatterbox` working directory
to see them being consumed by the tool.  
Every time a new json file is placed within the working directory, `chatterbox`
parses it and then generates the conversation(s) against the rest endpoint(s).  
Once the file has been processed, the file is deleted or optionally moved
into `./consumed` directory.

## Build requirements

Currently, `chatterbox` can be solely built under Linux.  
If you intend to build the `chatterbox` binary directly on your Linux
distribution, ensure your system provides:

- `Make`, `AutoGen`
- `GCC` with `g++`
- `Python`
- `CMake`
- `Ninja`
- `Git`
- `curl`

## How to build

First, ensure you have checked out all the `chatterbox`'s submodules:

```shell
git submodule init
git submodule update
```

## Build on bare metal

Build the `chatterbox` binary alongside with all its dependencies:

```shell
cd scripts
./build.sh build-all
```

## Build with a Dockerfile builder image

It is possible to use a Dockerfile builder image to compile the binary
of `chatterbox`.

Current supported Dockerfile list:

- `Dockerfile.builder-opensuse`

To build `chatterbox` with a Dockerfile builder image, your system needs
to provide Podman.

To create the `chatterbox` builder image:

```shell
cd scripts
./build.sh builder-create
```

```shell
$ podman images

REPOSITORY                                 TAG         IMAGE ID      CREATED             SIZE
localhost/chatterbox-builder-opensuse      latest      5739a6d404bc  38 minutes ago      7.57 GB
```

With this image it is possible to build the `chatterbox` binary:

```shell
cd scripts
./build.sh builder-build
```

Once you have built the `chatterbox` binary, you can create the
final consumable chatterbox image:

```shell
cd scripts
./build.sh chatterbox-create
```

```shell
$ podman images

REPOSITORY                                 TAG         IMAGE ID      CREATED             SIZE
localhost/chatterbox-opensuse              latest      d80adb80f677  About a minute ago  331 MB
```

## Usage

```text
SYNOPSIS
        chatterbox [-p] [-m] [-l <logging type>] [-v <logging verbosity>]

OPTIONS
        -p, --poll  monitor working directory for new scenarios
        -m, --move  move scenario files once consumed
        -l, --log   specify logging type [shell, filename]
        -v, --verbosity
                    specify logging verbosity [off, dbg, trc, inf, wrn, err]
```

## conversation scenarios

An S3 conversation scenario example:

```json
{
  "conversations": [
    {
      "host" : "s3gw.127.0.0.1.omg.howdoi.website:7480",
      "access_key" : "test",
      "secret_key" : "test",
      "service" : "s3",

      "conversation": [
        {
          "for" : 1,
          "talk" : {
            "auth" : "v4",
            "verb" : "PUT",
            "uri" : "foo",
            "query_string" : "format=json",
            "body_res_dump" : true,
            "body_res_format" : "json"
          }
        },
        {
          "for" : 3,
          "talk" : {
            "auth" : "v4",
            "verb" : "HEAD",
            "uri" : "foo",
            "query_string" : "format=json",
            "body_res_dump" : true,
            "body_res_format" : "json"
          }
        }
      ]
    }
  ]
}
```
