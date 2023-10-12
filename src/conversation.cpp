#include "request.h"

#define ERR_FAIL_RESET_CONV   "failed to reset conversation"
#define ERR_FAIL_READ_HOST    "failed to read 'host'"
#define ERR_REQ_NOT_SEQ       "'requests' is not a sequence"

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
  scen_out_p_resolv_(parent_.scen_out_p_resolv_),
  scen_p_evaluator_(parent_.scen_p_evaluator_),
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
  return 0;
}

int conversation::process(ryml::NodeRef conversation_in,
                          ryml::NodeRef conversation_out)
{
  int res = 0;

  if((res = reset(conversation_in))) {
    event_log_->error(ERR_FAIL_RESET_CONV);
    return res;
  }

  bool error = false;
  {
    scenario::stack_scope scope(parent_,
                                conversation_in,
                                conversation_out,
                                error,
                                utils::get_default_conversation_out_options());
    if(error) {
      res = 1;
      goto fun_end;
    }

    if(scope.enabled_) {
      parent_.stats_.incr_conversation_count();
      bool further_eval = false;
      auto raw_host = js_env_.eval_as<std::string>(conversation_out,
                                                   key_host,
                                                   std::nullopt,
                                                   true,
                                                   nullptr,
                                                   PROP_EVAL_RGX,
                                                   &further_eval);
      if(!raw_host) {
        if(further_eval) {
          raw_host = scen_p_evaluator_.eval_as<std::string>(conversation_out,
                                                            key_host,
                                                            scen_out_p_resolv_);
        }
        if(!raw_host) {
          event_log_->error(ERR_FAIL_READ_HOST);
          utils::clear_map_node_put_key_val(conversation_out, key_error, ERR_FAIL_READ_HOST);
          return 1;
        }
      }

      conversation_out.remove_child(key_host);
      conversation_out[key_host] << *raw_host;

      raw_host_ = *raw_host;
      auto host = raw_host_;
      utils::find_and_replace(host, "http://", "");
      utils::find_and_replace(host, "https://", "");
      host = host.substr(0, (host.find(':') == std::string::npos ? host.length() : host.find(':')));

      //auth
      further_eval = false;
      if(conversation_out.has_child(key_auth)) {
        ryml::NodeRef auth_node = conversation_out[key_auth];
        auto service = js_env_.eval_as<std::string>(auth_node,
                                                    key_service,
                                                    "s3",
                                                    true,
                                                    nullptr,
                                                    PROP_EVAL_RGX,
                                                    &further_eval);

        if(further_eval) {
          service = scen_p_evaluator_.eval_as<std::string>(conversation_out,
                                                           key_service,
                                                           scen_out_p_resolv_);
          if(!service) {
            event_log_->error(ERR_FAIL_EVAL);
            utils::clear_map_node_put_key_val(conversation_out, key_error, ERR_FAIL_EVAL);
            return 1;
          }
        }
        if(service) {
          if(auth_node.has_child(key_service)) {
            auth_node.remove_child(key_service);
          }
          auth_node[key_service] << *service;
        }

        //access_key
        further_eval = false;
        auto access_key = js_env_.eval_as<std::string>(auth_node,
                                                       key_access_key,
                                                       std::nullopt,
                                                       true,
                                                       nullptr,
                                                       PROP_EVAL_RGX,
                                                       &further_eval);
        if(further_eval) {
          access_key = scen_p_evaluator_.eval_as<std::string>(conversation_out,
                                                              key_access_key,
                                                              scen_out_p_resolv_);
          if(!access_key) {
            event_log_->error(ERR_FAIL_EVAL);
            utils::clear_map_node_put_key_val(conversation_out, key_error, ERR_FAIL_EVAL);
            return 1;
          }
        }
        if(access_key) {
          auth_node.remove_child(key_access_key);
          auth_node[key_access_key] << *access_key;
        }

        //secret_key
        further_eval = false;
        auto secret_key = js_env_.eval_as<std::string>(auth_node,
                                                       key_secret_key,
                                                       std::nullopt,
                                                       true,
                                                       nullptr,
                                                       PROP_EVAL_RGX,
                                                       &further_eval);
        if(further_eval) {
          secret_key = scen_p_evaluator_.eval_as<std::string>(conversation_out,
                                                              key_secret_key,
                                                              scen_out_p_resolv_);
          if(!secret_key) {
            event_log_->error(ERR_FAIL_EVAL);
            utils::clear_map_node_put_key_val(conversation_out, key_error, ERR_FAIL_EVAL);
            return 1;
          }
        }
        if(secret_key) {
          auth_node.remove_child(key_secret_key);
          auth_node[key_secret_key] << *secret_key;
        }

        //signed_headers
        further_eval = false;
        auto signed_headers = js_env_.eval_as<std::string>(auth_node,
                                                           key_signed_headers,
                                                           AUTH_AWS_DEF_SIGN_HDRS,
                                                           true,
                                                           nullptr,
                                                           PROP_EVAL_RGX,
                                                           &further_eval);
        if(further_eval) {
          signed_headers = scen_p_evaluator_.eval_as<std::string>(conversation_out,
                                                                  key_signed_headers,
                                                                  scen_out_p_resolv_);
          if(!signed_headers) {
            event_log_->error(ERR_FAIL_EVAL);
            utils::clear_map_node_put_key_val(conversation_out, key_error, ERR_FAIL_EVAL);
            return 1;
          }
        }
        if(signed_headers) {
          if(auth_node.has_child(key_signed_headers)) {
            auth_node.remove_child(key_signed_headers);
          }
          auth_node[key_signed_headers] << *signed_headers;
        }

        //region
        auto region = js_env_.eval_as<std::string>(auth_node,
                                                   key_region,
                                                   "US",
                                                   true,
                                                   nullptr,
                                                   PROP_EVAL_RGX,
                                                   &further_eval);

        if(further_eval) {
          region = scen_p_evaluator_.eval_as<std::string>(conversation_out,
                                                          key_region,
                                                          scen_out_p_resolv_);
          if(!region) {
            event_log_->error(ERR_FAIL_EVAL);
            utils::clear_map_node_put_key_val(conversation_out, key_error, ERR_FAIL_EVAL);
            return 1;
          }
        }
        if(region) {
          if(auth_node.has_child(key_region)) {
            auth_node.remove_child(key_region);
          }
          auth_node[key_region] << *region;
        }

        auth_.reset(host,
                    *access_key,
                    *secret_key,
                    *service,
                    *signed_headers,
                    *region);
      }

      if(conversation_in.has_child(key_requests)) {
        ryml::NodeRef requests_in = conversation_in[key_requests];
        if(!requests_in.is_seq()) {
          res = 1;
          event_log_->error(ERR_REQ_NOT_SEQ);
          utils::clear_map_node_put_key_val(conversation_out, key_error, ERR_REQ_NOT_SEQ);
          goto fun_end;
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
            break;
          }
        }
      }
      if(!res) {
        enrich_with_stats(conversation_out);
        scope.commit();
      }
    } else {
      utils::clear_map_node_put_key_val(conversation_out, key_enabled, STR_FALSE);
    }
  }
  if(error) {
    return 1;
  }

fun_end:
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
