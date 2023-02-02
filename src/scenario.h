#pragma once
#include "conversation.h"

namespace cbox {

struct scenario {

    // scenario configuration
    struct cfg {
      bool monitor = false;
      bool move_scenario = true;

      std::string in_scenario_path = "";
      std::string in_scenario_name = "";

      std::string out_channel = "stdout";

      std::string evt_log_channel = "stderr";
      std::string evt_log_level = "inf";

      bool no_out_ = false;
    };

    static const Json::Value &get_default_out_options();
    static const Json::Value &get_default_scenario_out_options();
    static const Json::Value &get_default_conversation_out_options();
    static const Json::Value &get_default_request_out_options();
    static const Json::Value &get_default_response_out_options();

    // -------------------
    // --- STACK SCOPE ---
    // -------------------

    struct stack_scope {

// *INDENT-OFF*
        stack_scope(scenario &parent,
                    Json::Value &obj_in,
                    Json::Value &obj_out,
                    bool &error,
                    const Json::Value &default_out_options = get_default_out_options(),
                    bool call_dump_val_cb = false,
                    const std::function<void(const std::string &key, bool val)> &dump_val_cb =
                    [](const std::string &, bool) {},
                    bool call_format_val_cb = false,
                    const std::function<void(const std::string &key, const std::string &val)> &format_val_cb =
                    [](const std::string &, const std::string &) {});
// *INDENT-ON*

      ~stack_scope();

// *INDENT-OFF*
      bool push_out_opts(const Json::Value &default_out_options,
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
      Json::Value &obj_in_;
      Json::Value &obj_out_;
      const Json::Value &default_out_options_;
      Json::Value out_options_;

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

    int process();
    int process(const char *fname);
    int process(std::istream &is);

    void poll();

    void set_assert_failure() {
      assert_failure_ = true;
    }

    // -------------
    // --- Utils ---
    // -------------

    void enrich_with_stats(Json::Value &scenario_out);
    void move_file(const char *filename);
    void rm_file(const char *filename);

  public:
    cfg cfg_;

    //current scenario_in and scenario_out
    Json::Value scenario_in_;
    Json::Value scenario_out_;

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
