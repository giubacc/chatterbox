# chatterbox

## Description

`chatterbox` is a tool to define restful conversations with a
generic endpoint.  
You can define scenarios with a json formalism describing conversations between the
client (chatterbox) and the endpoint.  
In polling mode, you can drop scenario files into a directory monitored by `chatterbox`
and see them being consumed by the tool.  
Every time a new scenario is placed within the directory, `chatterbox`
parses it and then generates the conversation(s) against the endpoint(s).  
Once the scenario has been processed, the file is deleted or optionally moved
into `${directoy}/consumed`.  

### Scripting capabilities

`chatterbox` embeds [V8](https://v8.dev/), that is Google’s open source high-performance
JavaScript and WebAssembly engine.  
This allows the user to define scenarios in a dynamic way.  
For example, the user could choose to compute the value of a certain
rest-call's property as the result of a user defined JavaScript function.  
More use cases could benefit from this capability and could be evaluated
in the future as the tool evolves.

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
        chatterbox [-i <input>] [-o <output>] [-p <path>] [-m] [-l <event log output>] [-v
                            <event log verbosity>]

OPTIONS
        -i, --input specify the input scenario [filename]
        -o, --output
                    specify the output channel [stdout, stderr, filename]

        -p, --poll  monitor filesystem for new scenarios
        -m, --move  move scenario files once consumed
        -l, --log   specify the event log output channel [stderr, stdout, filename]
        -v, --verbosity
                    specify the event log verbosity [off, dbg, trc, inf, wrn, err]
```

## Conversation scenario format

The conversation scenario format is pretty straightforward:
it is a json defining properties for the conversations you want to realize.

At the root level, or scenario level, you define an array of `conversations`:

```json
{
  "conversations": [
    {
      "host" : "http://service1.host1.domain1",
      "conversation": []
    },
    {
      "host" : "https://service2.host2.domain2",
      "conversation": []
    }
  ]
}
```

This means that the tool is able to issue a set of restful calls against
multiple endpoints.

A `conversation` is defined as an array of `talk`(s):

```json
{
  "conversation": [
    {
      "for" : 1,
      "talk" : {
        "auth" : "aws_v4",
        "verb" : "GET",
        "uri" : "foo",
        "query_string" : "param=value",
      }
    },
    {
      "for" : 4,
      "talk" : {
        "auth" : "aws_v2",
        "verb" : "HEAD",
        "uri" : "bar",
        "query_string" : "param=value",
      }
    }
  ]
}
```

A `talk` describes a single `HTTP` call.  
You can repeat a `talk` for `n` times specifying the `for` attribute
in the talk's context.

## Scripted scenarios

Every time a scenario runs, a brand new JavaScript context is spawned
into `V8` engine and it lasts for the whole scenario's lifespan.  
Generally speaking, attributes can be scripted defining JavaScript functions
that are executed into the scenario's JavaScript context.  
For example, the `query_string` attribute of a talk could be defined as this:

```json
{
  "talk" : {
    "auth" : "aws_v2",
    "verb" : "HEAD",
    "uri" : "bar",
    "query_string" : {
      "function": "GetQueryString",
      "args": ["foo", "bar"]
    },
  }
}
```

When the scenario runs, the `query_string` attribute's value is
evaluated as a JavaScript function named: `GetQueryString` taking 2 string parameters
valued respectively `foo` and `bar`.  
The `GetQueryString` function must be defined inside a file with
extension `.js` and placed into the directory that is being monitored by `chatterbox`.

```javascript

function GetQueryString(p1, p2) {
  log("inf", "GetQueryString", "Invoked with: " + p1 + "," + p2);
  return p1+p2;
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
          "for" : 1,
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
