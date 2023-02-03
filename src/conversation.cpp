#include "scenario.h"
#include "request.h"

namespace cbox {

// -------------
// --- STATS ---
// -------------

void conversation::statistics::reset()
{
  request_count_ = 0;
  categorization_.clear();
}

void conversation::statistics::incr_request_count()
{
  ++request_count_;
}

void conversation::statistics::incr_categorization(const std::string &key)
{
  auto value = categorization_[key];
  categorization_[key] = ++value;
}

// --------------------
// --- CONVERSATION ---
// --------------------

conversation::conversation(scenario &parent) :
  parent_(parent),
  js_env_(parent.js_env_),
  stats_(*this),
  event_log_(parent.event_log_)
{
  auth_.init(event_log_);
}

int conversation::reset(const Json::Value &conversation_in)
{
  // reset stats
  stats_.reset();

  auto raw_host = js_env_.eval_as<std::string>(conversation_in, key_host.c_str());
  if(!raw_host) {
    event_log_->error("failed to read 'host' field");
    return 1;
  }

  raw_host_ = *raw_host;
  auto host = raw_host_;
  utils::find_and_replace(host, "http://", "");
  utils::find_and_replace(host, "https://", "");
  host = host.substr(0, (host.find(':') == std::string::npos ? host.length() : host.find(':')));

  //authorization
  const Json::Value *auth_node = nullptr;
  if((auth_node = conversation_in.find(key_auth.data(), key_auth.data()+key_auth.length()))) {
    auto service = js_env_.eval_as<std::string>(conversation_in, key_service.c_str(), "s3");
    auto access_key = js_env_.eval_as<std::string>(*auth_node, key_access_key.c_str(), "");
    auto secret_key = js_env_.eval_as<std::string>(*auth_node, key_secret_key.c_str(), "");
    auto signed_headers = js_env_.eval_as<std::string>(*auth_node,
                                                       key_signed_headers.c_str(),
                                                       "host;x-amz-content-sha256;x-amz-date");
    auto region = js_env_.eval_as<std::string>(*auth_node, key_region.c_str(), "US");
    auth_.reset(host, *access_key, *secret_key, *service, *signed_headers, *region);
  }

  return 0;
}

int conversation::process(Json::Value &conversation_in,
                          Json::Value &conversation_out)
{
  int res = 0;

  if((res = reset(conversation_in))) {
    event_log_->error("failed to reset conversation");
    return res;
  }

  bool error = false;
  {
    scenario::stack_scope scope(parent_,
                                conversation_in,
                                conversation_out,
                                error,
                                scenario::get_default_conversation_out_options());
    if(error) {
      return 1;
    }

    if(scope.enabled_) {
      parent_.stats_.incr_conversation_count();

      Json::Value &requests_in = conversation_in[key_requests];
      if(!requests_in.isArray()) {
        event_log_->error("requests field is not an array");
        return 1;
      }

      Json::Value &requests_out = conversation_out[key_requests];
      requests_out.clear();

      //requests cycle
      uint32_t req_it = 0;
      while(true) {
        if(!requests_in.isValidIndex(req_it)) {
          break;
        }

        /*TODO parallel handling*/
        request req(*this);

        if((res = req.process(raw_host_,
                              requests_in[req_it],
                              requests_out))) {
          return res;
        }
        ++req_it;
      }

      enrich_with_stats(conversation_out);
      scope.commit();
    } else {
      conversation_out.clear();
      conversation_out[key_enabled] = false;
    }
  }
  if(error) {
    return 1;
  }

  return res;
}

void conversation::enrich_with_stats(Json::Value &conversation_out)
{
  Json::Value &statistics = conversation_out[key_stats];
  statistics[key_requests] = stats_.request_count_;
  std::for_each(stats_.categorization_.begin(),
  stats_.categorization_.end(), [&](auto it) {
    Json::Value &res_code_categorization = statistics[key_categorization];
    res_code_categorization[it.first] = it.second;
  });
}

}
