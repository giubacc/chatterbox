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
        chatterbox [-o <output>] [-p <path>] [-m] [-l <event log output>] [-v <event log
                         verbosity>]

OPTIONS
        -o, --out   specify the output channel [stdout, stderr, filename]
        -p, --poll  monitor filesystem for new scenarios
        -m, --move  move scenario files once consumed
        -l, --log   specify the event log output channel [stderr, stdout, filename]
        -v, --verbosity
                    specify the event log verbosity [off, dbg, trc, inf, wrn, err]
```

## Conversation scenario format

The conversation scenario format is pretty straightforward:
it is a json defining properties for the conversations you want to realize.

At the root level you define an array of `conversations`:

```json
{
  "conversations": [
    {
      "host" : "http://service1.host1.domain1",
      "conversation": {..}
    },
    {
      "host" : "https://service2.host2.domain2",
      "s3_access_key" : "test",
      "s3_secret_key" : "test",
      "service" : "s3",
      "conversation": {..}
    }
    ..
  ]
}
```

## Examples

A simple S3 conversation where the client creates a bucket: `foobar` and then
checks that the newly created bucket actually exists.

- Input conversation

```json
{
  "conversations": [
    {
      "host" : "s3gw.127.0.0.1.omg.howdoi.website:7480",
      "s3_access_key" : "test",
      "s3_secret_key" : "test",
      "service" : "s3",

      "conversation": [
        {
          "for" : 1,
          "talk" : {
            "auth" : "aws_v4",
            "verb" : "PUT",
            "uri" : "foo",
            "query_string" : "format=json",
            "res_body_dump" : true,
            "res_body_format" : "json"
          }
        },
        {
          "for" : 3,
          "talk" : {
            "auth" : "aws_v4",
            "verb" : "HEAD",
            "uri" : "foo",
            "query_string" : "format=json",
            "res_body_dump" : true,
            "res_body_format" : "json"
          }
        }
      ]
    }
  ]
}
```

- Output rendered conversation:

```json
{
        "rendered_conversations" :
        [
                {
                        "host" : "s3gw.127.0.0.1.omg.howdoi.website:7480",
                        "rendered_conversation" :
                        [
                                {
                                        "rendered_talk" :
                                        {
                                                "auth" : "aws_v4",
                                                "data" : "",
                                                "query_string" : "format=json",
                                                "uri" : "foobar",
                                                "verb" : "PUT"
                                        },
                                        "res_body" : null,
                                        "res_code" : 200
                                },
                                {
                                        "rendered_talk" :
                                        {
                                                "auth" : "aws_v4",
                                                "data" : "",
                                                "query_string" : "format=json",
                                                "uri" : "foobar",
                                                "verb" : "HEAD"
                                        },
                                        "res_body" : null,
                                        "res_code" : 200
                                }
                        ],
                        "s3_access_key" : "test",
                        "s3_secret_key" : "test",
                        "service" : "s3"
                }
        ]
}
```
