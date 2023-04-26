let TLV = {
  TRC: 0,
  DBG: 1,
  INF: 2,
  WRN: 3,
  ERR: 4,
  CRI: 5,
  OFF: 6,
}

function onScenarioBefore(outCtx) {
  log(TLV.INF, "-onScenarioBefore-", "PING");
}

function onScenarioAfter(outCtx) {
  log(TLV.INF, "-onScenarioAfter-", "PING");
}
