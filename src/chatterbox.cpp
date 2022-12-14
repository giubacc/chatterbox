#include "chatterbox.h"
#include <algorithm>

const std::string algorithm = "AWS4-HMAC-SHA256";

namespace rest {

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

void chatterbox::build_v2(const char *method,
                          const std::string &uri,
                          RestClient::HeaderFields &reqHF) const
{
  reqHF["x-amz-date"] = x_amz_date_;

  auto signature = crypto::base64(crypto::hmac_sha1(access_key_,
                                                    build_v2_canonical_string(method,
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

void chatterbox::build_v4(const char *method,
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
                                                     build_v4_canonical_request(method,
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
          process_scenario(ent->d_name);
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

int chatterbox::process_scenario(const char *fname)
{
  int res = 0;
  event_log_->trace("processing scenario file:{}", fname);

  std::stringstream ss;
  std::ostringstream fpath;
  fpath << cfg_.in_scenario_path << "/" << fname;
  if((res = utils::read_file(fpath.str().c_str(), ss, event_log_.get()))) {
    event_log_->error("[read_file] {}", fname);
  } else {
    res = process_scenario(ss);
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

  conv_conn_->SetHeaders(reqHF);
  dump_hdr(reqHF);

  if(query_string != "") {
    luri += '?';
    luri += query_string;
  }

  RestClient::Response res;
  res = conv_conn_->post(luri, data);

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

  conv_conn_->SetHeaders(reqHF);
  dump_hdr(reqHF);

  if(query_string != "") {
    luri += '?';
    luri += query_string;
  }

  RestClient::Response res;
  res = conv_conn_->put(luri, data);

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

  conv_conn_->SetHeaders(reqHF);
  dump_hdr(reqHF);

  if(query_string != "") {
    luri += '?';
    luri += query_string;
  }

  RestClient::Response res;
  res = conv_conn_->get(luri);

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

  conv_conn_->SetHeaders(reqHF);
  dump_hdr(reqHF);

  if(query_string != "") {
    luri += '?';
    luri += query_string;
  }

  RestClient::Response res;
  res = conv_conn_->del(luri);

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

  conv_conn_->SetHeaders(reqHF);
  dump_hdr(reqHF);

  if(query_string != "") {
    luri += '?';
    luri += query_string;
  }

  RestClient::Response res;
  res = conv_conn_->head(luri);

  cb(res);
  return res.code;
}

}
