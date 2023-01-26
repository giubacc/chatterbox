#include "chatterbox.h"
#include <algorithm>

namespace rest {

const std::string key_access_key = "access_key";
const std::string key_auth = "auth";
const std::string key_body = "body";
const std::string key_categorization = "categorization";
const std::string key_code = "code";
const std::string key_conversations = "conversations";
const std::string key_data = "data";
const std::string key_dump = "dump";
const std::string key_enabled = "enabled";
const std::string key_for = "for";
const std::string key_format = "format";
const std::string key_header = "header";
const std::string key_host = "host";
const std::string key_method = "method";
const std::string key_mock = "mock";
const std::string key_msec = "msec";
const std::string key_nsec = "nsec";
const std::string key_on_begin = "on_begin";
const std::string key_on_end = "on_end";
const std::string key_out = "out";
const std::string key_query_string = "query_string";
const std::string key_region = "region";
const std::string key_requests = "requests";
const std::string key_response = "response";
const std::string key_rtt = "rtt";
const std::string key_sec = "sec";
const std::string key_secret_key = "secret_key";
const std::string key_service = "service";
const std::string key_signed_headers = "signed_headers";
const std::string key_stats = "stats";
const std::string key_uri = "uri";
const std::string key_usec = "usec";

// *INDENT-OFF*
chatterbox::stack_scope::stack_scope(chatterbox &cbox,
                                     Json::Value &obj_in,
                                     Json::Value &obj_out,
                                     bool &error,
                                     const Json::Value &default_out_options,
                                     bool call_dump_val_cb,
                                     const std::function<void(const std::string &key, bool val)> &dump_val_cb,
                                     bool call_format_val_cb,
                                     const std::function<void(const std::string &key, const std::string &val)> &format_val_cb)
// *INDENT-ON*
  :
  cbox_(cbox),
  error_(error),
  enabled_(false),
  commit_(false)
{
  cbox_.stack_obj_in_.push(utils::json_value_ref(obj_in));
  cbox_.stack_obj_out_.push(utils::json_value_ref(obj_out));

  if(!cbox_.push_out_opts(default_out_options,
                          call_dump_val_cb,
                          dump_val_cb,
                          call_format_val_cb,
                          format_val_cb)) {
    cbox_.event_log_->error("failed to push out-options for the current stack scope");
    error_ = true;
  } else {
    //set newly pushed stack objects inside the js context
    cbox_.js_env_.install_current_objects();
    //on_begin handler
    if(!cbox_.js_env_.exec_as_function(obj_in, key_on_begin.c_str())) {
      cbox_.event_log_->error("failed to execute on_begin handler for the current stack scope");
      error_ = true;
    }
  }

  if(cbox_.assert_failure_) {
    error_ = true;
    return;
  }

  auto enabled_opt = cbox_.js_env_.eval_as<bool>(obj_in, key_enabled.c_str(), true);
  if(!enabled_opt) {
    cbox_.event_log_->error("failed to read 'enabled' field");
    error_ = true;
  } else {
    enabled_ = *enabled_opt;
  }
}

chatterbox::stack_scope::~stack_scope()
{
  if(commit_) {
    //on_end handler
    if(!cbox_.js_env_.exec_as_function(cbox_.stack_obj_in_.top().ref_, key_on_end.c_str())) {
      cbox_.event_log_->error("failed to execute on_end handler for the current stack scope");
      error_ = true;
    } else {
      if(!cbox_.pop_process_out_opts()) {
        cbox_.event_log_->error("failed to pop-process out-options for the current stack scope");
        error_ = true;
      }
    }
  } else {
    cbox_.stack_out_options_.pop();
  }
  cbox_.stack_obj_in_.pop();
  cbox_.stack_obj_out_.pop();

  //restore previous stack's top objects inside the js context
  if(!cbox_.stack_obj_in_.empty()) {
    cbox_.js_env_.install_current_objects();
  }

  if(cbox_.assert_failure_) {
    error_ = true;
    return;
  }
}

static Json::Value default_out_options;
const Json::Value &chatterbox::get_default_out_options()
{
  if(default_out_options.empty()) {
    auto &default_out_dumps = default_out_options[key_dump];
    default_out_dumps[key_out] = false;
    default_out_dumps[key_on_begin] = false;
    default_out_dumps[key_on_end] = false;
    default_out_dumps[key_enabled] = false;
    auto &default_out_formats = default_out_options[key_format];
    default_out_formats[key_rtt] = key_msec;
  }
  return default_out_options;
}

static Json::Value default_scenario_out_options;
const Json::Value &chatterbox::get_default_scenario_out_options()
{
  if(default_scenario_out_options.empty()) {
    default_scenario_out_options = get_default_out_options();
  }
  return default_scenario_out_options;
}

static Json::Value default_conversation_out_options;
const Json::Value &chatterbox::get_default_conversation_out_options()
{
  if(default_conversation_out_options.empty()) {
    default_conversation_out_options = get_default_out_options();
    // auto &default_out_dumps = default_conversation_out_options[key_dump];
  }
  return default_conversation_out_options;
}

static Json::Value default_request_out_options;
const Json::Value &chatterbox::get_default_request_out_options()
{
  if(default_request_out_options.empty()) {
    default_request_out_options = get_default_out_options();
    auto &default_out_dumps = default_request_out_options[key_dump];
    default_out_dumps[key_auth] = false;
    default_out_dumps[key_for] = false;
    default_out_dumps[key_mock] = false;
  }
  return default_request_out_options;
}

static Json::Value default_response_out_options;
const Json::Value &chatterbox::get_default_response_out_options()
{
  if(default_response_out_options.empty()) {
    default_response_out_options = get_default_out_options();
    auto &response_out_formats = default_response_out_options[key_format];
    response_out_formats[key_body] = "json";
  }
  return default_response_out_options;
}

int chatterbox::reset_scenario()
{
  assert_failure_ = false;

  // reset stats
  scen_conversation_count_ = scen_request_count_ = 0;
  scen_categorization_.clear();

  int res = js_env_.renew_scenario_context();

  //initialize scenario-out
  scenario_out_ = scenario_in_;

  //clear stacks
  while(!stack_obj_in_.empty()) {
    stack_obj_in_.pop();
  }
  while(!stack_obj_out_.empty()) {
    stack_obj_out_.pop();
  }
  while(!stack_out_options_.empty()) {
    stack_out_options_.pop();
  }

  return res;
}

int chatterbox::reset_conversation(const Json::Value &conversation_in)
{
  // reset stats
  conv_request_count_ = 0;
  conv_categorization_.clear();

  auto raw_host = js_env_.eval_as<std::string>(conversation_in, key_host.c_str());
  if(!raw_host) {
    event_log_->error("failed to read 'host' field");
    return 1;
  }

  raw_host_ = *raw_host;
  host_ = raw_host_;
  utils::find_and_replace(host_, "http://", "");
  utils::find_and_replace(host_, "https://", "");
  host_ = host_.substr(0, (host_.find(':') == std::string::npos ? host_.length() : host_.find(':')));

  //authorization
  const Json::Value *auth_node = nullptr;
  if((auth_node = conversation_in.find(key_auth.data(), key_auth.data()+key_auth.length()))) {
    auto service = js_env_.eval_as<std::string>(conversation_in, key_service.c_str(), "s3");
    service_ = *service;

    auto access_key = js_env_.eval_as<std::string>(*auth_node, key_access_key.c_str(), "");
    access_key_ = *access_key;

    auto secret_key = js_env_.eval_as<std::string>(*auth_node, key_secret_key.c_str(), "");
    secret_key_ = *secret_key;

    auto signed_headers = js_env_.eval_as<std::string>(*auth_node,
                                                       key_signed_headers.c_str(),
                                                       "host;x-amz-content-sha256;x-amz-date");
    signed_headers_ = *signed_headers;

    auto region = js_env_.eval_as<std::string>(*auth_node, key_region.c_str(), "US");
    region_ = *region;
  }

  // connection reset
  conv_conn_.reset(new RestClient::Connection(raw_host_));
  if(!conv_conn_) {
    event_log_->error("failed creating resource_conn_ object");
    return 1;
  }
  conv_conn_->SetVerifyPeer(false);
  conv_conn_->SetVerifyHost(false);
  conv_conn_->SetTimeout(30);

  return 0;
}

bool chatterbox::push_out_opts(const Json::Value &default_out_options,
                               bool call_dump_val_cb,
                               const std::function<void(const std::string &key, bool val)> &dump_val_cb,
                               bool call_format_val_cb,
                               const std::function<void(const std::string &key, const std::string &val)> &format_val_cb)
{
  bool res = true;

  Json::Value *out_node_ptr = nullptr;
  if((out_node_ptr = const_cast<Json::Value *>(stack_obj_in_.top().ref_.find(key_out.data(), key_out.data()+key_out.length())))) {
    Json::Value out_options = default_out_options;
    stack_out_options_.push(utils::json_value_ref(*const_cast<Json::Value *>(out_node_ptr)));

    {
      Json::Value *dump_node = nullptr;
      if((dump_node = const_cast<Json::Value *>(out_node_ptr->find(key_dump.data(), key_dump.data()+key_dump.length())))) {
        Json::Value::Members dn_keys = dump_node->getMemberNames();
        std::for_each(dn_keys.begin(), dn_keys.end(), [&](const Json::String &it) {
          const Json::Value &val = (*dump_node)[it];
          if(!val.isBool()) {
            res = false;
          }
          auto &default_out_dumps = out_options[key_dump];
          if(default_out_dumps.find(it.data(), it.data()+it.length())) {
            default_out_dumps.removeMember(it);
          }
        });
        auto &default_out_dumps = out_options[key_dump];
        Json::Value::Members ddn_keys = default_out_dumps.getMemberNames();
        std::for_each(ddn_keys.begin(), ddn_keys.end(), [&](const Json::String &it) {
          (*dump_node)[it] = default_out_dumps[it];
        });
      } else {
        (*out_node_ptr)[key_dump] = out_options[key_dump];
      }
    }

    {
      Json::Value *format_node = nullptr;
      if((format_node = const_cast<Json::Value *>(out_node_ptr->find(key_format.data(), key_format.data()+key_format.length())))) {
        Json::Value::Members fn_keys = format_node->getMemberNames();
        std::for_each(fn_keys.begin(), fn_keys.end(), [&](const Json::String &it) {
          const Json::Value &val = (*format_node)[it];
          if(!val.isString()) {
            res = false;
          }
          auto &default_out_formats = out_options[key_format];
          if(default_out_formats.find(it.data(), it.data()+it.length())) {
            default_out_formats.removeMember(it);
          }
        });
        auto &default_out_formats = out_options[key_format];
        Json::Value::Members dfn_keys = default_out_formats.getMemberNames();
        std::for_each(dfn_keys.begin(), dfn_keys.end(), [&](const Json::String &it) {
          (*format_node)[it] = default_out_formats[it];
        });
      } else {
        (*out_node_ptr)[key_format] = out_options[key_format];
      }
    }

  } else {
    stack_out_options_.push(utils::json_value_ref(default_out_options));
  }

  Json::Value &out_node = stack_out_options_.top().ref_;
  if(call_dump_val_cb) {
    Json::Value &dump_node = out_node[key_dump];
    Json::Value::Members dn_keys = dump_node.getMemberNames();
    std::for_each(dn_keys.begin(), dn_keys.end(), [&](const Json::String &it) {
      dump_val_cb(it, dump_node[it].asBool());
    });
  }
  if(call_format_val_cb) {
    Json::Value &format_node = out_node[key_format];
    Json::Value::Members fn_keys = format_node.getMemberNames();
    std::for_each(fn_keys.begin(), fn_keys.end(), [&](const Json::String &it) {
      format_val_cb(it, format_node[it].asString());
    });
  }

  return res;
}

bool chatterbox::pop_process_out_opts()
{
  bool res = true;
  const Json::Value &out_node = stack_out_options_.top().ref_;
  const Json::Value *dump_node = nullptr;
  if((dump_node = out_node.find(key_dump.data(), key_dump.data()+key_dump.length()))) {
    Json::Value::Members keys = dump_node->getMemberNames();
    if(!keys.empty()) {
      Json::Value &out_obj = stack_obj_out_.top().ref_;
      std::for_each(keys.begin(), keys.end(), [&](const Json::String &it) {
        const Json::Value &val = (*dump_node)[it];
        if(!val.asBool()) {
          out_obj.removeMember(it);
        }
      });
    }
  }
  stack_out_options_.pop();
  return res;
}

int chatterbox::process_scenario(std::istream &is)
{
  int res = 0;

  try {
    is >> scenario_in_;
  } catch(Json::RuntimeError &e) {
    event_log_->error("malformed scenario:\n{}", e.what());
    return 1;
  }
  if((res = reset_scenario())) {
    event_log_->error("failed to reset scenario");
    return res;
  }

  bool error = false;
  {
    stack_scope scope(*this,
                      scenario_in_,
                      scenario_out_,
                      error,
                      get_default_scenario_out_options());
    if(error) {
      goto fend;
    }

    if(scope.enabled_) {
      //conversations-in
      Json::Value &conversations_in = scenario_in_[key_conversations];
      if(!conversations_in.isArray()) {
        event_log_->error("conversations field is not an array");
        goto fend;
      }

      Json::Value &conversations_out = scenario_out_[key_conversations];

      // conversations cycle
      uint32_t conv_it = 0;
      while(true) {
        if(!conversations_in.isValidIndex(conv_it)) {
          break;
        }
        if((res = process_conversation(conversations_in[conv_it],
                                       conversations_out[conv_it]))) {
          goto fend;
        }
        ++conv_it;
      }

      enrich_with_stats_scenario(scenario_out_);
      scope.commit();
    } else {
      scenario_out_.clear();
      scenario_out_[key_enabled] = false;
    }
  }

fend:
  // finally write scenario_out on the output
  if(!cfg_.no_out_) {
    output_->info("{}", scenario_out_.toStyledString());
  }
  return res;
}

void chatterbox::enrich_with_stats_scenario(Json::Value &scenario_out)
{
  Json::Value &statistics = scenario_out[key_stats];
  statistics[key_conversations] = scen_conversation_count_;
  statistics[key_requests] = scen_request_count_;
  std::for_each(scen_categorization_.begin(),
  scen_categorization_.end(), [&](auto it) {
    Json::Value &res_code_categorization = statistics[key_categorization];
    res_code_categorization[it.first] = it.second;
  });
}

int chatterbox::process_conversation(Json::Value &conversation_in,
                                     Json::Value &conversation_out)
{
  int res = 0;

  if((res = reset_conversation(conversation_in))) {
    event_log_->error("failed to reset conversation");
    return res;
  }

  bool error = false;
  {
    stack_scope scope(*this,
                      conversation_in,
                      conversation_out,
                      error,
                      get_default_conversation_out_options());
    if(error) {
      return 1;
    }

    if(scope.enabled_) {
      ++scen_conversation_count_;

      Json::Value &requests_in = conversation_in[key_requests];
      if(!requests_in.isArray()) {
        event_log_->error("requests field is not an array");
        return 1;
      }

      Json::Value &requests_out = conversation_out[key_requests];

      //requests cycle
      uint32_t req_it = 0;
      while(true) {
        if(!requests_in.isValidIndex(req_it)) {
          break;
        }
        if((res = process_request(requests_in[req_it],
                                  requests_out[req_it]))) {
          return res;
        }
        ++req_it;
      }

      enrich_with_stats_conversation(conversation_out);
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

int chatterbox::process_request(Json::Value &request_in,
                                Json::Value &request_out)
{
  int res = 0;

  // for
  auto pfor = js_env_.eval_as<uint32_t>(request_in, key_for.c_str(), 1);
  if(!pfor) {
    event_log_->error("failed to read 'for' field");
    return 1;
  }

  for(uint32_t i = 0; i < pfor; ++i) {
    bool error = false;
    {
      stack_scope scope(*this,
                        request_in,
                        request_out,
                        error,
                        get_default_request_out_options());
      if(error) {
        return 1;
      }

      if(scope.enabled_) {
        ++conv_request_count_;
        ++scen_request_count_;

        // method
        auto method = js_env_.eval_as<std::string>(request_in, key_method.c_str());
        if(!method) {
          event_log_->error("failed to read 'method' field");
          return 1;
        }

        // uri
        auto uri = js_env_.eval_as<std::string>(request_in, key_uri.c_str(), "");
        if(!uri) {
          event_log_->error("failed to read 'uri' field");
          return 1;
        }

        // query_string
        auto query_string = js_env_.eval_as<std::string>(request_in, key_query_string.c_str(), "");
        if(!query_string) {
          event_log_->error("failed to read 'query_string' field");
          return 1;
        }

        //data
        auto data = js_env_.eval_as<std::string>(request_in, key_data.c_str(), "");
        if(!data) {
          event_log_->error("failed to read 'data' field");
          return 1;
        }

        //optional auth directive
        auto auth = js_env_.eval_as<std::string>(request_in, key_auth.c_str());
        if(auth) {
          request_out[key_auth] = *auth;
        }

        request_out[key_method] = *method;
        request_out[key_uri] = *uri;
        request_out[key_query_string] = *query_string;
        request_out[key_data] = *data;

        response_mock_ = const_cast<Json::Value *>(request_in.find(key_mock.data(), key_mock.data()+key_mock.length()));

        if((res = execute_request(*method,
                                  auth,
                                  *uri,
                                  *query_string,
                                  *data,
                                  request_in,
                                  request_out))) {
          return res;
        }
        scope.commit();
      } else {
        request_out.clear();
        request_out[key_enabled] = false;
        break;
      }
    }
    if(error) {
      return 1;
    }
  }

  return res;
}

void chatterbox::enrich_with_stats_conversation(Json::Value &conversation_out)
{
  Json::Value &statistics = conversation_out[key_stats];
  statistics[key_requests] = conv_request_count_;
  std::for_each(conv_categorization_.begin(),
  conv_categorization_.end(), [&](auto it) {
    Json::Value &res_code_categorization = statistics[key_categorization];
    res_code_categorization[it.first] = it.second;
  });
}

int chatterbox::process_response(const RestClient::Response &resRC,
                                 const int64_t rtt,
                                 Json::Value &response_in,
                                 Json::Value &response_out)
{
  bool error = false;
  {
    stack_scope scope(*this,
                      response_in,
                      response_out,
                      error,
                      get_default_response_out_options());
    if(error) {
      return 1;
    }

    auto &fopts = stack_out_options_.top().ref_[key_format];

    response_out[key_code] = resRC.code;
    response_out[key_rtt] = utils::from_nano(rtt, utils::from_literal(fopts[key_rtt].asString()));

    if(!resRC.body.empty()) {
      if(fopts[key_body] == "json") {
        try {
          Json::Value body;
          std::istringstream is(resRC.body);
          is >> body;
          response_out[key_body] = body;
        } catch(const Json::RuntimeError &) {
          response_out[key_body] = resRC.body;
        }
      } else {
        response_out[key_body] = resRC.body;
      }
    }

    scope.commit();
  }
  if(error) {
    return 1;
  }

  return 0;
}

int chatterbox::execute_request(const std::string &method,
                                const std::optional<std::string> &auth,
                                const std::string &uri,
                                const std::string &query_string,
                                const std::string &data,
                                Json::Value &request_in,
                                Json::Value &request_out)
{
  int res = 0;
  RestClient::HeaderFields reqHF;

  //read user defined http-headers
  Json::Value *header_node_ptr = nullptr;
  if((header_node_ptr = const_cast<Json::Value *>(request_in.find(key_header.data(), key_header.data()+key_header.length())))) {
    Json::Value::Members keys = header_node_ptr->getMemberNames();
    std::for_each(keys.begin(), keys.end(), [&](const Json::String &it) {
      auto hdr_val = js_env_.eval_as<std::string>(*header_node_ptr, it.c_str(), "");
      if(!hdr_val) {
        res = 1;
      } else {
        reqHF[it] = *hdr_val;
      }
    });
  }
  if(res) {
    return res;
  }

  //invoke http-method
  if(method == "GET") {
    res = get(reqHF, auth, uri, query_string, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out);});
  } else if(method == "POST") {
    res = post(reqHF, auth, uri, query_string, data, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out);});
  } else if(method == "PUT") {
    res = put(reqHF, auth, uri, query_string, data, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out);});
  } else if(method == "DELETE") {
    res = del(reqHF, auth, uri, query_string, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out);});
  } else if(method == "HEAD") {
    res = head(reqHF, auth, uri, query_string, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out);});
  } else {
    event_log_->error("bad method:{}", method);
    res = 1;
  }
  return res;
}

int chatterbox::on_response(const RestClient::Response &resRC,
                            const int64_t rtt,
                            Json::Value &request_in,
                            Json::Value &request_out)
{
  int res = 0;
  std::ostringstream os;
  os << resRC.code;

  // update conv stats
  int32_t value = conv_categorization_[os.str()];
  conv_categorization_[os.str()] = ++value;

  // update scenario stats
  value = scen_categorization_[os.str()];
  scen_categorization_[os.str()] = ++value;

  Json::Value &response_in = request_in[key_response];
  Json::Value &response_out = request_out[key_response];

  res = process_response(resRC,
                         rtt,
                         response_in,
                         response_out);
  return res;
}

}
