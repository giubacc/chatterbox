#include "endpoint.h"

namespace cbox {

// ---------------
// --- context ---
// ---------------

context::context(utils::cfg &cfg) :
  cfg_(cfg) {}

int context::init(std::shared_ptr<spdlog::logger> &event_log)
{
  event_log_ = event_log;
  int res = 0;

  //output init
  if(!cfg_.daemon) {
    if(cfg_.out_channel == "stdout") {
      output_.reset(new std::ostream(std::cout.rdbuf()));
    } else if(cfg_.out_channel == "stderr") {
      output_.reset(new std::ostream(std::cerr.rdbuf()));
    } else {
      output_.reset(new std::ofstream(cfg_.out_channel));
    }
  }

  //init scenario
  scenario_.reset(new scenario(*this));
  res = scenario_->init(event_log_);

  return res;
}

int context::load_document_by_file()
{
  std::string file_name;
  if(cfg_.in_path.empty()) {
    utils::base_name(cfg_.in_name,
                     cfg_.in_path,
                     file_name);
  } else {
    file_name = cfg_.in_name;
  }
  return load_document_by_file(file_name.c_str());
}

int context::load_document_by_file(const char *fname)
{
  int res = 0;
  event_log_->trace("loading document {}", fname);

  std::ostringstream fpath;
  fpath << cfg_.in_path << "/" << fname;

  size_t sz;
  if((sz = utils::file_get_contents(fpath.str().c_str(), ryml_load_buf_, event_log_.get()))) {
    res = load_document();
  } else {
    res = 1;
  }

  return res;
}

int context::load_document_by_string(const std::string &str)
{
  ryml_load_buf_.resize(str.size());
  std::memcpy(ryml_load_buf_.data(), str.data(), str.size());
  return load_document();
}

int context::load_document()
{
  int res = 0;
  ryml::set_callbacks(REH_.callbacks());
  REH_.check_error_occurs([&] {
    doc_in_ = ryml::parse_in_place(ryml::to_substr(ryml_load_buf_));
  }, [&](std::runtime_error const &e) {
    event_log_->error("malformed document\n{}", e.what());
    res = 1;
  });
  ryml::set_callbacks(REH_.defaults);
  return res;
}

int context::process_scenario()
{
  int res = 0;
  ryml::NodeRef stream = doc_in_.rootref();
  if(stream.is_stream()) {
    for(ryml::NodeRef doc : stream.children()) {
      if((res = scenario_->process(doc_in_, doc))) {
        return res;
      }
    }
  } else if(stream.is_doc()) {
    if((res = scenario_->process(doc_in_, doc_in_.rootref()))) {
      return res;
    }
  } else {
    event_log_->info("empty document");
  }
  //end of scenarios
  return -1;
}

// -----------
// --- env ---
// -----------

env::env()
{}

env::~env()
{
  event_log_.reset();
}

int env::init()
{
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

  //rest endpoint init
  if(cfg_.daemon) {
    endpoint_.reset(new rest::endpoint(*this));
    endpoint_->init(event_log_);
  }

  return 0;
}

int env::exec()
{
  int res = 0;

  if(cfg_.daemon) {
    res = endpoint_->start();
  } else {
    context ctx(cfg_);
    if((res = ctx.init(event_log_))) {
      return res;
    }
    if((res = ctx.load_document_by_file())) {
      return res;
    }
    while(!(res = ctx.process_scenario()));

    //a negative value means no more scenarios
    if(res<0) {
      res = 0;
    }
  }

  return res;
}

}
