# chatterbox

![License](https://img.shields.io/github/license/giubacc/chatterbox)
![Lint](https://github.com/giubacc/chatterbox/actions/workflows/lint.yaml/badge.svg)
![Builder Image](https://github.com/giubacc/chatterbox/actions/workflows/build-builder.yaml/badge.svg)
![Build](https://github.com/giubacc/chatterbox/actions/workflows/build-chatterbox.yaml/badge.svg)
<h1 align="left"><img alt="chatterbox-logo" src="./assets/images/cbox-logo.png"/>
</h1>

chatterbox is a tool for quickly write a conversation against a REST endpoint.

You write conversations using a `yaml` formalism describing an arbitrary
complex interaction between chatterbox and the endpoint.

chatterbox keeps a state during the execution of a conversation so that
you can use the collected responses and use those to feed the
subsequent inputs.

chatterbox could be useful to you when you have to:

- Prototype RESTful protocols.
- Quickly build a complex interaction with an endpoint.
- Debug/test your endpoint when APIs are not available.

chatterbox embeds [V8](https://v8.dev/) the Google’s open source
high-performance JavaScript engine.

The V8 engine is used to allow an input to be optionally evaluated
as the result of a user defined JavaScript function.
Moreover, the V8 engine allows to maintain a JavaScript context
during the conversation; this can be used to store an arbitrary state.

## quick example

The following example describes a conversation against an **S3** service
running on `localhost:7480`.
The interaction uses 3 distinct requests to realize an **object multipart
upload** using the S3 APIs:

- [CreateMultipartUpload](https://docs.aws.amazon.com/AmazonS3/latest/API/API_CreateMultipartUpload.html)
- [UploadPart](https://docs.aws.amazon.com/AmazonS3/latest/API/API_UploadPart.html)
- [CompleteMultipartUpload](https://docs.aws.amazon.com/AmazonS3/latest/API/API_CompleteMultipartUpload.html)

Every request is authenticated using the
[AWS Signature Version 4](https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_aws-signing.html).

```yaml
conversations:
- host: localhost:7480
  auth:
    accessKey: test
    secretKey: test
  requests:
    - method: POST
      enabled: true
      uri: my-bucket/my-object.mp
      queryString: uploads
      auth: aws_v4
    - method: PUT
      enabled: true
      uri: my-bucket/my-object.mp
      queryString: partNumber=1&uploadId={{.[0][0].response.body.UploadId}}
      data: "some-test-data-1"
      auth: aws_v4
    - method: POST
      enabled: true
      uri: my-bucket/my-object.mp
      queryString: format=json&uploadId={{.[0][0].response.body.UploadId}}
      auth: aws_v4
      data: |
        <?xml version="1.0" encoding="UTF-8"?>
        <CompleteMultipartUpload xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
          <Part>
              <ETag>{{.[0][1].response.headers.ETag}}</ETag>
              <PartNumber>1</PartNumber>
          </Part>
        </CompleteMultipartUpload>
```

As you can see, the second request is making use of the `UploadId` property
obtained with the response's body from the first request:

```yaml
queryString: partNumber=1&uploadId={{.[0][0].response.body.UploadId}}
```

The syntax:

```yaml
{{.[0][0].}}
```

is just a shorthand of:

```yaml
{{.conversations[0].requests[0].}}
```

Any property added in the output `yaml` during the conversation can be
referenced in subsequent properties using the `{{ .property }}` syntax.

## documentation

- [Build instructions](docs/build.md)
- [Usage instructions](docs/usage.md)
