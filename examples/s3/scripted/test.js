load("test_include.js")

function Initialize() {
  log(TLV.INF, "Initialize", "ver:" + DeclareVersion());
}

Initialize();

function GetResDump(a1, a2, a3, a4, a5) {
  log(TLV.INF, "GetResDump", a1 + "-" + a2 + "-" + a3 + "-" + a4 + "-" + a5);
  return true;
}

function OnScenarioBegin() {
  log(TLV.INF, "OnScenarioBegin", "<-");
  log(TLV.INF, "---", "inScenario.test_bool: " + inScenario.test_bool);
  log(TLV.INF, "---", "inScenario.test_int_32: " + inScenario.test_int_32);
  log(TLV.INF, "---", "inScenario.test_uint_32: " + inScenario.test_uint_32);
  log(TLV.INF, "---", "inScenario.test_double: " + inScenario.test_double);
  log(TLV.INF, "---", "inScenario.test_string: " + inScenario.test_string);
  log(TLV.INF, "---", "inScenario.test_array[2]: " + inScenario.test_array[2]);
  log(TLV.INF, "---", "inScenario.test_array[4]: " + inScenario.test_array[4]);
  log(TLV.INF, "---", "inScenario.conversations[0].host: " + inScenario.conversations[0].host);
}

function OnScenarioEnd() {
  log(TLV.INF, "OnScenarioEnd", "->");
  outScenario.test_bool = false;
  outScenario.test_int_32 = -42;
  outScenario.test_uint_32 = 42;
  outScenario.test_double = 42.32;
  outScenario.test_string = "Julius Caesar";
  outScenario.test_array = [1, 2, 3, "four", [-1.2, true]];
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

function OnResponseBegin() {
  log(TLV.INF, "OnResponseBegin", "<-");
}

function OnResponseEnd() {
  log(TLV.INF, "OnResponseEnd", "->");
}
