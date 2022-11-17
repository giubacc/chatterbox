#include "chatterbox.h"
#include "clipp.h"

int main(int argc, char *argv[])
{
  {
    rest::chatterbox cbox;

    auto cli = (
                 clipp::option("-o", "--out")
                 .doc("specify the output channel [stdout, stderr, filename]")
                 & clipp::value("output", cbox.cfg_.out_channel),

                 clipp::option("-p", "--poll")
                 .set(cbox.cfg_.poll, true)
                 .doc("monitor filesystem for new scenarios")
                 & clipp::value("path", cbox.cfg_.source_path),

                 clipp::option("-m", "--move")
                 .set(cbox.cfg_.move_scenario, false)
                 .doc("move scenario files once consumed"),

                 clipp::option("-l", "--log")
                 .doc("specify the event log output channel [stderr, stdout, filename]")
                 & clipp::value("event log output", cbox.cfg_.evt_log_channel),

                 clipp::option("-v", "--verbosity")
                 .doc("specify the event log verbosity [off, dbg, trc, inf, wrn, err]")
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
    int res = 0;
    if((res = cbox.init(argc, argv))) {
      std::cerr << "error init chatterbox, exiting..." << std::endl;
      return res;
    }

    if(cbox.cfg_.poll) {
      cbox.event_log_->set_pattern(RAW_EVT_LOG_PATTERN);
      cbox.event_log_->info("{}", fmt::format(fmt::fg(fmt::terminal_color::magenta) |
                                              fmt::emphasis::bold,
                                              ">>POLLING MODE<<\n"));
      cbox.event_log_->set_pattern(cbox.event_log_fmt_);
      while(true) {
        cbox.poll();
        usleep(2*1000*1000);
      }
    }
  }

  js::js_env::stop_V8();
  return 0;
}
