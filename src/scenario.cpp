#include "scenario.h"

namespace cbox {

const std::string key_access_key = "access_key";
const std::string key_auth = "auth";
const std::string key_body = "body";
const std::string key_categorization = "categorization";
const std::string key_code = "code";
const std::string key_conversations = "conversations";
const std::string key_data = "data";
const std::string key_dump = "dump";
const std::string key_enabled = "enabled";
const std::string key_for = "for";
const std::string key_format = "format";
const std::string key_header = "header";
const std::string key_host = "host";
const std::string key_method = "method";
const std::string key_mock = "mock";
const std::string key_msec = "msec";
const std::string key_nsec = "nsec";
const std::string key_on_begin = "on_begin";
const std::string key_on_end = "on_end";
const std::string key_out = "out";
const std::string key_query_string = "query_string";
const std::string key_region = "region";
const std::string key_requests = "requests";
const std::string key_response = "response";
const std::string key_rtt = "rtt";
const std::string key_sec = "sec";
const std::string key_secret_key = "secret_key";
const std::string key_service = "service";
const std::string key_signed_headers = "signed_headers";
const std::string key_stats = "stats";
const std::string key_uri = "uri";
const std::string key_usec = "usec";

static Json::Value default_out_options;
const Json::Value &scenario::get_default_out_options()
{
  if(default_out_options.empty()) {
    auto &default_out_dumps = default_out_options[key_dump];
    default_out_dumps[key_out] = false;
    default_out_dumps[key_on_begin] = false;
    default_out_dumps[key_on_end] = false;
    default_out_dumps[key_enabled] = false;
    auto &default_out_formats = default_out_options[key_format];
    default_out_formats[key_rtt] = key_msec;
  }
  return default_out_options;
}

static Json::Value default_scenario_out_options;
const Json::Value &scenario::get_default_scenario_out_options()
{
  if(default_scenario_out_options.empty()) {
    default_scenario_out_options = get_default_out_options();
  }
  return default_scenario_out_options;
}

static Json::Value default_conversation_out_options;
const Json::Value &scenario::get_default_conversation_out_options()
{
  if(default_conversation_out_options.empty()) {
    default_conversation_out_options = get_default_out_options();
    // auto &default_out_dumps = default_conversation_out_options[key_dump];
  }
  return default_conversation_out_options;
}

static Json::Value default_request_out_options;
const Json::Value &scenario::get_default_request_out_options()
{
  if(default_request_out_options.empty()) {
    default_request_out_options = get_default_out_options();
    auto &default_out_dumps = default_request_out_options[key_dump];
    default_out_dumps[key_auth] = false;
    default_out_dumps[key_for] = false;
    default_out_dumps[key_mock] = false;
  }
  return default_request_out_options;
}

static Json::Value default_response_out_options;
const Json::Value &scenario::get_default_response_out_options()
{
  if(default_response_out_options.empty()) {
    default_response_out_options = get_default_out_options();
    auto &response_out_formats = default_response_out_options[key_format];
    response_out_formats[key_body] = "json";
  }
  return default_response_out_options;
}

// -------------------
// --- STACK SCOPE ---
// -------------------

// *INDENT-OFF*
scenario::stack_scope::stack_scope(scenario &parent,
                                     Json::Value &obj_in,
                                     Json::Value &obj_out,
                                     bool &error,
                                     const Json::Value &default_out_options,
                                     bool call_dump_val_cb,
                                     const std::function<void(const std::string &key, bool val)> &dump_val_cb,
                                     bool call_format_val_cb,
                                     const std::function<void(const std::string &key, const std::string &val)> &format_val_cb)
// *INDENT-ON*
  :
  parent_(parent),
  obj_in_(obj_in),
  obj_out_(obj_out),
  default_out_options_(default_out_options),
  error_(error),
  enabled_(false),
  commit_(false)
{
  if(!push_out_opts(default_out_options,
                    call_dump_val_cb,
                    dump_val_cb,
                    call_format_val_cb,
                    format_val_cb)) {
    parent_.event_log_->error("failed to push out-options for the current stack scope");
    error_ = true;
  } else {
    //on_begin handler
    if(!parent_.js_env_.exec_as_function(obj_out_, key_on_begin.c_str())) {
      parent_.event_log_->error("failed to execute on_begin handler for the current stack scope");
      error_ = true;
    }
  }

  if(parent_.assert_failure_) {
    error_ = true;
    return;
  }

  auto enabled_opt = parent_.js_env_.eval_as<bool>(obj_in, key_enabled.c_str(), true);
  if(!enabled_opt) {
    parent_.event_log_->error("failed to read 'enabled' field");
    error_ = true;
  } else {
    enabled_ = *enabled_opt;
  }
}

scenario::stack_scope::~stack_scope()
{
  if(commit_) {
    //on_end handler
    if(!parent_.js_env_.exec_as_function(obj_out_, key_on_end.c_str())) {
      parent_.event_log_->error("failed to execute on_end handler for the current stack scope");
      error_ = true;
    } else {
      if(!pop_process_out_opts()) {
        parent_.event_log_->error("failed to pop-process out-options for the current stack scope");
        error_ = true;
      }
    }
  }

  if(parent_.assert_failure_) {
    error_ = true;
  }
}

bool scenario::stack_scope::push_out_opts(const Json::Value &default_out_options,
                                          bool call_dump_val_cb,
                                          const std::function<void(const std::string &key, bool val)> &dump_val_cb,
                                          bool call_format_val_cb,
                                          const std::function<void(const std::string &key, const std::string &val)> &format_val_cb)
{
  bool res = true;

  Json::Value *out_node_ptr = nullptr;
  if((out_node_ptr = const_cast<Json::Value *>(obj_out_.find(key_out.data(), key_out.data()+key_out.length())))) {
    Json::Value out_options = default_out_options;
    {
      Json::Value *dump_node = nullptr;
      if((dump_node = const_cast<Json::Value *>(out_node_ptr->find(key_dump.data(), key_dump.data()+key_dump.length())))) {
        Json::Value::Members dn_keys = dump_node->getMemberNames();
        std::for_each(dn_keys.begin(), dn_keys.end(), [&](const Json::String &it) {
          const Json::Value &val = (*dump_node)[it];
          if(!val.isBool()) {
            res = false;
          }
          auto &default_out_dumps = out_options[key_dump];
          if(default_out_dumps.find(it.data(), it.data()+it.length())) {
            default_out_dumps.removeMember(it);
          }
        });
        auto &default_out_dumps = out_options[key_dump];
        Json::Value::Members ddn_keys = default_out_dumps.getMemberNames();
        std::for_each(ddn_keys.begin(), ddn_keys.end(), [&](const Json::String &it) {
          (*dump_node)[it] = default_out_dumps[it];
        });
      } else {
        (*out_node_ptr)[key_dump] = out_options[key_dump];
      }
    }

    {
      Json::Value *format_node = nullptr;
      if((format_node = const_cast<Json::Value *>(out_node_ptr->find(key_format.data(), key_format.data()+key_format.length())))) {
        Json::Value::Members fn_keys = format_node->getMemberNames();
        std::for_each(fn_keys.begin(), fn_keys.end(), [&](const Json::String &it) {
          const Json::Value &val = (*format_node)[it];
          if(!val.isString()) {
            res = false;
          }
          auto &default_out_formats = out_options[key_format];
          if(default_out_formats.find(it.data(), it.data()+it.length())) {
            default_out_formats.removeMember(it);
          }
        });
        auto &default_out_formats = out_options[key_format];
        Json::Value::Members dfn_keys = default_out_formats.getMemberNames();
        std::for_each(dfn_keys.begin(), dfn_keys.end(), [&](const Json::String &it) {
          (*format_node)[it] = default_out_formats[it];
        });
      } else {
        (*out_node_ptr)[key_format] = out_options[key_format];
      }
    }
    out_options_ = *out_node_ptr;
  } else {
    out_options_ = default_out_options;
  }

  if(call_dump_val_cb) {
    Json::Value &dump_node = out_options_[key_dump];
    Json::Value::Members dn_keys = dump_node.getMemberNames();
    std::for_each(dn_keys.begin(), dn_keys.end(), [&](const Json::String &it) {
      dump_val_cb(it, dump_node[it].asBool());
    });
  }
  if(call_format_val_cb) {
    Json::Value &format_node = out_options_[key_format];
    Json::Value::Members fn_keys = format_node.getMemberNames();
    std::for_each(fn_keys.begin(), fn_keys.end(), [&](const Json::String &it) {
      format_val_cb(it, format_node[it].asString());
    });
  }

  return res;
}

bool scenario::stack_scope::pop_process_out_opts()
{
  bool res = true;
  const Json::Value *dump_node = nullptr;
  if((dump_node = out_options_.find(key_dump.data(), key_dump.data()+key_dump.length()))) {
    Json::Value::Members keys = dump_node->getMemberNames();
    if(!keys.empty()) {
      std::for_each(keys.begin(), keys.end(), [&](const Json::String &it) {
        const Json::Value &val = (*dump_node)[it];
        if(!val.asBool()) {
          obj_out_.removeMember(it);
        }
      });
    }
  }
  return res;
}

// -------------
// --- STATS ---
// -------------

void scenario::statistics::reset()
{
  conversation_count_ = 0;
  request_count_ = 0;
  categorization_.clear();
}

void scenario::statistics::incr_conversation_count()
{
  ++conversation_count_;
}

void scenario::statistics::incr_request_count()
{
  ++request_count_;
}

void scenario::statistics::incr_categorization(const std::string &key)
{
  auto value = categorization_[key];
  categorization_[key] = ++value;
}

// ----------------
// --- SCENARIO ---
// ----------------

scenario::scenario() :
  stats_(*this),
  js_env_(*this)
{}

scenario::~scenario()
{
  event_log_.reset();
}

int scenario::init(int argc, const char *argv[])
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

void scenario::poll()
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
          process(ent->d_name);
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

void scenario::move_file(const char *filename)
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

void scenario::rm_file(const char *filename)
{
  std::ostringstream fpath;
  fpath << cfg_.in_scenario_path << "/" << filename;
  if(unlink(fpath.str().c_str())) {
    event_log_->error("unlink: {}", strerror(errno));
  }
}

int scenario::process()
{
  std::string file_name;
  if(cfg_.in_scenario_path.empty()) {
    utils::base_name(cfg_.in_scenario_name,
                     cfg_.in_scenario_path,
                     file_name);
  } else {
    file_name = cfg_.in_scenario_name;
  }
  return process(file_name.c_str());
}

int scenario::process(const char *fname)
{
  int res = 0;
  event_log_->trace("processing scenario file:{}", fname);

  std::stringstream ss;
  std::ostringstream fpath;
  fpath << cfg_.in_scenario_path << "/" << fname;
  if(!(res = utils::read_file(fpath.str().c_str(), ss, event_log_.get()))) {
    res = process(ss);
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

int scenario::reset()
{
  assert_failure_ = false;

  // reset stats
  stats_.reset();

  int res = js_env_.renew_scenario_context();

  //initialize scenario-out
  scenario_out_ = scenario_in_;

  return res;
}

int scenario::process(std::istream &is)
{
  int res = 0;

  try {
    is >> scenario_in_;
  } catch(Json::RuntimeError &e) {
    event_log_->error("malformed scenario:\n{}", e.what());
    return 1;
  }
  if((res = reset())) {
    event_log_->error("failed to reset scenario");
    return res;
  }

  bool error = false;
  {
    stack_scope scope(*this,
                      scenario_in_,
                      scenario_out_,
                      error,
                      get_default_scenario_out_options());
    if(error) {
      goto fend;
    }

    if(scope.enabled_) {
      //conversations-in
      Json::Value &conversations_in = scenario_in_[key_conversations];
      if(!conversations_in.isArray()) {
        event_log_->error("conversations field is not an array");
        goto fend;
      }

      Json::Value &conversations_out = scenario_out_[key_conversations];

      // conversations cycle
      uint32_t conv_it = 0;
      while(true) {
        if(!conversations_in.isValidIndex(conv_it)) {
          break;
        }

        /*TODO parallel handling*/
        conversation conv(*this);

        if((res = conv.process(conversations_in[conv_it],
                               conversations_out[conv_it]))) {
          goto fend;
        }
        ++conv_it;
      }

      enrich_with_stats(scenario_out_);
      scope.commit();
    } else {
      scenario_out_.clear();
      scenario_out_[key_enabled] = false;
    }
  }

fend:
  // finally write scenario_out on the output
  if(!cfg_.no_out_) {
    output_->info("{}", scenario_out_.toStyledString());
  }
  return res;
}

void scenario::enrich_with_stats(Json::Value &scenario_out)
{
  Json::Value &statistics = scenario_out[key_stats];
  statistics[key_conversations] = stats_.conversation_count_;
  statistics[key_requests] = stats_.request_count_;
  std::for_each(stats_.categorization_.begin(),
  stats_.categorization_.end(), [&](auto it) {
    Json::Value &res_code_categorization = statistics[key_categorization];
    res_code_categorization[it.first] = it.second;
  });
}

}
