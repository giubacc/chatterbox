#include "chatterbox.h"
#include "clipp.h"

int main(int argc, char *argv[])
{
  int res = 0;

  {
    rest::chatterbox cbox;

    auto cli = (
                 clipp::option("-i", "--input")
                 .doc("specify input scenario [filename]")
                 & clipp::value("input", cbox.cfg_.in_scenario_name),

                 clipp::option("-p", "--path")
                 .doc("specify scenario's path [path]")
                 & clipp::value("path", cbox.cfg_.in_scenario_path),

                 clipp::option("-o", "--output")
                 .doc("specify output channel [stdout, stderr, filename]")
                 & clipp::value("output", cbox.cfg_.out_channel),

                 clipp::option("-m", "--monitor")
                 .set(cbox.cfg_.monitor, true)
                 .doc("monitor filesystem for new scenarios"),

                 clipp::option("-d", "--delete")
                 .set(cbox.cfg_.move_scenario, true)
                 .doc("delete scenario files once consumed"),

                 clipp::option("-l", "--log")
                 .doc("specify event log output channel [stderr, stdout, filename]")
                 & clipp::value("event log output", cbox.cfg_.evt_log_channel),

                 clipp::option("-v", "--verbosity")
                 .doc("specify event log verbosity [off, dbg, trc, inf, wrn, err]")
                 & clipp::value("event log verbosity", cbox.cfg_.evt_log_level)
               );

    if(!clipp::parse(argc, argv, cli)) {
      std::cout << clipp::make_man_page(cli, argv[0]);
      return 1;
    }

    //init V8
    if(!js::js_env::init_V8(argc, argv)) {
      std::cerr << "V8 javascript engine failed to init, exiting..." << std::endl;
      return -1;
    }

    //init chatterbox
    if((res = cbox.init(argc, argv))) {
      std::cerr << "error init chatterbox, exiting..." << std::endl;
      return res;
    }

    if(cbox.cfg_.monitor) {
      cbox.event_log_->set_pattern(RAW_EVT_LOG_PATTERN);
      cbox.event_log_->info("{}", fmt::format(fmt::fg(fmt::terminal_color::magenta) |
                                              fmt::emphasis::bold,
                                              ">>MONITORING MODE<<\n"));
      cbox.event_log_->set_pattern(cbox.event_log_fmt_);
      while(true) {
        cbox.poll();
        usleep(2*1000*1000);
      }
    } else if(!cbox.cfg_.in_scenario_name.empty()) {
      std::string file_name;
      if(cbox.cfg_.in_scenario_path.empty()) {
        utils::base_name(cbox.cfg_.in_scenario_name,
                         cbox.cfg_.in_scenario_path,
                         file_name);
      } else {
        file_name = cbox.cfg_.in_scenario_name;
      }
      res = cbox.execute_scenario(file_name.c_str());
    }
  }

  js::js_env::stop_V8();
  return res;
}
