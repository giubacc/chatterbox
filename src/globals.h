#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <optional>
#include <ctime>
#include <dirent.h>
#define PATH_MAX_LEN 2048

#include "restclient-cpp/connection.h"
#include "restclient-cpp/restclient.h"
#include "json/json.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/fmt/bundled/color.h"

#define DEF_EVT_LOG_PATTERN "[%^%l%$]%v"
#define ASSERT_LOG_PATTERN "[%^ASSERT%$]%v"
#define RAW_EVT_LOG_PATTERN "%v"

namespace rest {
extern const std::string key_access_key;
extern const std::string key_auth;
extern const std::string key_body;
extern const std::string key_categorization;
extern const std::string key_code;
extern const std::string key_conversations;
extern const std::string key_data;
extern const std::string key_dump;
extern const std::string key_enabled;
extern const std::string key_for;
extern const std::string key_format;
extern const std::string key_header;
extern const std::string key_host;
extern const std::string key_method;
extern const std::string key_mock;
extern const std::string key_msec;
extern const std::string key_nsec;
extern const std::string key_on_begin;
extern const std::string key_on_end;
extern const std::string key_out;
extern const std::string key_query_string;
extern const std::string key_region;
extern const std::string key_requests;
extern const std::string key_response;
extern const std::string key_rtt;
extern const std::string key_sec;
extern const std::string key_secret_key;
extern const std::string key_service;
extern const std::string key_signed_headers;
extern const std::string key_stats;
extern const std::string key_uri;
extern const std::string key_usec;
}

namespace utils {

enum resolution {
  nanoseconds,
  microseconds,
  milliseconds,
  seconds
};

inline resolution from_literal(const std::string &str)
{
  if(str == rest::key_nsec) {
    return nanoseconds;
  } else if(str == rest::key_usec) {
    return microseconds;
  } else if(str == rest::key_msec) {
    return milliseconds;
  } else if(str == rest::key_sec) {
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

int read_file(const char *fname,
              std::stringstream &out,
              spdlog::logger *log);

struct json_value_ref {
  json_value_ref(Json::Value &ref) : ref_(ref) {}
  json_value_ref(const Json::Value &ref) : ref_(const_cast<Json::Value &>(ref)) {}
  Json::Value &ref_;
};

inline std::string get_formatted_string(const std::string &str,
                                        fmt::terminal_color color,
                                        fmt::emphasis emphasis)
{
  return fmt::format(fmt::fg(color) | emphasis, "{}", str);
}

}
