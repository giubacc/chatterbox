#pragma once
#include "conversation.h"

namespace cbox {
struct request {

    request(conversation &parent);

    int reset(const std::string &raw_host,
              ryml::NodeRef request_in);

    int process(const std::string &raw_host,
                ryml::NodeRef request_in,
                ryml::NodeRef requests_out);

    int execute(const std::string &method,
                const std::optional<std::string> &auth,
                const std::string &uri,
                const std::optional<std::string> &query_string,
                const std::optional<std::string> &data,
                ryml::NodeRef request_in,
                ryml::NodeRef request_out);

    int on_response(const RestClient::Response &resRC,
                    const int64_t rtt,
                    ryml::NodeRef request_in,
                    ryml::NodeRef request_out);

    int process_response(const RestClient::Response &resRC,
                         const int64_t rtt,
                         ryml::NodeRef response_in,
                         ryml::NodeRef response_out);

    // ------------
    // --- HTTP ---
    // ------------

    /**
     * POST
     */
    int post(RestClient::HeaderFields &reqHF,
             const std::optional<std::string> &auth,
             const std::string &uri,
             const std::optional<std::string> &query_string,
             const std::optional<std::string> &data,
             const std::function<int(const RestClient::Response &, const int64_t)> &cb);

    /**
     * PUT
     */
    int put(RestClient::HeaderFields &reqHF,
            const std::optional<std::string> &auth,
            const std::string &uri,
            const std::optional<std::string> &query_string,
            const std::optional<std::string> &data,
            const std::function <int (const RestClient::Response &, const int64_t)> &cb);

    /**
     * GET
     */
    int get(RestClient::HeaderFields &reqHF,
            const std::optional<std::string> &auth,
            const std::string &uri,
            const std::optional<std::string> &query_string,
            const std::function <int (const RestClient::Response &, const int64_t)> &cb);

    /**
     * DELETE
     */
    int del(RestClient::HeaderFields &reqHF,
            const std::optional<std::string> &auth,
            const std::string &uri,
            const std::optional<std::string> &query_string,
            const std::function <int (const RestClient::Response &, const int64_t)> &cb);

    /**
     * HEAD
     */
    int head(RestClient::HeaderFields &reqHF,
             const std::optional<std::string> &auth,
             const std::string &uri,
             const std::optional<std::string> &query_string,
             const std::function <int (const RestClient::Response &, const int64_t)> &cb);

  private:

    int prepare_http_req(const char *method,
                         const std::optional<std::string> &auth,
                         const std::optional<std::string> &query_string,
                         const std::optional<std::string> &data,
                         std::string &uri_out,
                         RestClient::HeaderFields &reqHF);

    // -------------
    // --- UTILS ---
    // -------------

    void dump_hdr(const RestClient::HeaderFields &hdr) const;
    int mocked_to_res(RestClient::Response &resRC);

    // -----------
    // --- REP ---
    // -----------

    //parent
    conversation &parent_;

    //ryml request out support buffer
    std::vector<char> ryml_request_out_buf_;

    //scenario property resolver
    scenario_property_resolver &scen_out_p_resolv_;

    //scenario property evaluator
    scenario_property_evaluator &scen_p_evaluator_;

    //js environment
    js::js_env &js_env_;

    //event logger
    std::shared_ptr<spdlog::logger> event_log_;

    //current response mock
    ryml::NodeRef response_mock_;

    //request connection
    std::unique_ptr<RestClient::Connection> conv_conn_;
};

}
