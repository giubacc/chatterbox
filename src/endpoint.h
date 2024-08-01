#pragma once
#include "scenario.h"

namespace rest {

struct endpoint {

  endpoint(cbox::env &env);
  ~endpoint();

  int init(std::shared_ptr<spdlog::logger> &event_log);
  int start();

  void setup_routes();

  void do_put_echo(const Pistache::Rest::Request &request,
                   Pistache::Http::ResponseWriter response);

  void do_put_document(const Pistache::Rest::Request &request,
                       Pistache::Http::ResponseWriter response);

  cbox::env &env_;

  Pistache::Port port_;
  Pistache::Address addr_;
  std::shared_ptr<Pistache::Http::Endpoint> http_endpoint_;
  Pistache::Rest::Router router_;

  std::shared_ptr<spdlog::logger> event_log_;
};

}
