let TLV = {
  TRC: 0,
  DBG: 1,
  INF: 2,
  WRN: 3,
  ERR: 4,
  CRI: 5,
  OFF: 6,
}

function OnScenarioBegin(outCtxJson, p1, p2, p3) {
  log(TLV.INF, "-OnScenarioBegin-", "customTag from output-context-json parameter: " + outCtxJson.customTag);
  log(TLV.INF, "-OnScenarioBegin-", "customTag from output-global-json object: " + outJson.customTag);
  log(TLV.INF, "-OnScenarioBegin-", "parameter-1: " + p1);
  log(TLV.INF, "-OnScenarioBegin-", "parameter-2: " + p2);
  log(TLV.INF, "-OnScenarioBegin-", "parameter-3: " + p3);

  //set an optional execution tag in the output json at the scenario level
  outJson.executionTag = "my-custom-tag";
}

function OnScenarioEnd(outCtxJson, p1, p2, p3) {
  log(TLV.INF, "-OnScenarioEnd-", "parameter-1: " + p1);
  log(TLV.INF, "-OnScenarioEnd-", "parameter-2: " + p2);
  log(TLV.INF, "-OnScenarioEnd-", "parameter-3: " + p3);

  //read the optional execution tag we set before
  log(TLV.INF, "-OnScenarioEnd-", "executionTag: " + outJson.executionTag);
}

function OnGETUserInfoResponse_AssertNotFound(outCtxJson, tag) {
  log(TLV.INF, "tag:", tag);
  assert("-OnGETUserInfoResponse_AssertNotFound- [code]", outCtxJson.code == 404);
  assert("-OnGETUserInfoResponse_AssertNotFound- [outCtxJson.body.status]", outCtxJson.body.status == "notFound");
  assert("-OnGETUserInfoResponse_AssertNotFound- [outCtxJson.body.user]", outCtxJson.body.user == "julius");
}

function OnPUTUserResponse_AssertCreated(outCtxJson, tag) {
  log(TLV.INF, "tag:", tag);
  assert("-OnPUTUserResponse_AssertCreated- [code]", outCtxJson.code == 200);
  assert("-OnPUTUserResponse_AssertCreated- [outCtxJson.body.status]", outCtxJson.body.status == "created");
  assert("-OnPUTUserResponse_AssertCreated- [outCtxJson.body.user]", outCtxJson.body.user == "julius");
}

function OnGETUserInfoResponse_AssertFound(outCtxJson, tag) {
  log(TLV.INF, "tag:", tag);
  assert("-OnGETUserInfoResponse_AssertFound- [code]", outCtxJson.code == 200);
  assert("-OnGETUserInfoResponse_AssertFound- [outCtxJson.body.status]", outCtxJson.body.status == "found");
  assert("-OnGETUserInfoResponse_AssertFound- [outCtxJson.body.user]", outCtxJson.body.user == "julius");
}
