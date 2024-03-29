#include "utils.h"
#include "crypto.h"

const std::string algorithm = "AWS4-HMAC-SHA256";
namespace utils {

static std::unique_ptr<ryml::Tree> default_out_options;
const ryml::Tree &get_default_out_options()
{
  if(!default_out_options) {
    default_out_options.reset(new ryml::Tree);
    ryml::NodeRef root = default_out_options->rootref();
    root |= ryml::MAP;
    ryml::NodeRef default_out_dumps = root[key_dump];
    default_out_dumps |= ryml::MAP;
    default_out_dumps[key_out] << STR_FALSE;
    default_out_dumps[key_before] << STR_FALSE;
    default_out_dumps[key_after] << STR_FALSE;
    default_out_dumps[key_enabled] << STR_FALSE;
    ryml::NodeRef default_out_formats = root[key_format];
    default_out_formats |= ryml::MAP;
    default_out_formats[key_rtt] << key_msec;
  }
  return *default_out_options;
}

static std::unique_ptr<ryml::Tree> default_scenario_out_options;
const ryml::Tree &get_default_scenario_out_options()
{
  if(!default_scenario_out_options) {
    default_scenario_out_options.reset(new ryml::Tree);
    *default_scenario_out_options = get_default_out_options();
  }
  return *default_scenario_out_options;
}

static std::unique_ptr<ryml::Tree> default_conversation_out_options;
const ryml::Tree &get_default_conversation_out_options()
{
  if(!default_conversation_out_options) {
    default_conversation_out_options.reset(new ryml::Tree);
    *default_conversation_out_options = get_default_out_options();
  }
  return *default_conversation_out_options;
}

static std::unique_ptr<ryml::Tree> default_request_out_options;
const ryml::Tree &get_default_request_out_options()
{
  if(!default_request_out_options) {
    default_request_out_options.reset(new ryml::Tree);
    *default_request_out_options = get_default_out_options();
    ryml::NodeRef root = default_request_out_options->rootref();
    ryml::NodeRef request_out_dumps = root[key_dump];
    request_out_dumps[key_for] << STR_FALSE;
    request_out_dumps[key_mock] << STR_FALSE;
  }
  return *default_request_out_options;
}

static std::unique_ptr<ryml::Tree> default_response_out_options;
const ryml::Tree &get_default_response_out_options()
{
  if(!default_response_out_options) {
    default_response_out_options.reset(new ryml::Tree);
    *default_response_out_options = get_default_out_options();
    ryml::NodeRef root = default_response_out_options->rootref();
    ryml::NodeRef response_out_formats = root[key_format];
    response_out_formats[key_body] << STR_JSON;
  }
  return *default_response_out_options;
}

size_t file_get_contents(const char *filename,
                         std::vector<char> &v,
                         spdlog::logger *log,
                         int &error)
{
  ::FILE *fp = ::fopen(filename, "rb");
  if(fp == nullptr) {
    if(log) {
      log->error("{}:{}", filename, strerror(errno));
    }
    error = 1;
    return 0;
  }
  ::fseek(fp, 0, SEEK_END);
  long sz = ::ftell(fp);
  v.resize(sz);
  if(sz) {
    ::rewind(fp);
    ::fread(&(v)[0], 1, v.size(), fp);
  }
  ::fclose(fp);
  return v.size();
}

// ------------------
// --- RYML UTILS ---
// ------------------

void log_tree_node(ryml::ConstNodeRef node,
                   spdlog::logger &logger)
{
  std::stringstream ss;
  ss << node;
  logger.info("\n{}", ss.str());
}

void set_tree_node(ryml::Tree src_t,
                   ryml::ConstNodeRef src_n,
                   ryml::NodeRef to_n,
                   std::vector<char> &buf)
{
  ryml::csubstr n_ser = ryml::emit_yaml(src_t, src_n.id(), ryml::substr{}, false);
  buf.resize(n_ser.len);
  n_ser = ryml::emit_yaml(src_t, src_n.id(), ryml::to_substr(buf), true);
  ryml::parse_in_arena(n_ser, to_n);
}

// ------------
// --- AUTH ---
// ------------

int aws_auth::init(std::shared_ptr<spdlog::logger> &event_log)
{
  event_log_ = event_log;
  return 0;
}

void aws_auth::reset(const std::string &host,
                     const std::string &access_key,
                     const std::string &secret_key,
                     const std::string &service,
                     const std::string &signed_headers,
                     const std::string &region)
{
  host_ = host;
  access_key_ = access_key;
  secret_key_ = secret_key;
  service_ = service;
  signed_headers_ = signed_headers;
  region_ = region;
}

std::string aws_auth::aws_sign_v2_build_date()
{
  char buf[256];
  time_t now = time(0);
  struct tm tm = *gmtime(&now);
  strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);
  return buf;
}

std::string aws_auth::aws_sign_v4_build_date()
{
  time_t now;
  time(&now);
  char buf[sizeof "YYYYMMDDTHHMMSSZ"];
  strftime(buf, sizeof buf, "%Y%m%dT%H%M%SZ", gmtime(&now));
  return buf;
}

std::string aws_auth::aws_sign_v4_get_canonical_query_string(const std::optional<std::string> &query_string)
{
  if(!query_string) {
    return "";
  }
  std::ostringstream os;
  std::map<std::string, std::optional<std::string>> key_vals;

  std::size_t amp_it_start = 0, amp_it_end = 0, eq_it = 0;
  std::string key_val, key, val;
  bool end = false;
  while(!end) {
    amp_it_start = amp_it_end;
    if((amp_it_end = (*query_string).find("&", amp_it_end)) == std::string::npos) {
      amp_it_end = (*query_string).length();
      end = true;
    }

    key_val = (*query_string).substr(amp_it_start, (amp_it_end-amp_it_start));
    std::string key, val;
    if(((eq_it = key_val.find("=")) != std::string::npos) && (eq_it != key_val.length()-1)) {
      key_vals[key_val.substr(0, eq_it)] = key_val.substr(eq_it+1);
    } else if(!key_val.empty()) {
      key_vals[key_val] = std::nullopt;
    }
    ++amp_it_end;
  }

  auto it = key_vals.begin();
  do {
    os << it->first << '=' << (it->second ? *it->second : "");
    if(++it == key_vals.end()) {
      break;
    }
    os << '&';
  } while(true);
  return os.str();
}

std::string aws_auth::aws_sign_v2_build_authorization(const std::string &signature) const
{
  std::ostringstream os;
  os << "AWS "
     << access_key_ << ':'
     << signature;
  return os.str();
}

std::string aws_auth::aws_sign_v2_build_canonical_string(const std::string &method,
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

void aws_auth::aws_sign_v2_build(const char *method,
                                 const std::string &uri,
                                 RestClient::HeaderFields &reqHF) const
{
  reqHF["x-amz-date"] = x_amz_date_;

  auto signature = crypto::base64(crypto::hmac_sha1(access_key_,
                                                    aws_sign_v2_build_canonical_string(method,
                                                                                       uri)));

  event_log_->trace("v2 signature: {}", signature);

  reqHF["Authorization"] = aws_sign_v2_build_authorization(signature);
}

std::string aws_auth::aws_sign_v4_build_authorization(const std::string &signature) const
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

std::string aws_auth::aws_sign_v4_build_signing_key() const
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

std::string aws_auth::aws_sign_v4_build_canonical_headers(const std::string &x_amz_content_sha256) const
{
  std::ostringstream os;
  os << "host:" << host_ << std::endl
     << "x-amz-content-sha256:" << x_amz_content_sha256 << std::endl
     << "x-amz-date:" << x_amz_date_ << std::endl;
  return os.str();
}

std::string aws_auth::aws_sign_v4_build_canonical_request(const std::string &method,
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

std::string aws_auth::aws_sign_v4_build_string_to_sign(const std::string &canonical_request) const
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

void aws_auth::aws_sign_v4_build(const char *method,
                                 const std::string &uri,
                                 const std::optional<std::string> &query_string,
                                 const std::optional<std::string> &data,
                                 RestClient::HeaderFields &reqHF) const
{
  std::string x_amz_content_sha256 = crypto::hex(crypto::sha256(data ? *data : ""));

  reqHF["host"] = host_;
  reqHF["x-amz-date"] = x_amz_date_;
  reqHF["x-amz-content-sha256"] = x_amz_content_sha256;

  auto signature = crypto::hex(crypto::hmac_sha256(aws_sign_v4_build_signing_key(),
                                                   aws_sign_v4_build_string_to_sign(
                                                     aws_sign_v4_build_canonical_request(method,
                                                                                         uri,
                                                                                         aws_sign_v4_get_canonical_query_string(query_string),
                                                                                         aws_sign_v4_build_canonical_headers(x_amz_content_sha256),
                                                                                         x_amz_content_sha256))));
  event_log_->trace("v4 signature: {}", signature);

  reqHF["Authorization"] = aws_sign_v4_build_authorization(signature);
}

static const char *def_delims = " \t\n\r\f";

str_tok::str_tok(const std::string &str) :
  current_position_(0),
  max_position_((long)str.length()),
  new_position_(-1),
  str_(str),
  delimiters_(def_delims),
  ret_delims_(false),
  delims_changed_(false)
{
}

str_tok::~str_tok()
{}

long str_tok::skip_delimit(long start_pos)
{
  long position = start_pos;
  while(!ret_delims_ && position < max_position_) {
    if(delimiters_.find(str_.at(position)) == std::string::npos) {
      break;
    }
    position++;
  }
  return position;
}

long str_tok::scan_token(long start_pos, bool *is_delimit)
{
  long position = start_pos;
  while(position < max_position_) {
    if(delimiters_.find(str_.at(position)) != std::string::npos) {
      break;
    }
    position++;
  }
  if(ret_delims_ && (start_pos == position)) {
    if(delimiters_.find(str_.at(position)) != std::string::npos) {
      if(is_delimit) {
        *is_delimit = true;
      }
      position++;
    }
  } else if(is_delimit) {
    *is_delimit = false;
  }
  return position;
}

bool str_tok::next_token(std::string &out,
                         const char *delimiters,
                         bool return_delimiters,
                         bool *is_delimit)
{
  if(delimiters) {
    delimiters_.assign(delimiters);
    delims_changed_ = true;
  }
  ret_delims_ = return_delimiters;
  current_position_ = (new_position_ >= 0 && !delims_changed_) ?
                      new_position_ :
                      skip_delimit(current_position_);
  delims_changed_ = false;
  new_position_ = -1;
  if(current_position_ >= max_position_) {
    return false;
  }
  long start = current_position_;
  current_position_ = scan_token(current_position_, is_delimit);
  out = str_.substr(start, current_position_ - start);
  return true;
}

bool str_tok::has_more_tokens(bool return_delimiters)
{
  ret_delims_ = return_delimiters;
  new_position_ = skip_delimit(current_position_);
  return (new_position_ < max_position_);
}

}
