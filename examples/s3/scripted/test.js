load("test_include.js")

function Initialize() {
  log("trc", "test", "hello world! " + DeclareVersion() + "\n");
}

Initialize();

function GetResDump(one, two, three) {
  log("trc", "GetResDump", one + "-" +  two + "-" + three + "\n");
  return true;
}

function GetVerb(){
  return "GET";
}

function GetFor(){
  return 2;
}
