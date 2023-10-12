#pragma once
#include "cbox.h"

#define ERR_FAIL_EVAL "property evaluation failed"

constexpr const char *PROP_EVAL_RGX = "\\{\\{.*?\\}\\}";

namespace cbox {

// ----------------------------------
// --- SCENARIO PROPERTY RESOLVER ---
// ----------------------------------

struct scenario_property_resolver {

  scenario_property_resolver() = default;

  int init(std::shared_ptr<spdlog::logger> &event_log);
  int reset(ryml::ConstNodeRef scenario_obj_root);

  std::optional<ryml::ConstNodeRef> resolve(const std::string &path) const;

  std::optional<ryml::ConstNodeRef> resolve_common(ryml::ConstNodeRef from, utils::str_tok &tknz) const;

  ryml::ConstNodeRef scenario_obj_root_;

  //event logger
  std::shared_ptr<spdlog::logger> event_log_;
};

// -----------------------------------
// --- SCENARIO PROPERTY EVALUATOR ---
// -----------------------------------

struct scenario_property_evaluator {

  scenario_property_evaluator() = default;

  int init(std::shared_ptr<spdlog::logger> &event_log);
  int reset(ryml::ConstNodeRef scenario_obj_root);

  template <typename T>
  std::optional<T> eval_as(ryml::ConstNodeRef from,
                           const char *key,
                           const scenario_property_resolver &spr) {
    ryml::ConstNodeRef n_val = from[ryml::to_csubstr(key)];
    std::string str_val, str_res;
    n_val >> str_val;
    str_res = str_val;
    std::regex rgx("\\{\\{.*?\\}\\}");

    std::regex_iterator<std::string::const_iterator> rit(str_val.cbegin(), str_val.cend(), rgx);
    std::regex_iterator<std::string::const_iterator> rend;

    while(rit!=rend) {
      auto node_ref = spr.resolve(utils::trim(utils::find_and_replace(utils::find_and_replace(rit->str(),
                                                                                              "{{", ""), "}}", "")));
      if(node_ref && ((*node_ref).is_keyval() || (*node_ref).is_val())) {
        auto node_val = utils::converter<std::string>::asType(*node_ref);
        str_res = std::regex_replace(str_res,rgx,node_val,std::regex_constants::format_first_only);
      } else {
        return std::nullopt;
      }
      ++rit;
    }

    if constexpr(std::is_same_v<T, std::string>) {
      return str_res;
    } else {
      T res;
      std::stringstream ss;
      ss << str_res;
      ss >> res;
      return res;
    }
  }

  ryml::ConstNodeRef scenario_obj_root_;

  //event logger
  std::shared_ptr<spdlog::logger> event_log_;
};

struct scenario {

    // -------------------
    // --- STACK SCOPE ---
    // -------------------

    struct stack_scope {

// *INDENT-OFF*
        stack_scope(scenario &parent,
                    ryml::NodeRef obj_in,
                    ryml::NodeRef obj_out,
                    bool &error,
                    const ryml::Tree &default_out_options = utils::get_default_out_options(),
                    bool call_dump_val_cb = false,
                    const std::function<void(const std::string &key, bool val)> &dump_val_cb =
                    [](const std::string &, bool) {},
                    bool call_format_val_cb = false,
                    const std::function<void(const std::string &key, const std::string &val)> &format_val_cb =
                    [](const std::string &, const std::string &) {});
// *INDENT-ON*

      ~stack_scope();

// *INDENT-OFF*
      bool push_out_opts(const ryml::Tree &default_out_options,
                         bool call_dump_val_cb = false,
                         const std::function<void(const std::string &key, bool val)> &dump_val_cb =
                         [](const std::string &, bool) {},
                         bool call_format_val_cb = false,
                         const std::function<void(const std::string &key, const std::string &val)> &format_val_cb =
                         [](const std::string &, const std::string &) {});
// *INDENT-ON*

      bool pop_process_out_opts();

      void commit() {
        commit_ = true;
      }

      scenario &parent_;
      ryml::NodeRef obj_in_;
      ryml::NodeRef obj_out_;
      ryml::Tree out_opts_;

      bool &error_;
      bool enabled_, commit_;
    };

    // ------------------
    // --- STATISTICS ---
    // ------------------

    struct statistics {
        friend struct scenario;

        statistics(scenario &parent) : parent_(parent) {}

        void reset();
        void incr_conversation_count();
        void incr_request_count();
        void incr_categorization(const std::string &key);

        scenario &parent_;

      private:
        uint32_t conversation_count_ = 0;
        uint32_t request_count_ = 0;
        std::unordered_map<std::string, int32_t> categorization_;
    };

    scenario(context &env);
    ~scenario();

    int init(std::shared_ptr<spdlog::logger> &event_log);

    int reset(ryml::Tree &doc_in,
              ryml::NodeRef scenario_in);

    int process(ryml::Tree &doc_in,
                ryml::NodeRef scenario_in);

    void set_assert_failure() {
      assert_failure_ = true;
    }

    // -------------
    // --- Utils ---
    // -------------

    void enrich_with_stats(ryml::NodeRef scenario_out);

  public:
    context &ctx_;

    //current scenario_in and scenario_out
    ryml::NodeRef scenario_in_root_;
    ryml::Tree scenario_out_;

    //ryml scenario out support buffer
    std::vector<char> ryml_scenario_out_buf_;

    //scenario property resolver
    scenario_property_resolver scen_out_p_resolv_;

    //scenario property evaluator
    scenario_property_evaluator scen_p_evaluator_;

    //scenario statistics
    statistics stats_;

    //js environment
    js::js_env js_env_;

  private:
    //assert failure
    bool assert_failure_ = false;

  public:
    std::shared_ptr<spdlog::logger> event_log_;
};

}
