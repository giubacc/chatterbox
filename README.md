# chatterbox

![License](https://img.shields.io/github/license/giubacc/chatterbox)
![Lint](https://github.com/giubacc/chatterbox/actions/workflows/lint.yaml/badge.svg)
![Builder Image](https://github.com/giubacc/chatterbox/actions/workflows/build-builder.yaml/badge.svg)
![Chatterbox Image](https://github.com/giubacc/chatterbox/actions/workflows/build-chatterbox.yaml/badge.svg)

## Contents

- [chatterbox](#chatterbox)
  - [Contents](#contents)
  - [Description](#description)
    - [Scripting capabilities](#scripting-capabilities)
  - [Build requirements](#build-requirements)
  - [How to build](#how-to-build)
    - [Build](#build)
    - [Build using a Dockerfile builder image](#build-using-a-dockerfile-builder-image)
  - [Usage](#usage)
    - [Optional monitoring mode](#optional-monitoring-mode)
  - [Input/Output concepts](#inputoutput-concepts)
    - [Input/Output example](#inputoutput-example)
      - [Input](#input)
      - [Possible rendered output](#possible-rendered-output)
  - [chatterbox scenario format](#chatterbox-scenario-format)
    - [Scenario context](#scenario-context)
    - [Conversation context](#conversation-context)
    - [Request context](#request-context)
    - [Response context](#response-context)
    - [Dumps and Formats](#dumps-and-formats)
  - [Scripting](#scripting)
    - [Field's value](#fields-value)
    - [Context handlers](#context-handlers)
    - [Global objects](#global-objects)
      - [Example](#example)
    - [Global functions](#global-functions)

## Description

`chatterbox` is a tool to compose RESTful conversations with a
generic endpoint.

You can define scenarios with a json formalism describing conversations
between the client (chatterbox) and the endpoint.

`chatterbox` could be useful to you when you have to:

- Prototype a RESTful protocol
- Investigate and/or probe a RESTful endpoint
- Develop a RESTful endpoint with a test-driven methodology
- Develop system tests for a RESTful endpoint

### Scripting capabilities

`chatterbox` embeds [V8](https://v8.dev/) the Google’s open source
high-performance JavaScript and WebAssembly engine.

This allows the user to define scenarios in a dynamic way.
For example, the user could choose to compute the value of a certain
input's field as the result of a user defined JavaScript function.

More use cases could benefit from scripting capabilities and these could
be evaluated in the future as the tool evolves.

## Build requirements

Currently, `chatterbox` can be solely built under Linux.

If you wish to build the `chatterbox` binary directly on your Linux
distribution, ensure your system provides all the required dependencies.

## How to build

First, ensure you have checked out all the `chatterbox`'s submodules:

```shell
git submodule init
git submodule update
```

### Build

Build the `chatterbox` binary alongside with all its dependencies:

```shell
cd scripts
./build.sh build-all
```

### Build using a Dockerfile builder image

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

A typical usage scenario is to write your scenarios on files
and then submit them to `chatterbox`.

Once you have your scenario ready, you can submit it to `chatterbox` with:

```shell
chatterbox -f scenario.json
```

You can specify indifferently a relative or an absolute path.
If you want, you can also specify a relative path from where files are read.

```shell
chatterbox -p /scenarios -f scenario.json
```

### Optional monitoring mode

You can use `chatterbox` to monitor a directory on filesystem.
On this mode, `chatterbox` will constantly check the monitored directory
for new scenarios.

```shell
$ chatterbox -p /scenarios -m
>>MONITORING MODE<<
```

By default, files are moved into `${directory}/consumed` once they have
been consumed. If you prefer them to be deleted you can specify the `-d` flag.

## Input/Output concepts

`chatterbox` works with this fundamental concept: the input provided by
the user is also the skeleton used to render the output. The output object
keeps the same format of the input, eventually enriched.

Conceptually, the input object is unwound by the `chatterbox`'s engine
to produce the final rendered output object.

Each response for every request defined in the input is inserted in the output
object at the proper place.

All the modifications operated by some JavaScript function are inserted
in the output object as well.

### Input/Output example

#### Input

```json
{
  "conversations" : [
    {
      "host" : "localhost:8080",
      "requests" : [
        {
          "method" : "DELETE",
          "uri" : "foo/bar"
        }
      ]
    }
  ]
}

```

#### Possible rendered output

```json
{
  "conversations" : [
    {
      "host" : "localhost:8080",
      "requests" : [
        {
          "method" : "DELETE",
          "uri" : "foo/bar",
          "response" : {
            "code" : 200,
            "body" : {
              "deleteResult" : "OK"
            }
          }
        }
      ]
    }
  ]
}
```

## chatterbox scenario format

The chatterbox scenario format is pretty straightforward:
it is a json defining properties for the scenarios you want to realize.

### Scenario context

At the root context, or `scenario` context, you define an array of `conversations`:

```json
{
  "conversations" : [
    {
      "host" : "http://service1.host1.domain1",
      "requests" : []
    },
    {
      "host" : "https://service2.host2.domain2",
      "requests" : []
    }
  ]
}
```

This means that the tool can issue requests against multiple endpoints.

### Conversation context

A `conversation` is defined as an array of `requests`(s):

```json
{
  "requests" : [
    {},
    {}
  ]
}
```

### Request context

A `request` describes a single `HTTP` request.

```json
{
  "for" : 1,
  "auth" : "aws_v2",
  "method" : "HEAD",
  "uri" : "foo",
  "query_string" : "param=value",
  "response" : {}
}
```

You can repeat a `request` for `n` times specifying the `for` attribute
in the request's context.

### Response context

A `response` contains the response for a single `request`.

```json
{
  "code" : 200,
  "body" : {
    "opResult" : "OK"
  }
}
```

### Dumps and Formats

At every context in the input object is always possible to define
an `out` node describing what should be rendered in the output object.

Valid contexts are:

- `scenario`
- `conversation`
- `request`
- `response`

Suppose that this fragment is inserted inside a `response` context in the
input object:

```json
{
  "out": {
    "dump" : {
      "body" : true
    },
    "format" : {
      "body" : "json"
    }
  }
}
```

This means that in the corresponding output context, the `body` field
should be rendered and the output format should be rendered as `json`.

## Scripting

Every time a scenario runs, a brand new JavaScript context is spawned
into `V8` engine and it lasts for the whole scenario's life.

### Field's value

Generally speaking, all the fields in the input object can be scripted
defining JavaScript functions that are executed to obtain a value.

For example, the `query_string` attribute of a `request` could be defined
as this:

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
extension `.js` and placed into the directory where `chatterbox` reads
its inputs.

```javascript
function GetQueryString(p1, p2, p3) {
  if(p3){
    return "foo=default"
  }
  log(TLV.INF, "GetQueryString", "Invoked with: " + p1 + "," + p2);
  return "foo=" + p1 + p2;
}
```

### Context handlers

At every context activation, `chatterbox` calls, if defined, the corresponding
`on_begin` and `on_end` handlers.

Valid contexts, where this mechanism works, are:

- `scenario`
- `conversation`
- `request`
- `response`

Suppose that you want to define these handlers for the scenario context:

```json
{
  "on_begin": {
    "function": "OnScenarioBegin",
    "args": ["one", 2, "three"]
  },
  "on_end": {
    "function": "OnScenarioEnd",
    "args": [1, "two", 3]
  },
  "conversations" : []
}
```

You would also define the corresponding functions in the JavaScript:

```Javascript
function OnScenarioBegin(outCtxJson, p1, p2, p3) {
  log(TLV.INF, "begin", "parameter-1: " + p1);
  log(TLV.INF, "begin", "parameter-2: " + p2);
  log(TLV.INF, "begin", "parameter-3: " + p3);

  //set an optional tag in the contextual json passed as first argument
  outCtxJson.optionalTag = "my-custom-tag";
}

function OnScenarioEnd(outCtxJson, p1, p2, p3) {
  log(TLV.INF, "end", "parameter-1: " + p1);
  log(TLV.INF, "end", "parameter-2: " + p2);
  log(TLV.INF, "end", "parameter-3: " + p3);

  //read the optional tag we set before
  log(TLV.INF, "end", "optionalTag: " + outCtxJson.optionalTag);
}
```

Note that these handlers are called by `chatterbox` with the first argument:
`outCtxJson` always set to the contextual json node of the output object.

### Global objects

In the JavaScript environment you can access and manipulate a series of
objects automatically set by `chatterbox`.

Currently, you can access and manipulate the following objects:

- `outJson`.

This object represents the current state of the output object. You can access
any of its properties and also modify or add entries.

#### Example

Suppose that during the execution of your scenario the control reaches
a JavaScript function: `someFunction`; inside that, you could:

```javascript
function someFunction() {
  outJson.someTag1 = "myTag1";
  outJson.someTag2 = 2;

  log(TLV.INF, "someFunction", "outJson.someTag1: " + outJson.someTag1);
  log(TLV.INF, "someFunction", "outJson.someTag2: " + outJson.someTag2);
}
```

This would have the effect to produce an output object enriched with:

```javascript
{
  "conversations" : [
    {}
  ],
  "someTag1" : "myTag1",
  "someTag2" : 2
}
```

### Global functions

In the JavaScript environment you can invoke a series of
functions automatically set by `chatterbox`.

Currently, you can invoke the following functions:

- `log`

```javascript
let TLV = {
  TRC: 0,
  DBG: 1,
  INF: 2,
  WRN: 3,
  ERR: 4,
  CRI: 5,
  OFF: 6,
}

function someFunction() {
  log(TLV.INF, "someFunction", "lorem ipsum");
}
```

- `load`

```javascript
load("include.js")
```

- `assert`

```javascript
function someFunction(outCtxJson) {
  assert("someFunction [code]", outCtxJson.code == 200);
}
```
