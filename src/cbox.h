#pragma once
#include "jsenv.h"

namespace cbox {

struct context {

  context(utils::cfg &cfg, Pistache::Http::ResponseWriter *rw = nullptr);

  int init(std::shared_ptr<spdlog::logger> &event_log);

  int load_document_by_file();
  int load_document_by_file(const char *fname);
  int load_document_by_string(const std::string &str);
  int load_document();

  int process_scenario();

  utils::cfg &cfg_;

  //ryml error handler
  utils::RymlErrorHandler REH_;

  //ryml document-in load buffer
  std::vector<char> ryml_load_buf_;

  //document-in
  ryml::Tree doc_in_;

  //scenario
  std::unique_ptr<cbox::scenario> scenario_;

  //output
  std::unique_ptr<std::ostream> output_;

  //endpoint-output
  Pistache::Http::ResponseWriter *response_writer_;

  //event log
  std::shared_ptr<spdlog::logger> event_log_;
};

struct env {

  env();
  ~env();

  int init();
  int exec();

  utils::cfg cfg_;

  //endpoint
  std::unique_ptr<rest::endpoint> endpoint_;

  //event log
  std::string event_log_fmt_;
  std::shared_ptr<spdlog::logger> event_log_;
};

}
