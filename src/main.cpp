#include "cbox.h"
#include "clipp.h"

int main(int argc, char *argv[])
{
  int res = 0;

  {
    cbox::env env;

    auto cli = (
                 clipp::option("-n", "--noout")
                 .set(env.cfg_.no_out_, true)
                 .doc("no output"),

                 clipp::option("-f", "--filename")
                 .doc("specify input filename")
                 & clipp::value("input filename", env.cfg_.in_name),

                 clipp::option("-p", "--path")
                 .doc("specify input path [path]")
                 & clipp::value("path", env.cfg_.in_path),

                 clipp::option("-o", "--output-format")
                 .doc("specify output format [yaml, json]")
                 & clipp::value("output format", env.cfg_.out_format),

                 clipp::option("-oc", "--output-channel")
                 .doc("specify output channel [stdout, stderr, filename]")
                 & clipp::value("output channel", env.cfg_.out_channel),

                 clipp::option("-l", "--log")
                 .doc("specify event log output channel [stderr, stdout, filename]")
                 & clipp::value("event log output", env.cfg_.evt_log_channel),

                 clipp::option("-v", "--verbosity")
                 .doc("specify event log verbosity [t, d, i, w, e, c, o]")
                 & clipp::value("event log verbosity", env.cfg_.evt_log_level),

                 clipp::option("-d", "--daemon")
                 .set(env.cfg_.daemon, true)
                 .doc("start daemon"),

                 clipp::option("--endpoint-port")
                 .doc("specify the endpoint port")
                 & clipp::value("endpoint port", env.cfg_.endpoint_port),

                 clipp::option("--endpoint-concurrency")
                 .doc("specify the endpoint concurrency")
                 & clipp::value("endpoint concurrency", env.cfg_.endpoint_concurrency)
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

    //init environment
    if((res = env.init())) {
      std::cerr << "error init environment, exiting..." << std::endl;
      return res;
    }

    res = env.exec();
  }

  js::js_env::stop_V8();
  return res;
}
