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

int conversation::reset(ryml::NodeRef conversation_in)
{
  // reset stats
  stats_.reset();

  auto raw_host = js_env_.eval_as<std::string>(conversation_in, key_host);
  if(!raw_host) {
    event_log_->error("failed to read 'host'");
    return 1;
  }

  raw_host_ = *raw_host;
  auto host = raw_host_;
  utils::find_and_replace(host, "http://", "");
  utils::find_and_replace(host, "https://", "");
  host = host.substr(0, (host.find(':') == std::string::npos ? host.length() : host.find(':')));

  //authorization
  if(conversation_in.has_child(key_auth)) {
    ryml::NodeRef auth_node = conversation_in[key_auth];
    auto service = js_env_.eval_as<std::string>(conversation_in, key_service, "s3");
    auto access_key = js_env_.eval_as<std::string>(auth_node, key_access_key, "");
    auto secret_key = js_env_.eval_as<std::string>(auth_node, key_secret_key, "");
    auto signed_headers = js_env_.eval_as<std::string>(auth_node,
                                                       key_signed_headers,
                                                       "host;x-amz-content-sha256;x-amz-date");
    auto region = js_env_.eval_as<std::string>(auth_node, key_region, "US");
    auth_.reset(host, *access_key, *secret_key, *service, *signed_headers, *region);
  }

  return 0;
}

int conversation::process(ryml::NodeRef conversation_in,
                          ryml::NodeRef conversation_out)
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

      if(!conversation_in.has_child(key_requests)) {
        return 0;
      }
      ryml::NodeRef requests_in = conversation_in[key_requests];
      if(!requests_in.is_seq()) {
        event_log_->error("'requests' is not a sequence");
        return 1;
      }

      ryml::NodeRef requests_out = conversation_out[key_requests];
      requests_out |= ryml::SEQ;
      requests_out.clear_children();

      //requests cycle
      for(ryml::NodeRef const &request_in : requests_in.children()) {

        /*TODO parallel handling*/
        request req(*this);

        if((res = req.process(raw_host_,
                              request_in,
                              requests_out))) {
          return res;
        }
      }

      enrich_with_stats(conversation_out);
      scope.commit();
    } else {
      conversation_out.clear_children();
      conversation_out[key_enabled] << STR_FALSE;
    }
  }
  if(error) {
    return 1;
  }

  return res;
}

void conversation::enrich_with_stats(ryml::NodeRef conversation_out)
{
  ryml::NodeRef statistics = conversation_out[key_stats];
  statistics |= ryml::MAP;
  statistics[key_requests] << stats_.request_count_;
  ryml::NodeRef res_code_categorization = statistics[key_categorization];
  res_code_categorization |= ryml::MAP;
  std::for_each(stats_.categorization_.begin(), stats_.categorization_.end(), [&](const auto &it) {
    ryml::csubstr code = res_code_categorization.to_arena(it.first);
    res_code_categorization[code] << it.second;
  });
}

}
