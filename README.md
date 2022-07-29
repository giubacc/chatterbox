# chatterbox

In nutshell, `chatterbox` is a tool to define conversations with a generic rest endpoint.  
You can write json files describing scenarios for conversations between the client (chatterbox) and a restful server.  
In polling mode, you can drop the files into `chatterbox` working directory to see them being consumed by the tool.  
Every time a new json file is placed within the working directory, `chatterbox` parses it
and then generates the conversation(s) against the rest endpoint(s).  
Once the file has been processed, the file is deleted or optionally moved into `./consumed` directory.

## How to build

In order to build `chatterbox` alongside with all its dependencies, use the provided script:

- `scripts/build.sh`

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

## conversation files

S3 conversation scenario example:

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
          "for" : 1,
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
