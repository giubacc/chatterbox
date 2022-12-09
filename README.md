# chatterbox

![License](https://img.shields.io/github/license/giubacc/chatterbox)
![Lint](https://github.com/giubacc/chatterbox/actions/workflows/lint.yaml/badge.svg)
![Builder Build](https://github.com/giubacc/chatterbox/actions/workflows/build-builder.yaml/badge.svg)
![Chatterbox Build](https://github.com/giubacc/chatterbox/actions/workflows/build-chatterbox.yaml/badge.svg)

## Contents

- [chatterbox](#chatterbox)
  - [Description](#description)
    - [Scripting capabilities](#scripting-capabilities)
  - [Build requirements](#build-requirements)
  - [How to build](#how-to-build)
    - [Build on the host](#build-on-the-host)
    - [Build with a Dockerfile builder image](#build-with-a-dockerfile-builder-image)
  - [Usage](#usage)
  - [Conversation scenario format](#conversation-scenario-format)
  - [Scripted scenarios](#scripted-scenarios)
  - [Examples](#examples)

## Description

`chatterbox` is a tool to define restful conversations with a
generic endpoint.

You can define scenarios with a json formalism describing conversations
between the client (chatterbox) and the endpoint.

In polling mode, you can drop scenario files into a directory monitored
by `chatterbox` and see them being consumed by the tool.
Every time a new scenario is placed within the directory, `chatterbox`
parses it and then generates the conversation(s) against the endpoint(s).
Once the scenario has been processed, the file is deleted or optionally
moved into `${directory}/consumed`.

### Scripting capabilities

`chatterbox` embeds [V8](https://v8.dev/) the Google’s open source
high-performance JavaScript and WebAssembly engine.

This allows the user to define scenarios in a dynamic way.
For example, the user could choose to compute the value of a certain
rest-call's property as the result of a user defined JavaScript function.

More use cases could benefit from scripting capabilities and these could
be evaluated in the future as the tool evolves.

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

### Build on the host

Build the `chatterbox` binary alongside with all its dependencies:

```shell
cd scripts
./build.sh build-all
```

### Build with a Dockerfile builder image

It is possible to use a Dockerfile builder image to compile the binary
of `chatterbox`.

- `scripts/Dockerfile.builder`

To build `chatterbox` with a Dockerfile builder image, your system needs
to provide Podman or Docker.

To create the `chatterbox` builder image:

```shell
cd scripts
./build.sh builder-create
```

```shell
$ podman images

REPOSITORY                                  TAG
localhost/chatterbox-builder                latest
```

With this image, it is possible to build the `chatterbox` binary:

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

REPOSITORY                                 TAG
localhost/chatterbox                       latest
```

## Usage

You typically want to write your scenarios on files and then submit them to
`chatterbox`.

Once you have your scenario ready, you can submit it to `chatterbox` with:

```shell
chatterbox -i scenario.json
```

You can specify indifferently a relative or an absolute path.
If you want, you can also specify a relative path from where files are read.

```shell
chatterbox -p /scenarios -i scenario.json
```

You can use `chatterbox` to monitor a directory on filesystem.
On this mode, `chatterbox` will constantly check the monitored directory
for new scenarios.

```shell
$ chatterbox -p /scenarios -m
>>MONITORING MODE<<
```

By default, files are moved into `${directory}/consumed` once they have
been consumed. If you prefer them to be deleted you can specify the `-d` flag.

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

A `conversation` is defined as an array of `requests`(s):

```json
{
  "conversation": [
    {
      "for" : 1,
      "auth" : "aws_v4",
      "method" : "GET",
      "uri" : "foo",
      "query_string" : "param=value"
    },
    {
      "for" : 4,
      "auth" : "aws_v2",
      "method" : "HEAD",
      "uri" : "foo",
      "query_string" : "param=value"
    }
  ]
}
```

A `request` describes a single `HTTP` call.

You can repeat a `request` for `n` times specifying the `for` attribute
in the request's context.

## Scripted scenarios

Every time a scenario runs, a brand new JavaScript context is spawned
into `V8` engine and it lasts for the whole scenario's lifespan.

Generally speaking, attributes can be scripted defining JavaScript functions
that are executed into the scenario's JavaScript context.

For example, the `query_string` attribute of a request could be defined as this:

```json
{
  "auth" : "aws_v2",
  "method" : "HEAD",
  "uri" : "bar",
  "query_string" : {
    "function": "GetQueryString",
    "args": ["bar", 41, false]
  }
}
```

When the scenario runs, the `query_string` attribute's value is
evaluated as a JavaScript function named: `GetQueryString`
taking 3 parameters.

The `GetQueryString` function must be defined inside a file with
extension `.js` and placed into the directory checked by `chatterbox`.

```javascript

function GetQueryString(p1, p2, p3) {
  if(p3){
    return "foo=default"
  }
  log("inf", "GetQueryString", "Invoked with: " + p1 + "," + p2);
  return "foo=" + p1 + p2;
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
      "service": "s3",
      "s3": {
        "access_key": "test",
        "secret_key": "test"
      },
      "conversation": [
        {
          "for" : 1,
          "auth" : "aws_v4",
          "method" : "PUT",
          "uri" : "foo",
          "query_string" : "format=json",
          "response": {
            "out": {
              "format": {
                "body": "json"
              }
            }
          }
        },
        {
          "for" : 1,
          "auth" : "aws_v4",
          "method" : "HEAD",
          "uri" : "foo",
          "response": {
            "out": {
              "format": {
                "body": "json"
              }
            }
          }
        }
      ]
    }
  ]
}
```

- Possible output conversation

```json
{
  "conversations": [
    {
      "host": "s3gw.127.0.0.1.omg.howdoi.website:7480",
      "service": "s3",
      "s3": {
        "access_key": "test",
        "secret_key": "test"
      },
      "conversation": [
        {
          "auth": "aws_v4",
          "query_string": "format=json",
          "uri": "foo",
          "method": "PUT",
          "response": {
            "body": {},
            "code": 200
          }
        },
        {
          "auth": "aws_v4",
          "uri": "foo",
          "method": "HEAD",
          "response": {
            "code": 200
          }
        }
      ]
    }
  ]
}
```
