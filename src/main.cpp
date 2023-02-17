#include "scenario.h"
#include "clipp.h"
#include "ryml.hpp"

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

                 clipp::option("-o", "--output")
                 .doc("specify output channel [stdout, stderr, filename]")
                 & clipp::value("output", scenario.cfg_.out_channel),

                 clipp::option("-m", "--monitor")
                 .set(scenario.cfg_.monitor, true)
                 .doc("monitor filesystem for new scenarios"),

                 clipp::option("-d", "--delete")
                 .set(scenario.cfg_.move_scenario, true)
                 .doc("delete scenario files once consumed"),

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

    if(scenario.cfg_.monitor) {
      scenario.event_log_->set_pattern(RAW_EVT_LOG_PATTERN);
      scenario.event_log_->info("{}", fmt::format(fmt::fg(fmt::terminal_color::magenta) |
                                                  fmt::emphasis::bold,
                                                  ">>MONITORING MODE<<\n"));
      scenario.event_log_->set_pattern(scenario.event_log_fmt_);
      while(true) {
        scenario.poll();
        usleep(2*1000*1000);
      }
    } else if(!scenario.cfg_.in_scenario_name.empty()) {
      res = scenario.process();
    }
  }

  js::js_env::stop_V8();
  return res;
}
