#include "chatterbox.h"
#include <algorithm>

namespace rest {

int chatterbox::reset_scenario_ctx(const Json::Value &scenario_ctx)
{
  //reset stats
  scen_conversation_count_ = scen_talk_count_ = 0;
  scen_res_code_categorization_.clear();

  int res = js_env_.renew_scenario_context();
  return res;
}

int chatterbox::reset_conversation_ctx(const Json::Value &conversation_ctx)
{
  //reset stats
  conv_talk_count_ = 0;
  conv_res_code_categorization_.clear();

  auto raw_host = eval_as_string(conversation_ctx, "host");
  if(!raw_host) {
    event_log_->error("failed to read 'host' field");
    return 1;
  }

  raw_host_ = *raw_host;
  host_ = raw_host_;
  utils::find_and_replace(host_, "http://", "");
  utils::find_and_replace(host_, "https://", "");
  host_ = host_.substr(0, (host_.find(':') == std::string::npos ? host_.length() : host_.find(':')));

  auto service = eval_as_string(conversation_ctx, "service", "");
  service_ = *service;

  //S3 specific
  auto access_key = eval_as_string(conversation_ctx, "s3_access_key", "");
  access_key_ = *access_key;

  auto secret_key = eval_as_string(conversation_ctx, "s3_secret_key", "");
  secret_key_ = *secret_key;

  auto signed_headers = eval_as_string(conversation_ctx, "signed_headers", "host;x-amz-content-sha256;x-amz-date");
  signed_headers_ = *signed_headers;

  auto region = eval_as_string(conversation_ctx, "region", "US");
  region_ = *region;

  auto res_conv_dump = eval_as_bool(conversation_ctx, "res_conversation_dump", true);
  res_conv_dump_ = *res_conv_dump;

  //connection reset
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

  Json::Value scenario;
  try {
    is >> scenario;
  } catch(Json::RuntimeError &e) {
    event_log_->error("malformed scenario:\n{}", e.what());
    return 1;
  }
  if((res = reset_scenario_ctx(scenario))) {
    event_log_->error("failed to reset scenario context");
    return res;
  }

  Json::Value conversations = scenario["conversations"];
  scen_conversation_count_ = conversations.size();

  //output scenario value
  Json::Value scenario_out;
  Json::Value &conversations_out = scenario_out["conversations"] = Json::Value::null;

  //conversations cycle
  for(uint32_t i = 0; i < scen_conversation_count_; ++i) {

    Json::Value conversation_ctx = conversations[i];
    if((res = reset_conversation_ctx(conversation_ctx))) {
      event_log_->error("failed to reset conversation context");
      return res;
    }

    //output conversation value
    Json::Value conversation_ctx_light = conversation_ctx;
    conversation_ctx_light.removeMember("conversation");
    conversation_ctx_light.removeMember("res_conversation_dump");
    Json::Value &conversation_ctx_out = conversations_out.append(conversation_ctx_light);
    Json::Value &conversation_out = conversation_ctx_out["conversation"] = Json::Value::null;

    uint32_t talk_count = 0;
    Json::Value conversation = conversation_ctx["conversation"];

    //talks cycle
    for(uint32_t i = 0; i < conversation.size(); ++i) {

      //talk
      auto talk = conversation[i].get("talk", "");

      //for
      auto pfor = eval_as_uint32_t(conversation[i], "for", 0);
      if(!pfor) {
        event_log_->error("failed to read 'for' field");
        return 1;
      }
      talk_count += *pfor;

      for(uint32_t i = 0; i<pfor; ++i) {

        //verb
        auto verb = eval_as_string(talk, "verb");
        if(!verb) {
          event_log_->error("failed to read 'verb' field");
          return 1;
        }

        //uri
        auto uri = eval_as_string(talk, "uri", "");
        if(!uri) {
          event_log_->error("failed to read 'uri' field");
          return 1;
        }

        //query_string
        auto query_string = eval_as_string(talk, "query_string", "");
        if(!query_string) {
          event_log_->error("failed to read 'query_string' field");
          return 1;
        }

        //data
        auto data = eval_as_string(talk, "data", "");
        if(!data) {
          event_log_->error("failed to read 'data' field");
          return 1;
        }

        //res_body_dump
        auto res_body_dump = eval_as_bool(talk, "res_body_dump", true);
        if(!res_body_dump) {
          event_log_->error("failed to read 'res_body_dump' field");
          return 1;
        }

        //res_body_format
        auto res_body_format = eval_as_string(talk, "res_body_format", "");
        if(!res_body_format) {
          event_log_->error("failed to read 'res_body_format' field");
          return 1;
        }

        //optional auth directive
        auto auth = eval_as_string(talk, "auth", "aws_v4");
        if(!auth) {
          event_log_->error("failed to read 'auth' field");
          return 1;
        }

        if((res = execute_talk(*verb,
                               *auth,
                               *uri,
                               *query_string,
                               *data,
                               *res_body_dump,
                               *res_body_format,
                               conversation_out))) {
          return res;
        }
      }
    }

    on_conversation_complete(conversation_ctx_out);
  }

  on_scenario_complete(scenario_out);

  //finally write scenario_out on the output
  output_->info("{}", scenario_out.toStyledString());
  return res;
}

void chatterbox::on_scenario_complete(Json::Value &scenario_out)
{
  Json::Value &statistics = scenario_out["statistics"];
  statistics["conversation_count"] = scen_conversation_count_;
  statistics["talk_count"] = scen_talk_count_;
  std::for_each(scen_res_code_categorization_.begin(),
  scen_res_code_categorization_.end(), [&](auto it) {
    Json::Value &res_code_categorization = statistics["res_code_categorization"];
    res_code_categorization[it.first] = it.second;
  });
}

void chatterbox::on_conversation_complete(Json::Value &conversation_ctx_out)
{
  Json::Value &statistics = conversation_ctx_out["statistics"];
  statistics["talk_count"] = conv_talk_count_;
  std::for_each(conv_res_code_categorization_.begin(),
  conv_res_code_categorization_.end(), [&](auto it) {
    Json::Value &res_code_categorization = statistics["res_code_categorization"];
    res_code_categorization[it.first] = it.second;
  });
}

void chatterbox::dump_talk_response(const Json::Value &talk,
                                    bool res_body_dump,
                                    const std::string &res_body_format,
                                    const RestClient::Response &res,
                                    Json::Value &conversation_out)
{
  utils::scoped_log_fmt<chatterbox> slf(*this, RAW_EVT_LOG_PATTERN);
  Json::Value talk_response;

  talk_response["talk"] = talk;
  talk_response["res_code"] = res.code;

  if(res_body_dump) {
    if(res.body.empty()) {
      talk_response["res_body"] = Json::Value::null;
    } else {
      if(res_body_format == "json") {
        try {
          Json::Value body;
          std::istringstream is(res.body);
          is >> body;
          talk_response["res_body"] = body;
        } catch(const Json::RuntimeError &) {
          talk_response["res_body"] = res.body;
        }
      } else {
        talk_response["res_body"] = res.body;
      }
    }
  }

  conversation_out.append(talk_response);
}

Json::Value render_talk_json(const std::string &verb,
                             const std::string &auth,
                             const std::string &uri,
                             const std::string &query_string,
                             const std::string &data)
{
  Json::Value talk;
  talk["verb"] = verb;
  talk["auth"] = auth;
  talk["uri"] = uri;
  talk["query_string"] = query_string;
  talk["data"] = data;
  return talk;
}

int chatterbox::execute_talk(const std::string &verb,
                             const std::string &auth,
                             const std::string &uri,
                             const std::string &query_string,
                             const std::string &data,
                             bool res_body_dump,
                             const std::string &res_body_format,
                             Json::Value &conversation_out)
{
  if(verb == "GET") {
    get(auth, uri, query_string, [&](RestClient::Response &res) -> int {
      return on_talk_response(res,
                              verb,
                              auth,
                              uri,
                              query_string,
                              data,
                              res_body_dump,
                              res_body_format,
                              conversation_out);
    });
  } else if(verb == "POST") {
    post(auth, uri, query_string, data, [&](RestClient::Response &res) -> int {
      return on_talk_response(res,
                              verb,
                              auth,
                              uri,
                              query_string,
                              data,
                              res_body_dump,
                              res_body_format,
                              conversation_out);
    });
  } else if(verb == "PUT") {
    put(auth, uri, query_string, data, [&](RestClient::Response &res) -> int {
      return on_talk_response(res,
                              verb,
                              auth,
                              uri,
                              query_string,
                              data,
                              res_body_dump,
                              res_body_format,
                              conversation_out);
    });
  } else if(verb == "DELETE") {
    del(auth, uri, query_string, [&](RestClient::Response &res) -> int {
      return on_talk_response(res,
                              verb,
                              auth,
                              uri,
                              query_string,
                              data,
                              res_body_dump,
                              res_body_format,
                              conversation_out);
    });
  } else if(verb == "HEAD") {
    head(auth, uri, query_string, [&](RestClient::Response &res) -> int {
      return on_talk_response(res,
                              verb,
                              auth,
                              uri,
                              query_string,
                              data,
                              res_body_dump,
                              res_body_format,
                              conversation_out);
    });
  } else {
    event_log_->error("bad verb:{}", verb);
    return 1;
  }
  return 0;
}

int chatterbox::on_talk_response(const RestClient::Response &res,
                                 const std::string &verb,
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

  //update conv stats
  int32_t value = conv_res_code_categorization_[os.str()];
  conv_res_code_categorization_[os.str()] = ++value;
  ++conv_talk_count_;

  //update scenario stats
  value = scen_res_code_categorization_[os.str()];
  scen_res_code_categorization_[os.str()] = ++value;
  ++scen_talk_count_;

  if(res_conv_dump_)
    dump_talk_response(render_talk_json(verb,
                                        auth,
                                        uri,
                                        query_string,
                                        data),
                       res_body_dump,
                       res_body_format,
                       res,
                       conversation_out);
  return 0;
}

// ------------------
// --- Evaluators ---
// ------------------

std::optional<bool> chatterbox::eval_as_bool(const Json::Value &from,
                                             const char *key,
                                             const std::optional<bool> default_value)
{
  Json::Value val = from.get(key, Json::Value::null);
  if(val) {
    if(val.isBool()) {
      //eval as primitive value
      return val.asBool();
    } else {
      //eval as javascript function
      Json::Value function = val.get("function", Json::Value::null);
      if(!function || !function.isString()) {
        event_log_->error("function name is not string type");
        return std::nullopt;
      }

      Json::Value j_args = val.get("args", Json::Value::null);
      std::vector<std::string> args;
      if(j_args) {
        if(!j_args.isArray()) {
          event_log_->error("function args are not array type");
          return std::nullopt;
        }
        for(uint32_t i = 0; i < j_args.size(); ++i) {
          Json::Value arg = j_args[i];
          if(!arg.isString()) {
            event_log_->error("function arg is not string type");
            return std::nullopt;
          }
          args.emplace_back(arg.asString());
        }
      }

      //finally invoke the javascript function
      bool result = false;
      std::string error;

      bool js_res = js_env_.invoke_js_function(function.asCString(),
                                               args,
      [&](v8::Isolate *isl, const std::vector<std::string> &args, v8::Local<v8::Value> argv[]) -> bool{

        std::transform(args.begin(), args.end(), argv, [&](const std::string &it) -> v8::Local<v8::String> {
          return v8::String::NewFromUtf8(isl, it.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
        });

        return true;
      },
      [&](v8::Isolate *isl, const v8::Local<v8::Value> &res) -> bool{
        if(!res->IsBoolean())
        {
          event_log_->error("function result is not boolean type");
          return false;
        }
        result = res->BooleanValue(isl);
        return true;
      },
      error);

      if(!js_res) {
        event_log_->error("failure invoking function:{}, error:{}", function.asString(), error);
        return std::nullopt;
      }
      return result;
    }
  } else {
    return default_value;
  }
}

std::optional<uint32_t> chatterbox::eval_as_uint32_t(const Json::Value &from,
                                                     const char *key,
                                                     const std::optional<uint32_t> default_value)
{
  Json::Value val = from.get(key, Json::Value::null);
  if(val) {
    if(val.isUInt()) {
      //eval as primitive value
      return val.asUInt();
    } else {
      //eval as javascript function
      Json::Value function = val.get("function", Json::Value::null);
      if(!function || !function.isString()) {
        event_log_->error("function name is not string type");
        return std::nullopt;
      }

      Json::Value j_args = val.get("args", Json::Value::null);
      std::vector<std::string> args;
      if(j_args) {
        if(!j_args.isArray()) {
          event_log_->error("function args are not array type");
          return std::nullopt;
        }
        for(uint32_t i = 0; i < j_args.size(); ++i) {
          Json::Value arg = j_args[i];
          if(!arg.isString()) {
            event_log_->error("function arg is not string type");
            return std::nullopt;
          }
          args.emplace_back(arg.asString());
        }
      }

      //finally invoke the javascript function
      uint32_t result = 0;
      std::string error;

      bool js_res = js_env_.invoke_js_function(function.asCString(),
                                               args,
      [&](v8::Isolate *isl, const std::vector<std::string> &args, v8::Local<v8::Value> argv[]) -> bool{

        std::transform(args.begin(), args.end(), argv, [&](const std::string &it) -> v8::Local<v8::String> {
          return v8::String::NewFromUtf8(isl, it.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
        });

        return true;
      },
      [&](v8::Isolate *isl, const v8::Local<v8::Value> &res) -> bool{
        if(!res->IsUint32())
        {
          event_log_->error("function result is not UInt type");
          return false;
        }
        result = res->Uint32Value(isl->GetCurrentContext()).FromMaybe(0);
        return true;
      },
      error);

      if(!js_res) {
        event_log_->error("failure invoking function:{}, error:{}", function.asString(), error);
        return std::nullopt;
      }
      return result;
    }
  } else {
    return default_value;
  }
}

std::optional<std::string> chatterbox::eval_as_string(const Json::Value &from,
                                                      const char *key,
                                                      const std::optional<std::string> &default_value)
{
  Json::Value val = from.get(key, Json::Value::null);
  if(val) {
    if(val.isString()) {
      //eval as primitive value
      return val.asString();
    } else {
      //eval as javascript function
      Json::Value function = val.get("function", Json::Value::null);
      if(!function || !function.isString()) {
        event_log_->error("function name is not string type");
        return std::nullopt;
      }

      Json::Value j_args = val.get("args", Json::Value::null);
      std::vector<std::string> args;
      if(j_args) {
        if(!j_args.isArray()) {
          event_log_->error("function args are not array type");
          return std::nullopt;
        }
        for(uint32_t i = 0; i < j_args.size(); ++i) {
          Json::Value arg = j_args[i];
          if(!arg.isString()) {
            event_log_->error("function arg is not string type");
            return std::nullopt;
          }
          args.emplace_back(arg.asString());
        }
      }

      //finally invoke the javascript function
      std::string result;
      std::string error;

      bool js_res = js_env_.invoke_js_function(function.asCString(),
                                               args,
      [&](v8::Isolate *isl, const std::vector<std::string> &args, v8::Local<v8::Value> argv[]) -> bool{

        std::transform(args.begin(), args.end(), argv, [&](const std::string &it) -> v8::Local<v8::String> {
          return v8::String::NewFromUtf8(isl, it.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
        });

        return true;
      },
      [&](v8::Isolate *isl, const v8::Local<v8::Value> &res) -> bool{
        if(!res->IsString())
        {
          event_log_->error("function result is not String type");
          return false;
        }
        v8::String::Utf8Value utf_res(isl, res);
        result = *utf_res;
        return true;
      },
      error);

      if(!js_res) {
        event_log_->error("failure invoking function:{}, error:{}", function.asString(), error);
        return std::nullopt;
      }
      return result;
    }
  } else {
    return default_value;
  }
}

}
