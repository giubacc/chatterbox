#include "chatterbox.h"
#include <algorithm>

namespace rest {

const std::string s3_key = "s3";
const std::string out_key = "out";
const std::string dump_key = "dump";
const std::string format_key = "format";
const std::string on_begin_key = "on_begin";
const std::string on_end_key = "on_end";

static Json::Value default_out_options;
static const Json::Value &get_default_out_options()
{
  if(default_out_options.empty()) {
    auto &default_out_dumps = default_out_options[dump_key];
    default_out_dumps[out_key] = false;
    default_out_dumps[on_begin_key] = false;
    default_out_dumps[on_end_key] = false;
    default_out_options[format_key] = Json::Value::null;
  }
  return default_out_options;
}

int chatterbox::reset_scenario()
{
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

  auto raw_host = js_env_.eval_as<std::string>(conversation_in, "host");
  if(!raw_host) {
    event_log_->error("failed to read 'host' field");
    return 1;
  }

  raw_host_ = *raw_host;
  host_ = raw_host_;
  utils::find_and_replace(host_, "http://", "");
  utils::find_and_replace(host_, "https://", "");
  host_ = host_.substr(0, (host_.find(':') == std::string::npos ? host_.length() : host_.find(':')));

  auto service = js_env_.eval_as<std::string>(conversation_in, "service", "");
  service_ = *service;

  //S3 specific

  const Json::Value *s3_node = nullptr;
  if((s3_node = conversation_in.find(s3_key.data(), s3_key.data()+s3_key.length()))) {
    auto access_key = js_env_.eval_as<std::string>(*s3_node, "access_key", "");
    access_key_ = *access_key;

    auto secret_key = js_env_.eval_as<std::string>(*s3_node, "secret_key", "");
    secret_key_ = *secret_key;

    auto signed_headers = js_env_.eval_as<std::string>(*s3_node,
                                                       "signed_headers",
                                                       "host;x-amz-content-sha256;x-amz-date");
    signed_headers_ = *signed_headers;

    auto region = js_env_.eval_as<std::string>(*s3_node, "region", "US");
    region_ = *region;
  }

  auto res_conv_dump = js_env_.eval_as<bool>(conversation_in, "res_conversation_dump", true);
  res_conv_dump_ = *res_conv_dump;

  // connection reset
  conv_conn_.reset(new RestClient::Connection(raw_host_));
  if(!conv_conn_) {
    event_log_->error("failed creating resource_conn_ object");
    return -1;
  }
  conv_conn_->SetVerifyPeer(false);
  conv_conn_->SetVerifyHost(false);
  conv_conn_->SetTimeout(30);

  return 0;
}

bool chatterbox::push_out_opts(bool call_dump_val_cb,
                               const std::function<void(const std::string &key, bool val)> &dump_val_cb,
                               bool call_format_val_cb,
                               const std::function<void(const std::string &key, const std::string &val)> &format_val_cb)
{
  bool res = true;

  Json::Value *out_node_ptr = nullptr;
  if((out_node_ptr = const_cast<Json::Value *>(stack_obj_in_.top().ref_.find(out_key.data(), out_key.data()+out_key.length())))) {
    Json::Value default_out_options = get_default_out_options();
    stack_out_options_.push(utils::json_value_ref(*const_cast<Json::Value *>(out_node_ptr)));

    {
      Json::Value *dump_node = nullptr;
      if((dump_node = const_cast<Json::Value *>(out_node_ptr->find(dump_key.data(), dump_key.data()+dump_key.length())))) {
        Json::Value::Members dn_keys = dump_node->getMemberNames();
        std::for_each(dn_keys.begin(), dn_keys.end(), [&](const Json::String &it) {
          const Json::Value &val = (*dump_node)[it];
          if(!val.isBool()) {
            res = false;
          }
          auto &default_out_dumps = default_out_options[dump_key];
          if(default_out_dumps.find(it.data(), it.data()+it.length())) {
            default_out_dumps.removeMember(it);
          }
        });
        auto &default_out_dumps = default_out_options[dump_key];
        Json::Value::Members ddn_keys = default_out_dumps.getMemberNames();
        std::for_each(ddn_keys.begin(), ddn_keys.end(), [&](const Json::String &it) {
          (*dump_node)[it] = default_out_dumps[it];
        });
      } else {
        (*out_node_ptr)[dump_key] = default_out_options[dump_key];
      }
    }

    {
      Json::Value *format_node = nullptr;
      if((format_node = const_cast<Json::Value *>(out_node_ptr->find(format_key.data(), format_key.data()+format_key.length())))) {
        Json::Value::Members fn_keys = format_node->getMemberNames();
        std::for_each(fn_keys.begin(), fn_keys.end(), [&](const Json::String &it) {
          const Json::Value &val = (*format_node)[it];
          if(!val.isString()) {
            res = false;
          }
          auto &default_out_formats = default_out_options[format_key];
          if(default_out_formats.find(it.data(), it.data()+it.length())) {
            default_out_formats.removeMember(it);
          }
        });
        auto &default_out_formats = default_out_options[format_key];
        Json::Value::Members dfn_keys = default_out_formats.getMemberNames();
        std::for_each(dfn_keys.begin(), dfn_keys.end(), [&](const Json::String &it) {
          (*format_node)[it] = default_out_formats[it];
        });
      } else {
        (*out_node_ptr)[format_key] = default_out_options[format_key];
      }
    }

  } else {
    stack_out_options_.push(utils::json_value_ref(get_default_out_options()));
  }

  Json::Value &out_node = stack_out_options_.top().ref_;
  if(call_dump_val_cb) {
    Json::Value &dump_node = out_node[dump_key];
    Json::Value::Members dn_keys = dump_node.getMemberNames();
    std::for_each(dn_keys.begin(), dn_keys.end(), [&](const Json::String &it) {
      dump_val_cb(it, dump_node[it].asBool());
    });
  }
  if(call_format_val_cb) {
    Json::Value &format_node = out_node[format_key];
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
  if((dump_node = out_node.find(dump_key.data(), dump_key.data()+dump_key.length()))) {
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

  //push scenario_in_ on stack
  stack_obj_in_.push(utils::json_value_ref(scenario_in_));
  //push scenario_out_ on stack
  stack_obj_out_.push(utils::json_value_ref(scenario_out_));

  if(!push_out_opts(false,
  [](const std::string &, bool) {},
false,
[](const std::string &, const std::string &) {})) {
    event_log_->error("failed to push out-options for the scenario");
    return 1;
  }

  //scenario-on_begin handler
  if(!js_env_.exec_as_function(scenario_in_, on_begin_key.c_str())) {
    event_log_->error("failed to execute scenario.on_begin handler");
    return 1;
  }

  //conversations-in
  Json::Value &conversations_in = scenario_in_["conversations"];
  if(!conversations_in.isArray()) {
    event_log_->error("conversations field is not an array");
    return 1;
  }

  Json::Value &conversations_out = scenario_out_["conversations"];

  // conversations cycle
  uint32_t conv_it = 0;
  while(true) {
    if(!conversations_in.isValidIndex(conv_it)) {
      break;
    }
    if((res = process_conversation(conversations_in[conv_it],
                                   conversations_out[conv_it]))) {
      event_log_->error("failed to process conversation");
      return res;
    }
    ++conv_it;
  }

  enrich_stats_scenario(scenario_out_);

  //scenario-on_end handler
  if(!js_env_.exec_as_function(scenario_in_, on_end_key.c_str())) {
    event_log_->error("failed to execute scenario.on_end handler");
    return 1;
  }

  pop_process_out_opts();

  // finally write scenario_out on the output
  output_->info("{}", scenario_out_.toStyledString());
  return res;
}

void chatterbox::enrich_stats_scenario(Json::Value &scenario_out)
{
  Json::Value &statistics = scenario_out["statistics"];
  statistics["conversations"] = scen_conversation_count_;
  statistics["requests"] = scen_request_count_;
  std::for_each(scen_categorization_.begin(),
  scen_categorization_.end(), [&](auto it) {
    Json::Value &res_code_categorization = statistics["categorization"];
    res_code_categorization[it.first] = it.second;
  });
}

int chatterbox::process_conversation(Json::Value &conversation_in,
                                     Json::Value &conversation_out)
{
  int res = 0;
  ++scen_conversation_count_;

  if((res = reset_conversation(conversation_in))) {
    event_log_->error("failed to reset conversation");
    return res;
  }

  //exec conversation-begin handler
  if(!js_env_.exec_as_function(conversation_in, on_begin_key.c_str())) {
    event_log_->error("failed to execute conversation.on_begin handler");
    return 1;
  }

  Json::Value &requests_in = conversation_in["requests"];
  if(!requests_in.isArray()) {
    event_log_->error("requests field is not an array");
    return 1;
  }

  Json::Value &requests_out = conversation_out["requests"];

  //requests cycle
  uint32_t req_it = 0;
  while(true) {
    if(!requests_in.isValidIndex(req_it)) {
      break;
    }
    if((res = process_request(requests_in[req_it],
                              requests_out[req_it]))) {
      event_log_->error("failed to process request");
      return res;
    }
    ++req_it;
  }

  enrich_stats_conversation(conversation_out);

  //exec conversation-end handler
  if(!js_env_.exec_as_function(conversation_in, on_end_key.c_str())) {
    event_log_->error("failed to execute conversation.on_end handler");
    return 1;
  }

  return res;
}

int chatterbox::process_request(Json::Value &request_in,
                                Json::Value &request_out)
{
  int res = 0;

  // for
  auto pfor = js_env_.eval_as<uint32_t>(request_in, "for", 0);
  if(!pfor) {
    event_log_->error("failed to read 'for' field");
    return 1;
  }
  conv_request_count_ += *pfor;
  scen_request_count_ += *pfor;

  for(uint32_t i = 0; i < pfor; ++i) {

    // method
    auto method = js_env_.eval_as<std::string>(request_in, "method");
    if(!method) {
      event_log_->error("failed to read 'method' field");
      return 1;
    }

    // uri
    auto uri = js_env_.eval_as<std::string>(request_in, "uri", "");
    if(!uri) {
      event_log_->error("failed to read 'uri' field");
      return 1;
    }

    // query_string
    auto query_string = js_env_.eval_as<std::string>(request_in, "query_string", "");
    if(!query_string) {
      event_log_->error("failed to read 'query_string' field");
      return 1;
    }

    //data
    auto data = js_env_.eval_as<std::string>(request_in, "data", "");
    if(!data) {
      event_log_->error("failed to read 'data' field");
      return 1;
    }

    //res_body_dump
    auto res_body_dump = js_env_.eval_as<bool>(request_in, "res_body_dump", true);
    if(!res_body_dump) {
      event_log_->error("failed to read 'res_body_dump' field");
      return 1;
    }

    //res_body_format
    auto res_body_format = js_env_.eval_as<std::string>(request_in, "res_body_format", "");
    if(!res_body_format) {
      event_log_->error("failed to read 'res_body_format' field");
      return 1;
    }

    //optional auth directive
    auto auth = js_env_.eval_as<std::string>(request_in, "auth", "aws_v4");
    if(!auth) {
      event_log_->error("failed to read 'auth' field");
      return 1;
    }

    //exec equest-begin handler
    if(!js_env_.exec_as_function(request_in, on_begin_key.c_str())) {
      event_log_->error("failed to execute request.on_end handler");
      return 1;
    }

    if((res = execute_request(*method,
                              *auth,
                              *uri,
                              *query_string,
                              *data,
                              *res_body_dump,
                              *res_body_format,
                              request_out))) {
      return res;
    }

    //exec request-end handler
    if(!js_env_.exec_as_function(request_in, on_end_key.c_str())) {
      event_log_->error("failed to execute request.on_end handler");
      return 1;
    }
  }

  return res;
}

void chatterbox::enrich_stats_conversation(Json::Value &conversation_out)
{
  Json::Value &statistics = conversation_out["statistics"];
  statistics["requests"] = conv_request_count_;
  std::for_each(conv_categorization_.begin(),
  conv_categorization_.end(), [&](auto it) {
    Json::Value &res_code_categorization = statistics["categorization"];
    res_code_categorization[it.first] = it.second;
  });
}

void chatterbox::dump_response(const std::string &method,
                               const std::string &auth,
                               const std::string &uri,
                               const std::string &query_string,
                               const std::string &data,
                               bool res_body_dump,
                               const std::string &res_body_format,
                               const RestClient::Response &res,
                               Json::Value &request_out)
{
  request_out["method"] = method;
  request_out["auth"] = auth;
  request_out["uri"] = uri;
  request_out["query_string"] = query_string;
  request_out["data"] = data;

  Json::Value &response = request_out["response"] = Json::Value::null;
  response["code"] = res.code;

  if(res_body_dump) {
    if(res.body.empty()) {
      response["body"] = Json::Value::null;
    } else {
      if(res_body_format == "json") {
        try {
          Json::Value body;
          std::istringstream is(res.body);
          is >> body;
          response["body"] = body;
        } catch(const Json::RuntimeError &) {
          response["body"] = res.body;
        }
      } else {
        response["body"] = res.body;
      }
    }
  }
}

int chatterbox::execute_request(const std::string &method,
                                const std::string &auth,
                                const std::string &uri,
                                const std::string &query_string,
                                const std::string &data,
                                bool res_body_dump,
                                const std::string &res_body_format,
                                Json::Value &request_out)
{
  if(method == "GET") {
    get(auth, uri, query_string, [&](RestClient::Response &res) -> int {
      return on_response(res,
                         method,
                         auth,
                         uri,
                         query_string,
                         data,
                         res_body_dump,
                         res_body_format,
                         request_out); });
  } else if(method == "POST") {
    post(auth, uri, query_string, data, [&](RestClient::Response &res) -> int {
      return on_response(res,
                         method,
                         auth,
                         uri,
                         query_string,
                         data,
                         res_body_dump,
                         res_body_format,
                         request_out); });
  } else if(method == "PUT") {
    put(auth, uri, query_string, data, [&](RestClient::Response &res) -> int {
      return on_response(res,
                         method,
                         auth,
                         uri,
                         query_string,
                         data,
                         res_body_dump,
                         res_body_format,
                         request_out); });
  } else if(method == "DELETE") {
    del(auth, uri, query_string, [&](RestClient::Response &res) -> int {
      return on_response(res,
                         method,
                         auth,
                         uri,
                         query_string,
                         data,
                         res_body_dump,
                         res_body_format,
                         request_out); });
  } else if(method == "HEAD") {
    head(auth, uri, query_string, [&](RestClient::Response &res) -> int {
      return on_response(res,
                         method,
                         auth,
                         uri,
                         query_string,
                         data,
                         res_body_dump,
                         res_body_format,
                         request_out); });
  } else {
    event_log_->error("bad method:{}", method);
    return 1;
  }
  return 0;
}

int chatterbox::on_response(const RestClient::Response &res,
                            const std::string &method,
                            const std::string &auth,
                            const std::string &uri,
                            const std::string &query_string,
                            const std::string &data,
                            bool res_body_dump,
                            const std::string &res_body_format,
                            Json::Value &request_out)
{
  std::ostringstream os;
  os << res.code;

  // update conv stats
  int32_t value = conv_categorization_[os.str()];
  conv_categorization_[os.str()] = ++value;

  // update scenario stats
  value = scen_categorization_[os.str()];
  scen_categorization_[os.str()] = ++value;

  if(res_conv_dump_)
    dump_response(method,
                  auth,
                  uri,
                  query_string,
                  data,
                  res_body_dump,
                  res_body_format,
                  res,
                  request_out);
  return 0;
}

}
