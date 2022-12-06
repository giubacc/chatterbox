load("test_include.js")

function Initialize() {
  log(TLV.INF, "Initialize", "ver:" + DeclareVersion());
}

Initialize();

function GetResDump(one, two, three) {
  log(TLV.INF, "GetResDump", one + "-" + two + "-" + three);
  return true;
}

function GetMethod() {
  return "GET";
}

function GetFor() {
  return 1;
}

function OnScenarioBegin() {
  log(TLV.INF, "OnScenarioBegin", "<-");
}

function OnScenarioEnd() {
  log(TLV.INF, "OnScenarioEnd", "->");
}

function OnConversationBegin() {
  log(TLV.INF, "OnConversationBegin", "<-");
}

function OnConversationEnd() {
  log(TLV.INF, "OnConversationEnd", "->");
}

function OnRequestBegin() {
  log(TLV.INF, "OnRequestBegin", "<-");
}

function OnRequestEnd() {
  log(TLV.INF, "OnRequestEnd", "->");
}
