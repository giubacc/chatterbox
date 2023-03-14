let TLV = {
  TRC: 0,
  DBG: 1,
  INF: 2,
  WRN: 3,
  ERR: 4,
  CRI: 5,
  OFF: 6,
}

function onScenarioWill(outCtx, p1, p2, p3) {
  log(TLV.INF, "-onScenarioWill-", "customTag from output-context-json parameter: " + outCtx.customTag);
  log(TLV.INF, "-onScenarioWill-", "customTag from output-global-json object: " + out.customTag);
  log(TLV.INF, "-onScenarioWill-", "parameter-1: " + p1);
  log(TLV.INF, "-onScenarioWill-", "parameter-2: " + p2);
  log(TLV.INF, "-onScenarioWill-", "parameter-3: " + p3);

  //set an optional execution tag in the output json at the scenario level
  out.executionTag = "my-custom-tag";
}

function onScenarioDid(out, p1, p2, p3) {
  log(TLV.INF, "-onScenarioDid-", "parameter-1: " + p1);
  log(TLV.INF, "-onScenarioDid-", "parameter-2: " + p2);
  log(TLV.INF, "-onScenarioDid-", "parameter-3: " + p3);

  //read the optional execution tag we set before
  log(TLV.INF, "-onScenarioDid-", "executionTag: " + out.executionTag);
}

function onGETUserInfoResponse_AssertNotFound(outCtx, tag) {
  log(TLV.INF, "tag:", tag);
  assert("-onGETUserInfoResponse_AssertNotFound- [code]", outCtx.code == 404);
  assert("-onGETUserInfoResponse_AssertNotFound- [outCtx.body.status]", outCtx.body.status == "notFound");
  assert("-onGETUserInfoResponse_AssertNotFound- [outCtx.body.user]", outCtx.body.user == "julius");
}

function onPUTUserResponse_AssertCreated(outCtx, tag) {
  log(TLV.INF, "tag:", tag);
  assert("-onPUTUserResponse_AssertCreated- [code]", outCtx.code == 200);
  assert("-onPUTUserResponse_AssertCreated- [outCtx.body.status]", outCtx.body.status == "created");
  assert("-onPUTUserResponse_AssertCreated- [outCtx.body.user]", outCtx.body.user == "julius");
}

function onGETUserInfoResponse_AssertFound(outCtx, tag) {
  log(TLV.INF, "tag:", tag);
  assert("-onGETUserInfoResponse_AssertFound- [code]", outCtx.code == 200);
  assert("-onGETUserInfoResponse_AssertFound- [outCtx.body.status]", outCtx.body.status == "found");
  assert("-onGETUserInfoResponse_AssertFound- [outCtx.body.user]", outCtx.body.user == "julius");
}
