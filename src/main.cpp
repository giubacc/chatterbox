#include "scenario.h"
#include "clipp.h"

int main(int argc, char *argv[])
{
  int res = 0;

  {
    cbox::scenario scenario;

    auto cli = (
                 clipp::option("-n", "--noout")
                 .set(scenario.cfg_.no_out_, true)
                 .doc("no output"),

                 clipp::option("-f", "--filename")
                 .doc("specify scenario [filename]")
                 & clipp::value("filename", scenario.cfg_.in_scenario_name),

                 clipp::option("-p", "--path")
                 .doc("specify scenario's path [path]")
                 & clipp::value("path", scenario.cfg_.in_scenario_path),

                 clipp::option("-o", "--output-format")
                 .doc("specify output format [yaml, json]")
                 & clipp::value("output format", scenario.cfg_.out_format),

                 clipp::option("-oc", "--output-channel")
                 .doc("specify output channel [stdout, stderr, filename]")
                 & clipp::value("output channel", scenario.cfg_.out_channel),

                 clipp::option("-l", "--log")
                 .doc("specify event log output channel [stderr, stdout, filename]")
                 & clipp::value("event log output", scenario.cfg_.evt_log_channel),

                 clipp::option("-v", "--verbosity")
                 .doc("specify event log verbosity [trc, dbg, inf, wrn, err, cri, off]")
                 & clipp::value("event log verbosity", scenario.cfg_.evt_log_level)
               );

    if(!clipp::parse(argc, argv, cli)) {
      std::cout << clipp::make_man_page(cli, argv[0]);
      return 1;
    }

    //init V8
    if(!js::js_env::init_V8(argc, (const char **)argv)) {
      std::cerr << "V8 javascript engine failed to init, exiting..." << std::endl;
      return 1;
    }

    //init chatterbox
    if((res = scenario.init(argc, (const char **)argv))) {
      std::cerr << "error init chatterbox, exiting..." << std::endl;
      return res;
    }

    res = scenario.load_source_process();
  }

  js::js_env::stop_V8();
  return res;
}
