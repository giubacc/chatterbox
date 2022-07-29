#include "chatterbox.h"
#include "clipp.h"

int main(int argc, char *argv[])
{
  rest::chatterbox pr;

  auto cli = (
               clipp::option("-p", "--poll")
               .set(pr.cfg_.poll, true)
               .doc("monitor working directory for new scenarios"),

               clipp::option("-m", "--move")
               .set(pr.cfg_.move_scenario, false)
               .doc("move scenario files once consumed"),

               clipp::option("-l", "--log")
               .doc("specify logging type [shell, filename]")
               & clipp::value("logging type", pr.cfg_.log_type),

               clipp::option("-v", "--verbosity")
               .doc("specify logging verbosity [off, dbg, trc, inf, wrn, err]")
               & clipp::value("logging verbosity", pr.cfg_.log_level)
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
  if((res = pr.init(argc, argv))) {
    std::cerr << "error init chatterbox, exiting..." << std::endl;
    return res;
  }

  if(pr.cfg_.poll) {
    pr.log_->set_pattern(RAW_LOG_PATTERN);
    pr.log_->info("{}", fmt::format(fmt::fg(fmt::terminal_color::magenta) |
                                    fmt::emphasis::bold,
                                    ">>POLLING MODE<<\n"));
    pr.log_->set_pattern(pr.log_fmt_);
    while(true) {
      pr.poll();
      usleep(2*1000*1000);
    }
  }

  //stop V8
  js::js_env::stop_V8();
  return 0;
}
