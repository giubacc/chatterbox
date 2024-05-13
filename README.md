# chatterbox

![License](https://img.shields.io/github/license/giubacc/chatterbox)
![Lint](https://github.com/giubacc/chatterbox/actions/workflows/lint.yaml/badge.svg)
![Builder Image](https://github.com/giubacc/chatterbox/actions/workflows/build-builder.yaml/badge.svg)
![Build](https://github.com/giubacc/chatterbox/actions/workflows/build-chatterbox.yaml/badge.svg)
<h1 align="left"><img alt="chatterbox-logo" src="./assets/images/cbox-logo.png"/>
</h1>

chatterbox is a tool for chatting with a REST endpoint.

You write conversations using a `yaml` formalism describing an arbitrary
complex interaction between chatterbox and the endpoint.

chatterbox keeps a state during the execution of a conversation so that
you can use the collected responses and use those to feed the
subsequent inputs.

chatterbox could be useful to you when you have to:

- Prototype RESTful protocols.
- Quickly build a complex interaction with an endpoint.
- Debug/test your endpoint when APIs are not available.

chatterbox embeds a JavaScript interpreter: [V8](https://v8.dev/)
so the user can benefit from scripting capabilities.
A JavaScript context is kept during the entire conversation;
this allows to maintain an arbitrary state.

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
      id: req1
      enabled: true
      uri: my-bucket/my-object.mp
      queryString: uploads
      auth: aws_v4
    - method: PUT
      id: req2
      enabled: true
      uri: my-bucket/my-object.mp
      queryString: partNumber=1&uploadId={{req1.response.body.UploadId}}
      data: "some-test-data-1"
      auth: aws_v4
    - method: POST
      enabled: true
      uri: my-bucket/my-object.mp
      queryString: format=json&uploadId={{req1.response.body.UploadId}}
      auth: aws_v4
      data: |
        <?xml version="1.0" encoding="UTF-8"?>
        <CompleteMultipartUpload xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
          <Part>
              <ETag>{{req2.response.headers.ETag}}</ETag>
              <PartNumber>1</PartNumber>
          </Part>
        </CompleteMultipartUpload>
```

The second request is making use of the `UploadId` property
obtained from the response's body in the first request:

```yaml
queryString: partNumber=1&uploadId={{req1.response.body.UploadId}}
```

The syntax:

```script
{{req1.*}}
```

denotes a path starting from the node identified with `id: req1` in the output `yaml`.

Any property added in the output `yaml` during the conversation can be
referenced in any subsequent property using the `{{.id.*}}` syntax.

## documentation

- [Build instructions](docs/build.md)
- [Usage instructions](docs/usage.md)
