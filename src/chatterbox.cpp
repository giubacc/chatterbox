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
  access_key_ = conversation_ctx.get("access_key", "").asString();
  secret_key_ = conversation_ctx.get("secret_key", "").asString();
  signed_headers_ = conversation_ctx.get("signed_headers", "host;x-amz-content-sha256;x-amz-date").asString();
  region_ = conversation_ctx.get("region", "US").asString();

  //connection reset
  resource_conn_.reset(new RestClient::Connection(raw_host_));
  if(!resource_conn_) {
    log_->error("failed creating resource_conn_ object");
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
  //logger init
  std::shared_ptr<spdlog::logger> log;
  if(cfg_.log_type == "shell") {
    log = spdlog::stdout_color_mt("chatterbox");
  } else {
    log = spdlog::basic_logger_mt(cfg_.log_type, cfg_.log_type);
  }

  log_fmt_ = DEF_LOG_PATTERN;
  log->set_pattern(log_fmt_);
  log->set_level(utils::get_spdloglvl(cfg_.log_level.c_str()));
  spdlog::flush_every(std::chrono::seconds(2));
  log_ = log;

  int res = 0;
  if((res = js_env_.init())) {
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

  log_->trace("canonical_string:\n{}", os.str());
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

  log_->trace("v2 signature: {}", signature);

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
  log_->trace("date_key: {}", crypto::hex(date_key));

  auto date_region_key = crypto::hmac_sha256(date_key, region_);
  log_->trace("date_region_key: {}", crypto::hex(date_region_key));

  auto date_region_service_key = crypto::hmac_sha256(date_region_key, service_);
  log_->trace("date_region_service_key: {}", crypto::hex(date_region_service_key));

  auto signing_key = crypto::hmac_sha256(date_region_service_key, "aws4_request");
  log_->trace("signing_key: {}", crypto::hex(signing_key));

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

  log_->trace("canonical_request:\n{}", os.str());
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

  log_->trace("string_to_sign:\n{}", os.str());
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
  log_->trace("v4 signature: {}", signature);

  reqHF["Authorization"] = build_v4_authorization(signature);
}

void chatterbox::dump_hdr(const RestClient::HeaderFields &hdr) const
{
  std::for_each(hdr.begin(), hdr.end(), [&](auto it) {
    log_->trace("{}:{}", it.first, it.second);
  });
}

void chatterbox::poll()
{
  char file_path[PATH_MAX_LEN] = {0};
  DIR *dir;
  struct dirent *ent;
  int lerrno = 0;
  errno = 0;

  if((dir = opendir(cfg_.source_path.c_str())) != nullptr) {
    while((ent = readdir(dir)) != nullptr) {
      if(strcmp(ent->d_name,".") && strcmp(ent->d_name,"..")) {
        struct stat info;
        strncpy(file_path, cfg_.source_path.c_str(), sizeof(file_path)-1);
        strncat(file_path, "/", sizeof(file_path)-1);
        strncat(file_path, ent->d_name, sizeof(file_path)-1);
        stat(file_path, &info);
        if(!S_ISDIR(info.st_mode)) {
          if(!utils::ends_with(ent->d_name, ".json")) {
            continue;
          }
          log_->trace("processing scenario file:{}", ent->d_name);
          std::stringstream ss;
          int res = 0;
          if((res = utils::read_file(file_path, ss, log_.get()))) {
            log_->error("[read_file] {}, {}",
                        ent->d_name,
                        strerror(res));
            continue;
          }
          execute_scenario(ss);
          if(cfg_.move_scenario) {
            move_file(ent->d_name);
          } else {
            rm_file(file_path);
          }
        }
      }
    }
    if((lerrno = errno)) {
      log_->trace("[readdir] failure for location:{}, errno:{}, {}",
                  cfg_.source_path,
                  lerrno,
                  strerror(lerrno));
    }
    if(closedir(dir)) {
      lerrno = errno;
      log_->error("[closedir] failure for location:{}, errno:{}, {}",
                  cfg_.source_path,
                  lerrno,
                  strerror(lerrno));
    }
  } else {
    lerrno = errno;
    log_->critical("cannot open source location:{}, [opendir] errno:{}, {}",
                   cfg_.source_path,
                   lerrno,
                   strerror(lerrno));
  }
}

void chatterbox::move_file(const char *filename)
{
  std::ostringstream os_src, os_to;
  os_src << cfg_.source_path << "/" << filename;
  os_to << cfg_.source_path << "/" << "consumed" << "/" << filename;

  std::string to_fname_base(os_to.str());
  std::string to_fname(to_fname_base);

  int ntry = 0;
  while(rename(os_src.str().c_str(), to_fname.c_str()) && ntry++ < 10) {
    log_->error("moving to file_path: {}", to_fname_base.c_str());
    std::ostringstream os_to_retry;
    os_to_retry << to_fname_base << ".try" << ntry;
    to_fname = os_to_retry.str();
  }
}

void chatterbox::rm_file(const char *filename)
{
  if(unlink(filename)) {
    log_->error("[unlink] failure for removing:{}, errno:{}",
                filename,
                strerror(errno));
  }
}

std::string get_formatted_code(int code)
{
  std::ostringstream oss;
  oss << code;
  if(code >= 200 && code < 300) {
    return fmt::format(
             fmt::fg(fmt::terminal_color::green) |
             fmt::emphasis::bold,
             oss.str()
           );
  } else if(code >= 300 && code < 500) {
    return fmt::format(
             fmt::fg(fmt::terminal_color::yellow) |
             fmt::emphasis::bold,
             oss.str()
           );
  } else {
    return fmt::format(
             fmt::fg(fmt::terminal_color::red) |
             fmt::emphasis::bold,
             oss.str()
           );
  }
}

void chatterbox::dump_response(const std::string &body_res_format,
                               RestClient::Response &res)
{
  utils::scoped_log_fmt<chatterbox> slf(*this, RAW_LOG_PATTERN);

  if(res.body.empty()) {
    log_->info("\n{}\n\n{}\n", get_formatted_code(res.code), "{}");
  } else {
    if(body_res_format == "json") {
      try {
        Json::Value jres;
        std::istringstream istr(res.body);
        istr >> jres;
        log_->info("\n{}\n\n{}", get_formatted_code(res.code), jres.toStyledString());
      } catch(const Json::RuntimeError &) {
        log_->info("\n{}\n\n{}", get_formatted_code(res.code), res.body);
      }
    } else {
      log_->info("\n{}\n\n{}", get_formatted_code(res.code), res.body);
    }
  }
}

void chatterbox::execute_scenario(std::istream &is)
{
  Json::Value scenario;
  try {
    is >> scenario;
  } catch(Json::RuntimeError &e) {
    log_->error("malformed scenario:\n{}", e.what());
    return;
  }
  if(reset_scenario_ctx(scenario)) {
    log_->error("failed to reset scenario context");
    return;
  }

  Json::Value conversations = scenario["conversations"];

  //conversations cycle
  for(uint32_t i = 0; i < conversations.size(); ++i) {

    Json::Value conversation_ctx = conversations[i];
    if(reset_conversation_ctx(conversation_ctx)) {
      log_->error("failed to reset conversation context");
      return;
    }

    uint32_t talk_count = 0;
    Json::Value conversation = conversation_ctx["conversation"];

    //talks cycle
    for(uint32_t i = 0; i < conversation.size(); ++i) {

      //talk
      auto talk = conversation[i].get("talk", "");

      //for
      auto pfor = eval_as_uint32_t(conversation[i], "for", 0);
      if(!pfor) {
        log_->error("failed to read 'for' field");
        return;
      }
      talk_count += *pfor;

      for(uint32_t i = 0; i<pfor; ++i) {

        //verb
        auto verb = eval_as_string(talk, "verb");
        if(!verb) {
          log_->error("failed to read 'verb' field");
          return;
        }

        //uri
        auto uri = eval_as_string(talk, "uri", "");
        if(!uri) {
          log_->error("failed to read 'uri' field");
          return;
        }

        //query_string
        auto query_string = eval_as_string(talk, "query_string", "");
        if(!query_string) {
          log_->error("failed to read 'query_string' field");
          return;
        }

        //data
        auto data = eval_as_string(talk, "data", "");
        if(!data) {
          log_->error("failed to read 'data' field");
          return;
        }

        //body_res_dump
        auto body_res_dump = eval_as_bool(talk, "body_res_dump", true);
        if(!body_res_dump) {
          log_->error("failed to read 'body_res_dump' field");
          return;
        }

        //body_res_format
        auto body_res_format = eval_as_string(talk, "body_res_format", "");
        if(!body_res_format) {
          log_->error("failed to read 'body_res_format' field");
          return;
        }

        //optional auth directive
        auto auth = eval_as_string(talk, "auth", "v4");
        if(!auth) {
          log_->error("failed to read 'auth' field");
          return;
        }

        execute_talk(*verb,
                     *auth,
                     *uri,
                     *query_string,
                     *data,
                     *body_res_dump,
                     *body_res_format);
      }
    }
    {
      utils::scoped_log_fmt<chatterbox> slf(*this, RAW_LOG_PATTERN);
      log_->info("\ntalk count:{}\n", talk_count);
    }
  }
}

void chatterbox::execute_talk(const std::string &verb,
                              const std::string &auth,
                              const std::string &uri,
                              const std::string &query_string,
                              const std::string &data,
                              bool body_res_dump,
                              const std::string &body_res_format)
{
  if(verb == "GET") {
    get(auth, uri, query_string, [&](RestClient::Response &res) -> int {
      if(body_res_dump)
        dump_response(body_res_format, res);
      return 0;
    });
  } else if(verb == "POST") {
    post(auth, uri, query_string, data, [&](RestClient::Response &res) -> int {
      if(body_res_dump)
        dump_response(body_res_format, res);
      return 0;
    });
  } else if(verb == "PUT") {
    put(auth, uri, query_string, data, [&](RestClient::Response &res) -> int {
      if(body_res_dump)
        dump_response(body_res_format, res);
      return 0;
    });
  } else if(verb == "DELETE") {
    del(auth, uri, query_string, [&](RestClient::Response &res) -> int {
      if(body_res_dump)
        dump_response(body_res_format, res);
      return 0;
    });
  } else if(verb == "HEAD") {
    head(auth, uri, query_string, [&](RestClient::Response &res) -> int {
      if(body_res_dump)
        dump_response(body_res_format, res);
      return 0;
    });
  } else {
    log_->error("bad verb:{}", verb);
  }
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
    if(auth == "v2") {
      x_amz_date_ = build_v2_date();
      build_v2("POST", luri, reqHF);
    } else if(auth == "v4") {
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
    if(auth == "v2") {
      x_amz_date_ = build_v2_date();
      build_v2("PUT", luri, reqHF);
    } else if(auth == "v4") {
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
    if(auth == "v2") {
      x_amz_date_ = build_v2_date();
      build_v2("GET", luri, reqHF);
    } else if(auth == "v4") {
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
    if(auth == "v2") {
      x_amz_date_ = build_v2_date();
      build_v2("DELETE", luri, reqHF);
    } else if(auth == "v4") {
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
    if(auth == "v2") {
      x_amz_date_ = build_v2_date();
      build_v2("HEAD", luri, reqHF);
    } else if(auth == "v4") {
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
        log_->error("function name is not string type");
        return std::nullopt;
      }

      Json::Value j_args = val.get("args", Json::Value::null);
      std::vector<std::string> args;
      if(j_args) {
        if(!j_args.isArray()) {
          log_->error("function args are not array type");
          return std::nullopt;
        }
        for(uint32_t i = 0; i < j_args.size(); ++i) {
          Json::Value arg = j_args[i];
          if(!arg.isString()) {
            log_->error("function arg is not string type");
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
          log_->error("function result is not boolean type");
          return false;
        }
        result = res->BooleanValue(isl);
        return true;
      },
      error);

      if(!js_res) {
        log_->error("failure invoking function:{}, error:{}", function.asString(), error);
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
        log_->error("function name is not string type");
        return std::nullopt;
      }

      Json::Value j_args = val.get("args", Json::Value::null);
      std::vector<std::string> args;
      if(j_args) {
        if(!j_args.isArray()) {
          log_->error("function args are not array type");
          return std::nullopt;
        }
        for(uint32_t i = 0; i < j_args.size(); ++i) {
          Json::Value arg = j_args[i];
          if(!arg.isString()) {
            log_->error("function arg is not string type");
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
          log_->error("function result is not UInt type");
          return false;
        }
        result = res->Uint32Value(isl->GetCurrentContext()).FromMaybe(0);
        return true;
      },
      error);

      if(!js_res) {
        log_->error("failure invoking function:{}, error:{}", function.asString(), error);
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
        log_->error("function name is not string type");
        return std::nullopt;
      }

      Json::Value j_args = val.get("args", Json::Value::null);
      std::vector<std::string> args;
      if(j_args) {
        if(!j_args.isArray()) {
          log_->error("function args are not array type");
          return std::nullopt;
        }
        for(uint32_t i = 0; i < j_args.size(); ++i) {
          Json::Value arg = j_args[i];
          if(!arg.isString()) {
            log_->error("function arg is not string type");
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
          log_->error("function result is not String type");
          return false;
        }
        v8::String::Utf8Value utf_res(isl, res);
        result = *utf_res;
        return true;
      },
      error);

      if(!js_res) {
        log_->error("failure invoking function:{}, error:{}", function.asString(), error);
        return std::nullopt;
      }
      return result;
    }
  } else {
    return default_value;
  }
}

}
