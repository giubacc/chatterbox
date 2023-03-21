#include "conversation.h"

#define ERR_PUSH_OUT_OPTS       "failed to push the out options in the current scope"
#define ERR_POP_OUT_OPTS        "failed to process the out options in the current scope"
#define ERR_EXEC_BEF_HNDL       "failed to execute the before handler in the current scope"
#define ERR_EXEC_AFT_HNDL       "failed to execute the after handler in the current scope"
#define ERR_CONV_NOT_SEQ        "'conversations' is not a sequence"
#define ERR_FAIL_READ_ENABLED   "failed to read 'enabled'"

namespace cbox {

// -------------------
// --- STACK SCOPE ---
// -------------------

// *INDENT-OFF*
scenario::stack_scope::stack_scope(scenario &parent,
                                   ryml::NodeRef obj_in,
                                   ryml::NodeRef obj_out,
                                   bool &error,
                                   const ryml::Tree &default_out_options,
                                   bool call_dump_val_cb,
                                   const std::function<void(const std::string &key, bool val)> &dump_val_cb,
                                   bool call_format_val_cb,
                                   const std::function<void(const std::string &key, const std::string &val)> &format_val_cb)
// *INDENT-ON*
  :
  parent_(parent),
  obj_in_(obj_in),
  obj_out_(obj_out),
  error_(error),
  enabled_(false),
  commit_(false)
{
  if(!push_out_opts(default_out_options,
                    call_dump_val_cb,
                    dump_val_cb,
                    call_format_val_cb,
                    format_val_cb)) {
    parent_.event_log_->error(ERR_PUSH_OUT_OPTS);
    utils::clear_map_node_put_key_val(obj_out_, key_error, ERR_PUSH_OUT_OPTS);
    error_ = true;
  } else {
    if(!parent_.js_env_.exec_as_function(obj_out_, key_before)) {
      parent_.event_log_->error(ERR_EXEC_BEF_HNDL);
      utils::clear_map_node_put_key_val(obj_out_, key_error, ERR_EXEC_BEF_HNDL);
      error_ = true;
    }
  }

  if(parent_.assert_failure_) {
    error_ = true;
    return;
  }

  if(obj_in_.valid()) {
    auto enabled_opt = parent_.js_env_.eval_as<bool>(obj_in_, key_enabled, true);
    if(!enabled_opt) {
      parent_.event_log_->error(ERR_FAIL_READ_ENABLED);
      utils::clear_map_node_put_key_val(obj_out_, key_error, ERR_FAIL_READ_ENABLED);
      error_ = true;
    } else {
      enabled_ = *enabled_opt;
    }
  }
}

scenario::stack_scope::~stack_scope()
{
  if(commit_) {
    //on_end handler
    if(!parent_.js_env_.exec_as_function(obj_out_, key_after)) {
      parent_.event_log_->error(ERR_EXEC_AFT_HNDL);
      utils::clear_map_node_put_key_val(obj_out_, key_error, ERR_EXEC_AFT_HNDL);
      error_ = true;
    }
  }

  if(!pop_process_out_opts()) {
    parent_.event_log_->error(ERR_POP_OUT_OPTS);
    utils::clear_map_node_put_key_val(obj_out_, key_error, ERR_POP_OUT_OPTS);
    error_ = true;
  }

  if(parent_.assert_failure_) {
    error_ = true;
  }
}

bool scenario::stack_scope::push_out_opts(const ryml::Tree &default_out_options,
                                          bool call_dump_val_cb,
                                          const std::function<void(const std::string &key, bool val)> &dump_val_cb,
                                          bool call_format_val_cb,
                                          const std::function<void(const std::string &key, const std::string &val)> &format_val_cb)
{
  bool res = true;

  out_opts_ = default_out_options;
  ryml::NodeRef out_opts_root = out_opts_.rootref();
  ryml::NodeRef out_node;

  if(obj_out_.has_child(key_out)) {
    out_node = obj_out_[key_out];

    if(out_node.has_child(key_dump)) {
      ryml::NodeRef out_opts_dump_node = out_opts_root[key_dump];
      ryml::NodeRef dump_node = out_node[key_dump];
      for(ryml::ConstNodeRef const &dn_c : dump_node.children()) {
        if(out_opts_dump_node.has_child(dn_c.key())) {
          out_opts_dump_node.remove_child(dn_c.key());
        }
      }
      for(ryml::ConstNodeRef const &dn_c : out_opts_dump_node.children()) {
        if(out_opts_dump_node.has_child(dn_c.key())) {
          dump_node[dn_c.key()] << dn_c.val();
        }
      }
    } else {
      ryml::NodeRef dump_node = out_node[key_dump];
      utils::set_tree_node(out_opts_,
                           out_opts_root[key_dump],
                           dump_node,
                           parent_.ryml_scenario_out_buf_);
    }

    if(out_node.has_child(key_format)) {
      ryml::NodeRef out_opts_format_node = out_opts_root[key_format];
      ryml::NodeRef format_node = out_node[key_format];
      for(ryml::ConstNodeRef const &dn_c : format_node.children()) {
        if(out_opts_format_node.has_child(dn_c.key())) {
          out_opts_format_node.remove_child(dn_c.key());
        }
      }
      for(ryml::ConstNodeRef const &dn_c : out_opts_format_node.children()) {
        if(out_opts_format_node.has_child(dn_c.key())) {
          format_node[dn_c.key()] << dn_c.val();
        }
      }
    } else {
      ryml::NodeRef format_node = out_node[key_format];
      utils::set_tree_node(out_opts_,
                           out_opts_root[key_format],
                           format_node,
                           parent_.ryml_scenario_out_buf_);
    }

  } else {
    out_node = obj_out_[key_out];
    out_node |= ryml::MAP;
    utils::set_tree_node(out_opts_,
                         out_opts_root,
                         out_node,
                         parent_.ryml_scenario_out_buf_);
  }

  out_opts_root.clear();
  out_opts_root |= ryml::MAP;

  utils::set_tree_node(*out_node.tree(),
                       out_node[key_dump],
                       out_opts_root,
                       parent_.ryml_scenario_out_buf_);

  utils::set_tree_node(*out_node.tree(),
                       out_node[key_format],
                       out_opts_root,
                       parent_.ryml_scenario_out_buf_);

  if(call_dump_val_cb) {
    ryml::NodeRef dump_node = out_node[key_dump];
    for(ryml::ConstNodeRef const &dn_c : dump_node.children()) {
      std::ostringstream os;
      os << dn_c.key();
      std::string val;
      dn_c >> val;
      dump_val_cb(os.str(), (val == STR_TRUE));
    }
  }
  if(call_format_val_cb) {
    ryml::NodeRef format_node = out_node[key_format];
    for(ryml::ConstNodeRef const &dn_c : format_node.children()) {
      std::ostringstream os;
      os << dn_c.key();
      std::string val;
      dn_c >> val;
      format_val_cb(os.str(), val);
    }
  }

  return res;
}

bool scenario::stack_scope::pop_process_out_opts()
{
  bool res = true;
  ryml::NodeRef out_opts_root = out_opts_.rootref();
  if(out_opts_root.has_child(key_dump)) {
    ryml::NodeRef out_opts_dump_node = out_opts_root[key_dump];
    std::string val;
    for(ryml::ConstNodeRef const &dn_c : out_opts_dump_node.children()) {
      dn_c >> val;
      if(val == STR_FALSE) {
        if(obj_out_.has_child(dn_c.key())) {
          obj_out_.remove_child(dn_c.key());
        }
      }
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

scenario::scenario(context &env) :
  ctx_(env),
  stats_(*this),
  js_env_(*this)
{}

scenario::~scenario()
{}

int scenario::init(std::shared_ptr<spdlog::logger> &event_log)
{
  int res = 0;
  event_log_ = event_log;

  if((res = js_env_.init(event_log_))) {
    return res;
  }

  return res;
}

int scenario::reset(ryml::Tree &doc_in,
                    ryml::NodeRef scenario_in)
{
  scenario_in_root_ = scenario_in;
  assert_failure_ = false;

  //clear out & buffers
  scenario_out_.clear();
  ryml_scenario_out_buf_.clear();

  // reset stats
  stats_.reset();

  //initialize scenario-out
  utils::set_tree_node(doc_in,
                       scenario_in_root_,
                       scenario_out_.rootref(),
                       ryml_scenario_out_buf_);

  int res = js_env_.reset();
  return res;
}

int scenario::process(ryml::Tree &doc_in,
                      ryml::NodeRef scenario_in)
{
  int res = 0;

  if((res = reset(doc_in, scenario_in))) {
    event_log_->error("failed to reset scenario");
    return res;
  }

  bool error = false;
  ryml::NodeRef scenario_out_root = scenario_out_.rootref();

  {
    stack_scope scope(*this,
                      scenario_in_root_,
                      scenario_out_root,
                      error,
                      utils::get_default_scenario_out_options());
    if(error) {
      goto fun_end;
    }

    if(scope.enabled_) {
      if(scenario_in_root_.has_child(key_conversations)) {
        ryml::NodeRef conversations_in = scenario_in_root_[key_conversations];
        if(!conversations_in.is_seq()) {
          res = 1;
          event_log_->error(ERR_CONV_NOT_SEQ);
          utils::clear_map_node_put_key_val(scenario_out_root, key_error, ERR_CONV_NOT_SEQ);
          goto fun_end;
        }

        ryml::NodeRef conversations_out = scenario_out_root[key_conversations];

        uint32_t conv_it = 0;
        for(ryml::NodeRef const &conversation_in : conversations_in.children()) {

          /*TODO parallel handling*/
          conversation conv(*this);

          if((res = conv.process(conversation_in,
                                 conversations_out[conv_it]))) {
            break;
          }
          ++conv_it;
        }
      }
      if(!res) {
        enrich_with_stats(scenario_out_root);
        scope.commit();
      }
    } else {
      utils::clear_map_node_put_key_val(scenario_out_root, key_enabled, STR_FALSE);
    }
  }

fun_end:
  if(error) {
    res = 1;
  }
  if(res) {
    scenario_out_root[key_error_occurred] << STR_TRUE;
  }
  // finally write scenario_out on the output
  if(!ctx_.cfg_.no_out_) {
    if(ctx_.cfg_.out_format == STR_YAML) {
      *ctx_.output_ << YAML_DOC_SEP << std::endl << scenario_out_;
    } else if(ctx_.cfg_.out_format == STR_JSON) {
      *ctx_.output_ << ryml::as_json(scenario_out_);
    }
  }
  return res;
}

void scenario::enrich_with_stats(ryml::NodeRef scenario_out)
{
  ryml::NodeRef statistics = scenario_out[key_stats];
  statistics |= ryml::MAP;
  statistics[key_conversations] << stats_.conversation_count_;
  statistics[key_requests] << stats_.request_count_;
  ryml::NodeRef res_code_categorization = statistics[key_categorization];
  res_code_categorization |= ryml::MAP;
  std::for_each(stats_.categorization_.begin(), stats_.categorization_.end(), [&](const auto &it) {
    ryml::csubstr code = res_code_categorization.to_arena(it.first);
    res_code_categorization[code] << it.second;
  });
}

}
