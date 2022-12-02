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

    chatterbox();
    ~chatterbox();

    int init(int argc, char *argv[]);
    int reset_scenario_ctx(const Json::Value &scenario_ctx);
    int reset_conversation_ctx(const Json::Value &conversation_ctx);
    void poll();

    int execute_scenario(const char *fname);
    int execute_scenario(std::istream &is);

    int execute_talk(const std::string &verb,
                     const std::string &auth,
                     const std::string &uri,
                     const std::string &query_string,
                     const std::string &data,
                     bool res_body_dump,
                     const std::string &res_body_format,
                     Json::Value &conversation_out);

    int on_talk_response(const RestClient::Response &res,
                         const std::string &verb,
                         const std::string &auth,
                         const std::string &uri,
                         const std::string &query_string,
                         const std::string &data,
                         bool res_body_dump,
                         const std::string &res_body_format,
                         Json::Value &conversation_out);

    void on_conversation_complete(Json::Value &conversation_ctx_out);
    void on_scenario_complete(Json::Value &scenario_out);

    // ------------
    // --- HTTP ---
    // ------------

    /** post
     *
     *  HTTP POST
     */
    int post(const std::string &auth,
             const std::string &uri,
             const std::string &query_string,
             const std::string &data,
             const std::function<void(RestClient::Response &)> &cb);

    /** put
     *
     *  HTTP PUT
     */
    int put(const std::string &auth,
            const std::string &uri,
            const std::string &query_string,
            const std::string &data,
            const std::function <void (RestClient::Response &)> &cb);

    /** get
     *
     *  HTTP GET
     */
    int get(const std::string &auth,
            const std::string &uri,
            const std::string &query_string,
            const std::function <void (RestClient::Response &)> &cb);

    /** del
     *
     *  HTTP DELETE
     */
    int del(const std::string &auth,
            const std::string &uri,
            const std::string &query_string,
            const std::function <void (RestClient::Response &)> &cb);

    /** head
     *
     *  HTTP HEAD
     */
    int head(const std::string &auth,
             const std::string &uri,
             const std::string &query_string,
             const std::function <void (RestClient::Response &)> &cb);

  private:
    // ---------------
    // --- S3 auth ---
    // ---------------

    static std::string build_v2_date();
    static std::string build_v4_date();

    // v2

    std::string build_v2_canonical_string(const std::string &method,
                                          const std::string &canonical_uri) const;

    std::string build_v2_authorization(const std::string &signature) const;

    void build_v2(const char *verb,
                  const std::string &uri,
                  RestClient::HeaderFields &reqHF) const;

    // v4

    std::string build_v4_signing_key() const;
    std::string build_v4_canonical_headers(const std::string &x_amz_content_sha256) const;

    std::string build_v4_canonical_request(const std::string &method,
                                           const std::string &canonical_uri,
                                           const std::string &canonical_query_string,
                                           const std::string &canonical_headers,
                                           const std::string &payload_hash) const;

    std::string build_v4_string_to_sign(const std::string &canonical_request) const;
    std::string build_v4_authorization(const std::string &signature) const;

    void build_v4(const char *verb,
                  const std::string &uri,
                  const std::string &query_string,
                  const std::string &data,
                  RestClient::HeaderFields &reqHF) const;

    // -------------
    // --- Utils ---
    // -------------

    void dump_hdr(const RestClient::HeaderFields &hdr) const;

    void dump_talk_response(const Json::Value &talk,
                            bool res_body_dump,
                            const std::string &res_body_format,
                            const RestClient::Response &res,
                            Json::Value &conversation_out);

    void move_file(const char *filename);
    void rm_file(const char *filename);

    // ------------------
    // --- Evaluators ---
    // ------------------

    std::optional<bool> eval_as_bool(const Json::Value &from,
                                     const char *key,
                                     const std::optional<bool> default_value = std::nullopt);

    std::optional<uint32_t> eval_as_uint32_t(const Json::Value &from,
                                             const char *key,
                                             const std::optional<uint32_t> default_value = std::nullopt);

    std::optional<std::string> eval_as_string(const Json::Value &from,
                                              const char *key,
                                              const std::optional<std::string> &default_value = std::nullopt);

    // ----------------
    // --- JS Utils ---
    // ----------------

    bool exec_as_function(const Json::Value &from,
                          const char *key,
                          bool optional = true);

  public:
    cfg cfg_;

  private:
    //conversation connection
    std::unique_ptr<RestClient::Connection> conv_conn_;

    //current scenario and scenario_out
    Json::Value scenario_;
    Json::Value scenario_out_;

    //conversation context
    std::string access_key_;
    std::string secret_key_;
    std::string raw_host_;
    std::string host_;
    std::string x_amz_date_;
    std::string signed_headers_;
    std::string region_;
    std::string service_;
    bool res_conv_dump_ = true;

    //conversation statistics
    uint32_t conv_talk_count_ = 0;
    std::unordered_map<std::string, int32_t> conv_res_code_categorization_;

    //scenario statistics
    uint32_t scen_conversation_count_ = 0;
    uint32_t scen_talk_count_ = 0;
    std::unordered_map<std::string, int32_t> scen_res_code_categorization_;

    //js environment
    js::js_env js_env_;

    //output
    std::shared_ptr<spdlog::logger> output_;

  public:
    std::string event_log_fmt_;
    std::shared_ptr<spdlog::logger> event_log_;
};

}
