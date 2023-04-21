#include "endpoint.h"

namespace rest {

endpoint::endpoint(cbox::env &env):
  env_(env),
  port_(env_.cfg_.endpoint_port),
  addr_(Pistache::Ipv4::any(), port_),
  http_endpoint_(std::make_shared<Pistache::Http::Endpoint>(addr_))
{}

endpoint::~endpoint()
{}

int endpoint::init(std::shared_ptr<spdlog::logger> &event_log)
{
  event_log_ = event_log;

  auto opts = Pistache::Http::Endpoint::options().threads(static_cast<int>(env_.cfg_.endpoint_concurrency));
  http_endpoint_->init(opts);
  setup_routes();
  return 0;
}

int endpoint::start()
{
  http_endpoint_->setHandler(router_.handler());
  http_endpoint_->serve();
  return 0;
}

void endpoint::setup_routes()
{
  using namespace Pistache::Rest;
  Routes::Put(router_, "/echo", Routes::bind(&endpoint::do_put_echo, this));
  Routes::Put(router_, "/document", Routes::bind(&endpoint::do_put_document, this));
}

void endpoint::do_put_echo(const Pistache::Rest::Request &request,
                           Pistache::Http::ResponseWriter response)
{
  if(request.body().empty()) {
    response.send(Pistache::Http::Code::Bad_Request, "empty document");
    return;
  }
  response.send(Pistache::Http::Code::Ok, request.body());
}

void endpoint::do_put_document(const Pistache::Rest::Request &request,
                               Pistache::Http::ResponseWriter response)
{
  int res = 0;

  if(request.body().empty()) {
    response.send(Pistache::Http::Code::Bad_Request, "empty document");
    return;
  }

  cbox::context ctx(env_.cfg_);
  if((res = ctx.init(event_log_))) {
    return;
  }
  if((res = ctx.load_document_by_string(request.body()))) {
    response.send(Pistache::Http::Code::Bad_Request, "malformed document");
    return;
  }
  while(!(res = ctx.process_scenario()));

  //a negative value means no more scenarios
  if(res<0) {
  }
}

}
