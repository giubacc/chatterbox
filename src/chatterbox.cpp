#include "chatterbox.h"
#include <algorithm>

const std::string algorithm = "AWS4-HMAC-SHA256";

namespace rest {

int chatterbox::reset_scenario_ctx(const Json::Value &scenario_ctx)
{
  int res = js_env_.renew_scenario_context();
  return res;
}

int chatterbox::reset_conversation_ctx(const Json::Value &conversation_ctx)
{
  raw_host_ = conversation_ctx.get("host", "").asString();
  host_ = raw_host_;
  utils::find_and_replace(host_, "http://", "");
  utils::find_and_replace(host_, "https://", "");
  host_ = host_.substr(0, (host_.find(':') == std::string::npos ? host_.length() : host_.find(':')));

  service_ = conversation_ctx.get("service", "").asString();

  //S3 specific
  access_key_ = conversation_ctx.get("s3_access_key", "").asString();
  secret_key_ = conversation_ctx.get("s3_secret_key", "").asString();
  signed_headers_ = conversation_ctx.get("signed_headers", "host;x-amz-content-sha256;x-amz-date").asString();
  region_ = conversation_ctx.get("region", "US").asString();

  //connection reset
  resource_conn_.reset(new RestClient::Connection(raw_host_));
  if(!resource_conn_) {
    event_log_->error("failed creating resource_conn_ object");
    return -1;
  }
  resource_conn_->SetVerifyPeer(false);
  resource_conn_->SetVerifyHost(false);
  resource_conn_->SetTimeout(30);

  return 0;
}

chatterbox::chatterbox() : js_env_(*this)
{}

chatterbox::~chatterbox()
{}

int chatterbox::init(int argc, char *argv[])
{
  //output init
  std::shared_ptr<spdlog::logger> output;
  if(cfg_.out_channel == "stdout") {
    output = spdlog::stdout_color_mt("output");
  } else if(cfg_.out_channel == "stderr") {
    output = spdlog::stderr_color_mt("output");
  } else {
    output = spdlog::basic_logger_mt(cfg_.out_channel, cfg_.out_channel);
  }
  output->set_pattern("%v");
  output->set_level(spdlog::level::info);
  output_ = output;

  //event logger init
  std::shared_ptr<spdlog::logger> event_log;
  if(cfg_.evt_log_channel == "stdout") {
    event_log = spdlog::stdout_color_mt("event_log");
  } else if(cfg_.evt_log_channel == "stderr") {
    event_log = spdlog::stderr_color_mt("event_log");
  } else {
    event_log = spdlog::basic_logger_mt(cfg_.evt_log_channel, cfg_.evt_log_channel);
  }

  event_log_fmt_ = DEF_EVT_LOG_PATTERN;
  event_log->set_pattern(event_log_fmt_);
  event_log->set_level(utils::get_spdloglvl(cfg_.evt_log_level.c_str()));
  event_log_ = event_log;

  spdlog::flush_every(std::chrono::seconds(2));

  int res = 0;
  if((res = js_env_.init(event_log_))) {
    return res;
  }

  return res;
}

std::string chatterbox::build_v2_date()
{
  char buf[256];
  time_t now = time(0);
  struct tm tm = *gmtime(&now);
  strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);
  return buf;
}

std::string chatterbox::build_v4_date()
{
  time_t now;
  time(&now);
  char buf[sizeof "YYYYMMDDTHHMMSSZ"];
  strftime(buf, sizeof buf, "%Y%m%dT%H%M%SZ", gmtime(&now));
  return buf;
}

std::string chatterbox::build_v2_authorization(const std::string &signature) const
{
  std::ostringstream os;
  os << "AWS "
     << access_key_ << ':'
     << signature;
  return os.str();
}

std::string chatterbox::build_v2_canonical_string(const std::string &method,
                                                  const std::string &canonical_uri) const
{
  std::ostringstream os;
  os << method << std::endl
     << std::endl //content-md5
     << std::endl //content-type
     << std::endl //date
     << "x-amz-date:" << x_amz_date_ << std::endl
     << canonical_uri;

  event_log_->trace("canonical_string:\n{}", os.str());
  return os.str();
}

void chatterbox::build_v2(const char *verb,
                          const std::string &uri,
                          RestClient::HeaderFields &reqHF) const
{
  reqHF["x-amz-date"] = x_amz_date_;

  auto signature = crypto::base64(crypto::hmac_sha1(access_key_,
                                                    build_v2_canonical_string(verb,
                                                                              uri)));

  event_log_->trace("v2 signature: {}", signature);

  reqHF["Authorization"] = build_v2_authorization(signature);
}

std::string chatterbox::build_v4_authorization(const std::string &signature) const
{
  std::ostringstream os;
  os << algorithm << " Credential="
     << access_key_ << '/'
     << x_amz_date_.substr(0,8) << '/'
     << region_ << '/'
     << service_ << '/'
     << "aws4_request" << ','
     << "SignedHeaders=" << signed_headers_ << ','
     << "Signature=" << signature;
  return os.str();
}

std::string chatterbox::build_v4_signing_key() const
{
  /*
    DateKey              = HMAC-SHA256("AWS4"+"<SecretAccessKey>", "<YYYYMMDD>")
    DateRegionKey        = HMAC-SHA256(<DateKey>, "<aws-region>")
    DateRegionServiceKey = HMAC-SHA256(<DateRegionKey>, "<aws-service>")
    SigningKey           = HMAC-SHA256(<DateRegionServiceKey>, "aws4_request")
  */

  std::string sec_key;
  {
    std::ostringstream os;
    os << "AWS4" << secret_key_ ;
    sec_key = os.str();
  }

  auto date_key = crypto::hmac_sha256(sec_key, x_amz_date_.substr(0,8));
  event_log_->trace("date_key: {}", crypto::hex(date_key));

  auto date_region_key = crypto::hmac_sha256(date_key, region_);
  event_log_->trace("date_region_key: {}", crypto::hex(date_region_key));

  auto date_region_service_key = crypto::hmac_sha256(date_region_key, service_);
  event_log_->trace("date_region_service_key: {}", crypto::hex(date_region_service_key));

  auto signing_key = crypto::hmac_sha256(date_region_service_key, "aws4_request");
  event_log_->trace("signing_key: {}", crypto::hex(signing_key));

  return signing_key;
}

std::string chatterbox::build_v4_canonical_headers(const std::string &x_amz_content_sha256) const
{
  std::ostringstream os;
  os << "host:" << host_ << std::endl
     << "x-amz-content-sha256:" << x_amz_content_sha256 << std::endl
     << "x-amz-date:" << x_amz_date_ << std::endl;
  return os.str();
}

std::string chatterbox::build_v4_canonical_request(const std::string &method,
                                                   const std::string &canonical_uri,
                                                   const std::string &canonical_query_string,
                                                   const std::string &canonical_headers,
                                                   const std::string &payload_hash) const
{
  std::ostringstream os;
  os << method << std::endl
     << canonical_uri << std::endl
     << canonical_query_string << std::endl
     << canonical_headers << std::endl
     << signed_headers_ << std::endl
     << payload_hash;

  event_log_->trace("canonical_request:\n{}", os.str());
  return os.str();
}

std::string chatterbox::build_v4_string_to_sign(const std::string &canonical_request) const
{
  /*
    AWS4-HMAC-SHA256
    20220802T082848Z
    20220802/US/s3/aws4_request
    8bd209a688692fd34b25a2b81a026ce79752a60633bd7dd570a24deb651b2d41
  */
  std::ostringstream os;
  os << algorithm << std::endl
     << x_amz_date_ << std::endl
     << x_amz_date_.substr(0,8) << '/'
     << region_ << '/'
     << service_ << '/'
     << "aws4_request" << std::endl
     << crypto::hex(crypto::sha256(canonical_request));

  event_log_->trace("string_to_sign:\n{}", os.str());
  return os.str();
}

void chatterbox::build_v4(const char *verb,
                          const std::string &uri,
                          const std::string &query_string,
                          const std::string &data,
                          RestClient::HeaderFields &reqHF) const
{
  std::string x_amz_content_sha256 = crypto::hex(crypto::sha256(data));

  reqHF["host"] = host_;
  reqHF["x-amz-date"] = x_amz_date_;
  reqHF["x-amz-content-sha256"] = x_amz_content_sha256;

  auto signature = crypto::hex(crypto::hmac_sha256(build_v4_signing_key(),
                                                   build_v4_string_to_sign(
                                                     build_v4_canonical_request(verb,
                                                                                uri,
                                                                                query_string,
                                                                                build_v4_canonical_headers(x_amz_content_sha256),
                                                                                x_amz_content_sha256))));
  event_log_->trace("v4 signature: {}", signature);

  reqHF["Authorization"] = build_v4_authorization(signature);
}

void chatterbox::dump_hdr(const RestClient::HeaderFields &hdr) const
{
  std::for_each(hdr.begin(), hdr.end(), [&](auto it) {
    event_log_->trace("{}:{}", it.first, it.second);
  });
}

void chatterbox::poll()
{
  DIR *dir;
  struct dirent *ent;

  if((dir = opendir(cfg_.in_scenario_path.c_str())) != nullptr) {
    while((ent = readdir(dir)) != nullptr) {
      if(strcmp(ent->d_name,".") && strcmp(ent->d_name,"..")) {
        struct stat info;
        std::ostringstream fpath;
        fpath << cfg_.in_scenario_path << "/" << ent->d_name;
        stat(fpath.str().c_str(), &info);
        if(!S_ISDIR(info.st_mode)) {
          if(!utils::ends_with(ent->d_name, ".json")) {
            continue;
          }
          execute_scenario(ent->d_name);
        }
      }
    }
    if(closedir(dir)) {
      event_log_->error("closedir: {}", strerror(errno));
    }
  } else {
    event_log_->critical("opendir: {}", strerror(errno));
  }
}

void chatterbox::move_file(const char *filename)
{
  std::ostringstream os_src, os_to;
  os_src << cfg_.in_scenario_path;
  mkdir((os_src.str() + "/consumed").c_str(), 0777);

  os_src << "/" << filename;
  os_to << cfg_.in_scenario_path << "/consumed/" << filename;

  std::string to_fname_base(os_to.str());
  std::string to_fname(to_fname_base);

  int ntry = 0;
  while(rename(os_src.str().c_str(), to_fname.c_str()) && ntry++ < 10) {
    event_log_->error("moving to file_path: {}", to_fname_base.c_str());
    std::ostringstream os_to_retry;
    os_to_retry << to_fname_base << ".try" << ntry;
    to_fname = os_to_retry.str();
  }
}

void chatterbox::rm_file(const char *filename)
{
  std::ostringstream fpath;
  fpath << cfg_.in_scenario_path << "/" << filename;
  if(unlink(fpath.str().c_str())) {
    event_log_->error("unlink: {}", strerror(errno));
  }
}

void chatterbox::dump_talk_response(const Json::Value &talk,
                                    bool res_body_dump,
                                    const std::string &res_body_format,
                                    RestClient::Response &res,
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

int chatterbox::execute_scenario(const char *fname)
{
  int res = 0;
  event_log_->trace("processing scenario file:{}", fname);

  std::stringstream ss;
  std::ostringstream fpath;
  fpath << cfg_.in_scenario_path << "/" << fname;
  if((res = utils::read_file(fpath.str().c_str(), ss, event_log_.get()))) {
    event_log_->error("[read_file] {}", fname);
  } else {
    res = execute_scenario(ss);
  }

  if(cfg_.monitor) {
    if(cfg_.move_scenario) {
      move_file(fname);
    } else {
      rm_file(fname);
    }
  }

  return res;
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

  //output scenario value
  Json::Value scenario_out;
  Json::Value &conversations_out = scenario_out["conversations"] = Json::Value::null;

  //conversations cycle
  for(uint32_t i = 0; i < conversations.size(); ++i) {

    Json::Value conversation_ctx = conversations[i];
    if((res = reset_conversation_ctx(conversation_ctx))) {
      event_log_->error("failed to reset conversation context");
      return res;
    }

    //output conversation value
    Json::Value conversation_ctx_light = conversation_ctx;
    conversation_ctx_light.removeMember("conversation");
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
  }

  //finally write scenario_out on the output
  output_->info("{}", scenario_out.toStyledString());
  return res;
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
    });
  } else if(verb == "POST") {
    post(auth, uri, query_string, data, [&](RestClient::Response &res) -> int {
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
    });
  } else if(verb == "PUT") {
    put(auth, uri, query_string, data, [&](RestClient::Response &res) -> int {
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
    });
  } else if(verb == "DELETE") {
    del(auth, uri, query_string, [&](RestClient::Response &res) -> int {
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
    });
  } else if(verb == "HEAD") {
    head(auth, uri, query_string, [&](RestClient::Response &res) -> int {
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
    });
  } else {
    event_log_->error("bad verb:{}", verb);
    return 1;
  }
  return 0;
}

// --------------------
// --- HTTP METHODS ---
// --------------------

int chatterbox::post(const std::string &auth,
                     const std::string &uri,
                     const std::string &query_string,
                     const std::string &data,
                     const std::function <void (RestClient::Response &)> &cb)
{

  std::string luri("/");
  luri += uri;

  RestClient::HeaderFields reqHF;

  if(service_ == "s3") {
    if(auth == "aws_v2") {
      x_amz_date_ = build_v2_date();
      build_v2("POST", luri, reqHF);
    } else if(auth == "aws_v4") {
      x_amz_date_ = build_v4_date();
      build_v4("POST", luri, query_string, data, reqHF);
    }
  }

  resource_conn_->SetHeaders(reqHF);
  dump_hdr(reqHF);

  if(query_string != "") {
    luri += '?';
    luri += query_string;
  }

  RestClient::Response res;
  res = resource_conn_->post(luri, data);

  cb(res);
  return res.code;
}

int chatterbox::put(const std::string &auth,
                    const std::string &uri,
                    const std::string &query_string,
                    const std::string &data,
                    const std::function <void (RestClient::Response &)> &cb)
{

  std::string luri("/");
  luri += uri;

  RestClient::HeaderFields reqHF;

  if(service_ == "s3") {
    if(auth == "aws_v2") {
      x_amz_date_ = build_v2_date();
      build_v2("PUT", luri, reqHF);
    } else if(auth == "aws_v4") {
      x_amz_date_ = build_v4_date();
      build_v4("PUT", luri, query_string, data, reqHF);
    }
  }

  resource_conn_->SetHeaders(reqHF);
  dump_hdr(reqHF);

  if(query_string != "") {
    luri += '?';
    luri += query_string;
  }

  RestClient::Response res;
  res = resource_conn_->put(luri, data);

  cb(res);
  return res.code;
}

int chatterbox::get(const std::string &auth,
                    const std::string &uri,
                    const std::string &query_string,
                    const std::function <void (RestClient::Response &)> &cb)
{

  std::string luri("/");
  luri += uri;

  RestClient::HeaderFields reqHF;

  if(service_ == "s3") {
    if(auth == "aws_v2") {
      x_amz_date_ = build_v2_date();
      build_v2("GET", luri, reqHF);
    } else if(auth == "aws_v4") {
      x_amz_date_ = build_v4_date();
      build_v4("GET", luri, query_string, "", reqHF);
    }
  }

  resource_conn_->SetHeaders(reqHF);
  dump_hdr(reqHF);

  if(query_string != "") {
    luri += '?';
    luri += query_string;
  }

  RestClient::Response res;
  res = resource_conn_->get(luri);

  cb(res);
  return res.code;
}

int chatterbox::del(const std::string &auth,
                    const std::string &uri,
                    const std::string &query_string,
                    const std::function <void (RestClient::Response &)> &cb)
{

  std::string luri("/");
  luri += uri;

  RestClient::HeaderFields reqHF;

  if(service_ == "s3") {
    if(auth == "aws_v2") {
      x_amz_date_ = build_v2_date();
      build_v2("DELETE", luri, reqHF);
    } else if(auth == "aws_v4") {
      x_amz_date_ = build_v4_date();
      build_v4("DELETE", luri, query_string, "", reqHF);
    }
  }

  resource_conn_->SetHeaders(reqHF);
  dump_hdr(reqHF);

  if(query_string != "") {
    luri += '?';
    luri += query_string;
  }

  RestClient::Response res;
  res = resource_conn_->del(luri);

  cb(res);
  return res.code;
}

int chatterbox::head(const std::string &auth,
                     const std::string &uri,
                     const std::string &query_string,
                     const std::function <void (RestClient::Response &)> &cb)
{

  std::string luri("/");
  luri += uri;

  RestClient::HeaderFields reqHF;

  if(service_ == "s3") {
    if(auth == "aws_v2") {
      x_amz_date_ = build_v2_date();
      build_v2("HEAD", luri, reqHF);
    } else if(auth == "aws_v4") {
      x_amz_date_ = build_v4_date();
      build_v4("HEAD", luri, query_string, "", reqHF);
    }
  }

  resource_conn_->SetHeaders(reqHF);
  dump_hdr(reqHF);

  if(query_string != "") {
    luri += '?';
    luri += query_string;
  }

  RestClient::Response res;
  res = resource_conn_->head(luri);

  cb(res);
  return res.code;
}

std::optional<bool> chatterbox::eval_as_bool(Json::Value &from,
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

std::optional<uint32_t> chatterbox::eval_as_uint32_t(Json::Value &from,
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

std::optional<std::string> chatterbox::eval_as_string(Json::Value &from,
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
