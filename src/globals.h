#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <chrono>
#include <optional>
#include <ctime>
#include <dirent.h>
#define PATH_MAX_LEN 2048

#include "restclient-cpp/connection.h"
#include "restclient-cpp/restclient.h"
#include "ryml.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/fmt/bundled/color.h"

#define DEF_EVT_LOG_PATTERN "[%^%l%$]%v"
#define ASSERT_LOG_PATTERN  "[%^ASSERT%$]%v"
#define RAW_EVT_LOG_PATTERN "%v"

#define key_access_key      "accessKey"
#define key_auth            "auth"
#define key_body            "body"
#define key_categorization  "categorization"
#define key_code            "code"
#define key_conversations   "conversations"
#define key_data            "data"
#define key_dump            "dump"
#define key_enabled         "enabled"
#define key_for             "for"
#define key_format          "format"
#define key_headers         "headers"
#define key_host            "host"
#define key_method          "method"
#define key_mock            "mock"
#define key_msec            "msec"
#define key_nsec            "nsec"
#define key_will            "will"
#define key_did             "did"
#define key_out             "out"
#define key_query_string    "queryString"
#define key_region          "region"
#define key_requests        "requests"
#define key_response        "response"
#define key_rtt             "rtt"
#define key_sec             "sec"
#define key_secret_key      "secretKey"
#define key_service         "service"
#define key_signed_headers  "signedHeaders"
#define key_stats           "stats"
#define key_uri             "uri"
#define key_usec            "usec"

#define STR_TRUE            "true"
#define STR_FALSE           "false"
#define STR_JSON            "json"
#define STR_YAML            "yaml"

namespace utils {

enum resolution {
  nanoseconds,
  microseconds,
  milliseconds,
  seconds
};

inline resolution from_literal(const std::string &str)
{
  if(str == key_nsec) {
    return nanoseconds;
  } else if(str == key_usec) {
    return microseconds;
  } else if(str == key_msec) {
    return milliseconds;
  } else if(str == key_sec) {
    return seconds;
  }
  return nanoseconds;
}

inline int64_t from_nano(int64_t nanoseconds, resolution to)
{
  switch(to) {
    case utils::resolution::nanoseconds:
      return nanoseconds;
    case utils::resolution::microseconds:
      return nanoseconds/(int64_t)1000;
    case utils::resolution::milliseconds:
      return nanoseconds/(int64_t)1000000;
    case utils::resolution::seconds:
      return nanoseconds/(int64_t)1000000000;
    default:
      return -1;
  }
}

inline spdlog::level::level_enum get_spdloglvl(const char *log_level)
{
  if(!strcmp("trc", log_level)) {
    return spdlog::level::level_enum::trace;
  } else if(!strcmp("dbg", log_level)) {
    return spdlog::level::level_enum::debug;
  } else if(!strcmp("inf", log_level)) {
    return spdlog::level::level_enum::info;
  } else if(!strcmp("wrn", log_level)) {
    return spdlog::level::level_enum::warn;
  } else if(!strcmp("err", log_level)) {
    return spdlog::level::level_enum::err;
  } else if(!strcmp("cri", log_level)) {
    return spdlog::level::level_enum::critical;
  } else if(!strcmp("off", log_level)) {
    return spdlog::level::level_enum::off;
  } else {
    return spdlog::level::level_enum::err;
  }
}

inline spdlog::level::level_enum get_spdloglvl(uint32_t log_level)
{
  if(log_level >= spdlog::level::level_enum::off) {
    return spdlog::level::level_enum::off;
  }
  return (spdlog::level::level_enum)log_level;
}

template<typename T>
struct scoped_log_fmt {
  T &p_;

  scoped_log_fmt(T &p, const std::string &scoped_log_fmt) : p_(p) {
    p_.event_log_->set_pattern(scoped_log_fmt);
  }
  ~scoped_log_fmt() {
    p_.event_log_->set_pattern(p_.event_log_fmt_);
  }
};

inline std::string &ltrim(std::string &str, const std::string &chars = "\t\n\v\f\r ")
{
  str.erase(0, str.find_first_not_of(chars));
  return str;
}

inline std::string &rtrim(std::string &str, const std::string &chars = "\t\n\v\f\r ")
{
  str.erase(str.find_last_not_of(chars) + 1);
  return str;
}

inline std::string &trim(std::string &str, const std::string &chars = "\t\n\v\f\r ")
{
  return ltrim(rtrim(str, chars), chars);
}

inline std::string &find_and_replace(std::string &str, const char *find, const char *replace)
{
  size_t f_len = strlen(find), r_len = strlen(replace);
  for(std::string::size_type i = 0; (i = str.find(find, i)) != std::string::npos;) {
    str.replace(i, f_len, replace);
    i += r_len;
  }
  return str;
}

inline bool ends_with(const std::string &str, const std::string &match)
{
  if(str.size() >= match.size() &&
      str.compare(str.size() - match.size(), match.size(), match) == 0) {
    return true;
  } else {
    return false;
  }
}

inline void base_name(const std::string &input,
                      std::string &base_path,
                      std::string &file_name)
{
  if(input.find("/") != std::string::npos) {
    base_path = input.substr(0, input.find_last_of("/"));
    file_name = input.substr(input.find_last_of("/") + 1);
  } else {
    base_path = ".";
    file_name = input;
  }
}

size_t file_get_contents(const char *filename,
                         std::vector<char> &v,
                         spdlog::logger *log);

inline std::string get_formatted_string(const std::string &str,
                                        fmt::terminal_color color,
                                        fmt::emphasis emphasis)
{
  return fmt::format(fmt::fg(color) | emphasis, "{}", str);
}

// ------------------
// --- RYML UTILS ---
// ------------------

struct RymlErrorHandler {
  // this will be called on error
  void on_error(const char *msg,
                size_t len,
                ryml::Location loc) {
    throw std::runtime_error(ryml::formatrs<std::string>("{}:{}:{} ({}B) {}",
                                                         loc.name,
                                                         loc.line,
                                                         loc.col,
                                                         loc.offset,
                                                         ryml::csubstr(msg, len)));
  }

  // bridge
  ryml::Callbacks callbacks() {
    return ryml::Callbacks(this, nullptr, nullptr, RymlErrorHandler::s_error);
  }

  static void s_error(const char *msg,
                      size_t len,
                      ryml::Location loc,
                      void *this_) {
    return ((RymlErrorHandler *)this_)->on_error(msg, len, loc);
  }

  // checking
  template<typename Fn, typename Err>
  void check_error_occurs(Fn &&fn, Err &&err) const {
    try {
      fn();
    } catch(std::runtime_error const &e) {
      err(e);
    }
  }

  RymlErrorHandler() : defaults(ryml::get_callbacks()) {}
  ryml::Callbacks defaults;
};

void log_tree_node(ryml::ConstNodeRef node,
                   spdlog::logger &logger);

void set_tree_node(ryml::Tree src_t,
                   ryml::ConstNodeRef src_n,
                   ryml::NodeRef to_n,
                   std::vector<char> &buf);

// ------------
// --- AUTH ---
// ------------

struct aws_auth {

    int init(std::shared_ptr<spdlog::logger> &event_log);

    void reset(const std::string &host,
               const std::string &access_key,
               const std::string &secret_key,
               const std::string &service,
               const std::string &signed_headers,
               const std::string &region);

    // AWS Signature Version 2

    static std::string aws_sign_v2_build_date();

    std::string aws_sign_v2_build_canonical_string(const std::string &method,
                                                   const std::string &canonical_uri) const;

    std::string aws_sign_v2_build_authorization(const std::string &signature) const;

    void aws_sign_v2_build(const char *method,
                           const std::string &uri,
                           RestClient::HeaderFields &reqHF) const;

    // AWS Signature Version 4

    static std::string aws_sign_v4_build_date();
    static std::string aws_sign_v4_get_canonical_query_string(const std::string &query_string);

    std::string aws_sign_v4_build_signing_key() const;
    std::string aws_sign_v4_build_canonical_headers(const std::string &x_amz_content_sha256) const;

    std::string aws_sign_v4_build_canonical_request(const std::string &method,
                                                    const std::string &canonical_uri,
                                                    const std::string &canonical_query_string,
                                                    const std::string &canonical_headers,
                                                    const std::string &payload_hash) const;

    std::string aws_sign_v4_build_string_to_sign(const std::string &canonical_request) const;
    std::string aws_sign_v4_build_authorization(const std::string &signature) const;

    void aws_sign_v4_build(const char *method,
                           const std::string &uri,
                           const std::string &query_string,
                           const std::string &data,
                           RestClient::HeaderFields &reqHF) const;

    std::string host_;
    std::string access_key_;
    std::string secret_key_;
    std::string service_;
    std::string x_amz_date_;
    std::string signed_headers_;
    std::string region_;

  public:
    //event logger
    std::shared_ptr<spdlog::logger> event_log_;
};

}
