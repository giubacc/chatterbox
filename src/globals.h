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
#define RAW_EVT_LOG_PATTERN "%v"

namespace utils {

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
  } else {
    return spdlog::level::level_enum::off;
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

}
