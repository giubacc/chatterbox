#pragma once
#include "conversation.h"

namespace cbox {

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

    scenario();
    ~scenario();

    int init(int argc, const char *argv[]);
    int reset();

    int load_source_process();
    int load_source_process(const char *fname);
    int process();

    void set_assert_failure() {
      assert_failure_ = true;
    }

    // -------------
    // --- Utils ---
    // -------------

    void enrich_with_stats(ryml::NodeRef scenario_out);

  public:
    utils::cfg cfg_;

    //ryml error handler
    utils::RymlErrorHandler REH_;

    //current scenario_in and scenario_out
    ryml::Tree scenario_in_;
    ryml::Tree scenario_out_;

    //ryml scenario load buffer
    std::vector<char> ryml_load_buf_;
    //ryml scenario modify support buffer
    std::vector<char> ryml_modify_buf_;

    //scenario statistics
    statistics stats_;

  private:
    //assert failure
    bool assert_failure_ = false;

  public:
    //js environment
    js::js_env js_env_;

    //output
    std::shared_ptr<spdlog::logger> output_;

    //event log
    std::string event_log_fmt_;
    std::shared_ptr<spdlog::logger> event_log_;
};

}
