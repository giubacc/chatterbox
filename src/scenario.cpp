#include "chatterbox.h"
#include <algorithm>

namespace rest {

int chatterbox::reset_scenario_ctx(const Json::Value &scenario_ctx)
{
  // reset stats
  scen_conversation_count_ = scen_request_count_ = 0;
  scen_res_code_categorization_.clear();

  int res = js_env_.renew_scenario_context();
  return res;
}

int chatterbox::reset_conversation_ctx(const Json::Value &conversation_ctx)
{
  // reset stats
  conv_request_count_ = 0;
  conv_res_code_categorization_.clear();

  auto raw_host = js_env_.eval_as<std::string>(conversation_ctx, "host");
  if(!raw_host) {
    event_log_->error("failed to read 'host' field");
    return 1;
  }

  raw_host_ = *raw_host;
  host_ = raw_host_;
  utils::find_and_replace(host_, "http://", "");
  utils::find_and_replace(host_, "https://", "");
  host_ = host_.substr(0, (host_.find(':') == std::string::npos ? host_.length() : host_.find(':')));

  auto service = js_env_.eval_as<std::string>(conversation_ctx, "service", "");
  service_ = *service;

  //S3 specific
  auto access_key = js_env_.eval_as<std::string>(conversation_ctx, "s3_access_key", "");
  access_key_ = *access_key;

  auto secret_key = js_env_.eval_as<std::string>(conversation_ctx, "s3_secret_key", "");
  secret_key_ = *secret_key;

  auto signed_headers = js_env_.eval_as<std::string>(conversation_ctx,
                                                     "signed_headers",
                                                     "host;x-amz-content-sha256;x-amz-date");
  signed_headers_ = *signed_headers;

  auto region = js_env_.eval_as<std::string>(conversation_ctx, "region", "US");
  region_ = *region;

  auto res_conv_dump = js_env_.eval_as<bool>(conversation_ctx, "res_conversation_dump", true);
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

int chatterbox::execute_scenario(std::istream &is)
{
  int res = 0;

  try {
    is >> scenario_in_;
  } catch(Json::RuntimeError &e) {
    event_log_->error("malformed scenario:\n{}", e.what());
    return 1;
  }
  if((res = reset_scenario_ctx(scenario_in_))) {
    event_log_->error("failed to reset scenario context");
    return res;
  }

  //exec scenario-begin handler
  if(!js_env_.exec_as_function(scenario_in_, "on_begin")) {
    event_log_->error("failed to execute scenario.on_begin handler");
    return 1;
  }

  Json::Value conversations = scenario_in_["conversations"];
  scen_conversation_count_ = conversations.size();

  // output scenario value
  scenario_out_.clear();
  Json::Value &conversations_out = scenario_out_["conversations"] = Json::Value::null;

  // conversations cycle
  for(uint32_t i = 0; i < scen_conversation_count_; ++i) {

    Json::Value conversation_ctx = conversations[i];
    if((res = reset_conversation_ctx(conversation_ctx))) {
      event_log_->error("failed to reset conversation context");
      return res;
    }

    //exec conversation-begin handler
    if(!js_env_.exec_as_function(conversation_ctx, "on_begin")) {
      event_log_->error("failed to execute conversation.on_begin handler");
      return 1;
    }

    // output conversation value
    Json::Value conversation_ctx_light = conversation_ctx;
    conversation_ctx_light.removeMember("conversation");
    conversation_ctx_light.removeMember("res_conversation_dump");
    conversation_ctx_light.removeMember("on_begin");
    conversation_ctx_light.removeMember("on_end");
    Json::Value &conversation_ctx_out = conversations_out.append(conversation_ctx_light);
    Json::Value &conversation_out = conversation_ctx_out["conversation"] = Json::Value::null;

    uint32_t request_count = 0;
    Json::Value conversation = conversation_ctx["conversation"];

    // requests cycle
    for(uint32_t i = 0; i < conversation.size(); ++i) {
      Json::Value request_ctx = conversation[i];

      // for
      auto pfor = js_env_.eval_as<uint32_t>(request_ctx, "for", 0);
      if(!pfor) {
        event_log_->error("failed to read 'for' field");
        return 1;
      }
      request_count += *pfor;

      for(uint32_t i = 0; i < pfor; ++i) {

        // method
        auto method = js_env_.eval_as<std::string>(request_ctx, "method");
        if(!method) {
          event_log_->error("failed to read 'method' field");
          return 1;
        }

        // uri
        auto uri = js_env_.eval_as<std::string>(request_ctx, "uri", "");
        if(!uri) {
          event_log_->error("failed to read 'uri' field");
          return 1;
        }

        // query_string
        auto query_string = js_env_.eval_as<std::string>(request_ctx, "query_string", "");
        if(!query_string) {
          event_log_->error("failed to read 'query_string' field");
          return 1;
        }

        //data
        auto data = js_env_.eval_as<std::string>(request_ctx, "data", "");
        if(!data) {
          event_log_->error("failed to read 'data' field");
          return 1;
        }

        //res_body_dump
        auto res_body_dump = js_env_.eval_as<bool>(request_ctx, "res_body_dump", true);
        if(!res_body_dump) {
          event_log_->error("failed to read 'res_body_dump' field");
          return 1;
        }

        //res_body_format
        auto res_body_format = js_env_.eval_as<std::string>(request_ctx, "res_body_format", "");
        if(!res_body_format) {
          event_log_->error("failed to read 'res_body_format' field");
          return 1;
        }

        //optional auth directive
        auto auth = js_env_.eval_as<std::string>(request_ctx, "auth", "aws_v4");
        if(!auth) {
          event_log_->error("failed to read 'auth' field");
          return 1;
        }

        //exec equest-begin handler
        if(!js_env_.exec_as_function(request_ctx, "on_begin")) {
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
                                  conversation_out))) {
          return res;
        }
        //exec request-end handler
        if(!js_env_.exec_as_function(request_ctx, "on_end")) {
          event_log_->error("failed to execute request.on_end handler");
          return 1;
        }
      }
    }

    //exec conversation-end handler
    on_conversation_complete(conversation_ctx_out);
    if(!js_env_.exec_as_function(conversation_ctx, "on_end")) {
      event_log_->error("failed to execute conversation.on_end handler");
      return 1;
    }
  }

  //exec scenario-end handler
  on_scenario_complete(scenario_out_);
  if(!js_env_.exec_as_function(scenario_in_, "on_end")) {
    event_log_->error("failed to execute scenario.on_end handler");
    return 1;
  }

  // finally write scenario_out on the output
  output_->info("{}", scenario_out_.toStyledString());
  return res;
}

void chatterbox::on_scenario_complete(Json::Value &scenario_out)
{
  Json::Value &statistics = scenario_out["statistics"];
  statistics["conversation_count"] = scen_conversation_count_;
  statistics["request_count"] = scen_request_count_;
  std::for_each(scen_res_code_categorization_.begin(),
  scen_res_code_categorization_.end(), [&](auto it) {
    Json::Value &res_code_categorization = statistics["res_code_categorization"];
    res_code_categorization[it.first] = it.second;
  });
}

void chatterbox::on_conversation_complete(Json::Value &conversation_ctx_out)
{
  Json::Value &statistics = conversation_ctx_out["statistics"];
  statistics["request_count"] = conv_request_count_;
  std::for_each(conv_res_code_categorization_.begin(),
  conv_res_code_categorization_.end(), [&](auto it) {
    Json::Value &res_code_categorization = statistics["res_code_categorization"];
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
                               Json::Value &conversation_out)
{
  utils::scoped_log_fmt<chatterbox> slf(*this, RAW_EVT_LOG_PATTERN);
  Json::Value request_out;

  request_out["method"] = method;
  request_out["auth"] = auth;
  request_out["uri"] = uri;
  request_out["query_string"] = query_string;
  request_out["data"] = data;
  request_out["res_code"] = res.code;

  if(res_body_dump) {
    if(res.body.empty()) {
      request_out["res_body"] = Json::Value::null;
    } else {
      if(res_body_format == "json") {
        try {
          Json::Value body;
          std::istringstream is(res.body);
          is >> body;
          request_out["res_body"] = body;
        } catch(const Json::RuntimeError &) {
          request_out["res_body"] = res.body;
        }
      } else {
        request_out["res_body"] = res.body;
      }
    }
  }

  conversation_out.append(request_out);
}

int chatterbox::execute_request(const std::string &method,
                                const std::string &auth,
                                const std::string &uri,
                                const std::string &query_string,
                                const std::string &data,
                                bool res_body_dump,
                                const std::string &res_body_format,
                                Json::Value &conversation_out)
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
                         conversation_out); });
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
                         conversation_out); });
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
                         conversation_out); });
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
                         conversation_out); });
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
                         conversation_out); });
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
                            Json::Value &conversation_out)
{
  std::ostringstream os;
  os << res.code;

  // update conv stats
  int32_t value = conv_res_code_categorization_[os.str()];
  conv_res_code_categorization_[os.str()] = ++value;
  ++conv_request_count_;

  // update scenario stats
  value = scen_res_code_categorization_[os.str()];
  scen_res_code_categorization_[os.str()] = ++value;
  ++scen_request_count_;

  if(res_conv_dump_)
    dump_response(method,
                  auth,
                  uri,
                  query_string,
                  data,
                  res_body_dump,
                  res_body_format,
                  res,
                  conversation_out);
  return 0;
}

}
