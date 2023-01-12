#pragma once

#include "crypto.h"
#include "jsenv.h"

namespace rest {

struct chatterbox {

    // chatterbox configuration
    struct cfg {
      bool monitor = false;
      bool move_scenario = true;

      std::string in_scenario_path = "";
      std::string in_scenario_name = "";

      std::string out_channel = "stdout";

      std::string evt_log_channel = "stderr";
      std::string evt_log_level = "inf";
    };

    static const Json::Value &get_default_out_options();
    static const Json::Value &get_default_scenario_out_options();
    static const Json::Value &get_default_conversation_out_options();
    static const Json::Value &get_default_request_out_options();
    static const Json::Value &get_default_response_out_options();

    struct stack_scope {
// *INDENT-OFF*
        stack_scope(chatterbox &cbox,
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

      void commit() {
        commit_ = true;
      }

      chatterbox &cbox_;
      bool &error_;
      bool commit_;
    };

    chatterbox();
    ~chatterbox();

    int init(int argc, const char *argv[]);

    void poll();

// *INDENT-OFF*
    bool push_out_opts(const Json::Value &default_out_options = get_default_out_options(),
                       bool call_dump_val_cb = false,
                       const std::function<void(const std::string &key, bool val)> &dump_val_cb =
                       [](const std::string &, bool) {},
                       bool call_format_val_cb = false,
                       const std::function<void(const std::string &key, const std::string &val)> &format_val_cb =
                       [](const std::string &, const std::string &) {});
// *INDENT-ON*

    bool pop_process_out_opts();

    int reset_scenario();

    int process_scenario();
    int process_scenario(const char *fname);
    int process_scenario(std::istream &is);

    int reset_conversation(const Json::Value &conversation_in);

    int process_conversation(Json::Value &conversation_in,
                             Json::Value &conversation_out);

    int process_request(Json::Value &request_in,
                        Json::Value &request_out);

    int execute_request(const std::string &method,
                        const std::optional<std::string> &auth,
                        const std::string &uri,
                        const std::string &query_string,
                        const std::string &data,
                        Json::Value &request_in,
                        Json::Value &request_out);

    int on_response(const RestClient::Response &resRC,
                    const int64_t rtt,
                    Json::Value &request_in,
                    Json::Value &request_out);

    int process_response(const RestClient::Response &resRC,
                         const int64_t rtt,
                         Json::Value &response_in,
                         Json::Value &response_out);

    void enrich_with_stats_conversation(Json::Value &conversation_out);
    void enrich_with_stats_scenario(Json::Value &scenario_out);

    // ------------
    // --- HTTP ---
    // ------------

    /** post
     *
     *  HTTP POST
     */
    int post(const std::optional<std::string> &auth,
             const std::string &uri,
             const std::string &query_string,
             const std::string &data,
             const std::function<int(const RestClient::Response &, const int64_t)> &cb);

    /** put
     *
     *  HTTP PUT
     */
    int put(const std::optional<std::string> &auth,
            const std::string &uri,
            const std::string &query_string,
            const std::string &data,
            const std::function <int (const RestClient::Response &, const int64_t)> &cb);

    /** get
     *
     *  HTTP GET
     */
    int get(const std::optional<std::string> &auth,
            const std::string &uri,
            const std::string &query_string,
            const std::function <int (const RestClient::Response &, const int64_t)> &cb);

    /** del
     *
     *  HTTP DELETE
     */
    int del(const std::optional<std::string> &auth,
            const std::string &uri,
            const std::string &query_string,
            const std::function <int (const RestClient::Response &, const int64_t)> &cb);

    /** head
     *
     *  HTTP HEAD
     */
    int head(const std::optional<std::string> &auth,
             const std::string &uri,
             const std::string &query_string,
             const std::function <int (const RestClient::Response &, const int64_t)> &cb);

  private:

    // ------------
    // --- HTTP ---
    // ------------

    int prepare_http_req(const char *method,
                         const std::optional<std::string> &auth,
                         const std::string &query_string,
                         const std::string &data,
                         std::string &uri_out,
                         RestClient::HeaderFields &reqHF);

    // ------------
    // --- auth ---
    // ------------

    // AWS Signature Version 2

    static std::string aws_sign_v2_build_date();

    std::string aws_sign_v2_build_canonical_string(const std::string &method,
                                                   const std::string &canonical_uri) const;

    std::string aws_sign_v2_build_authorization(const std::string &signature) const;

    void aws_sign_v2_build(const char *method,
                           const std::string &uri,
                           RestClient::HeaderFields &reqHF) const;

    // AWS Signature Version 4

    static std::string aws_sign_v4_build_date();

    std::string aws_sign_v4_build_signing_key() const;
    std::string aws_sign_v4_build_canonical_headers(const std::string &x_amz_content_sha256) const;

    std::string aws_sign_v4_build_canonical_request(const std::string &method,
                                                    const std::string &canonical_uri,
                                                    const std::string &canonical_query_string,
                                                    const std::string &canonical_headers,
                                                    const std::string &payload_hash) const;

    std::string aws_sign_v4_build_string_to_sign(const std::string &canonical_request) const;
    std::string aws_sign_v4_build_authorization(const std::string &signature) const;

    void aws_sign_v4_build(const char *method,
                           const std::string &uri,
                           const std::string &query_string,
                           const std::string &data,
                           RestClient::HeaderFields &reqHF) const;

    // -------------
    // --- Utils ---
    // -------------

    void dump_hdr(const RestClient::HeaderFields &hdr) const;
    void move_file(const char *filename);
    void rm_file(const char *filename);
    int mocked_to_res(RestClient::Response &resRC);

  public:
    cfg cfg_;

    //current scenario_in and scenario_out
    Json::Value scenario_in_;
    Json::Value scenario_out_;

    //stacks for current json-object
    std::stack<utils::json_value_ref> stack_obj_in_;
    std::stack<utils::json_value_ref> stack_obj_out_;

    //stack for current out-options configuration
    std::stack<utils::json_value_ref> stack_out_options_;

    //current response mock
    Json::Value *response_mock_ = nullptr;

    //conversation statistics
    uint32_t conv_request_count_ = 0;
    std::unordered_map<std::string, int32_t> conv_categorization_;

    //scenario statistics
    uint32_t scen_conversation_count_ = 0;
    uint32_t scen_request_count_ = 0;
    std::unordered_map<std::string, int32_t> scen_categorization_;

  private:
    //conversation connection
    std::unique_ptr<RestClient::Connection> conv_conn_;

    //conversation context
    std::string access_key_;
    std::string secret_key_;
    std::string raw_host_;
    std::string host_;
    std::string x_amz_date_;
    std::string signed_headers_;
    std::string region_;
    std::string service_;

    //js environment
    js::js_env js_env_;

  public:
    //output
    std::shared_ptr<spdlog::logger> output_;

    //event log
    std::string event_log_fmt_;
    std::shared_ptr<spdlog::logger> event_log_;
};

}
