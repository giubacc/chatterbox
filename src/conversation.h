#pragma once
#include "scenario.h"

namespace cbox {

struct conversation {

    // ------------------
    // --- STATISTICS ---
    // ------------------

    struct statistics {
        friend struct conversation;

        statistics(conversation &parent) : parent_(parent) {}

        void reset();
        void incr_request_count();
        void incr_categorization(const std::string &key);

        conversation &parent_;

      private:
        uint32_t request_count_ = 0;
        std::unordered_map<std::string, int32_t> categorization_;
    };

    conversation(scenario &parent);

    int init(std::shared_ptr<spdlog::logger> &event_log);

    int reset(ryml::NodeRef conversation_in);

    int process(ryml::NodeRef conversation_in,
                ryml::NodeRef conversation_out);

    // -------------
    // --- UTILS ---
    // -------------

    void enrich_with_stats(ryml::NodeRef conversation_out);

    // -----------
    // --- REP ---
    // -----------

  public:
    //parent
    scenario &parent_;

    //js environment
    js::js_env &js_env_;

    //conversation statistics
    statistics stats_;

    //conversation context
    std::string raw_host_;

    //aws auth
    utils::aws_auth auth_;

    //event logger
    std::shared_ptr<spdlog::logger> event_log_;
};

}
