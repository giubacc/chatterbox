#pragma once

#include "crypto.h"
#include "jsenv.h"

namespace rest {

struct chatterbox {

    // chatterbox configuration
    struct cfg {
      bool poll = false;
      bool move_scenario = false;

      std::string source_origin = ".";
      std::string processed_dest = "./consumed";

      std::string log_type = "shell";
      std::string log_level = "inf";
    };

    chatterbox();
    ~chatterbox();

    int init(int argc, char *argv[]);
    int reset_scenario_ctx(const Json::Value &scenario_ctx);
    int reset_conversation_ctx(const Json::Value &conversation_ctx);
    void poll();

    void execute_scenario(std::istream &is);

    void execute_talk(const std::string &verb,
                      const std::string &auth,
                      const std::string &uri,
                      const std::string &query_string,
                      const std::string &data,
                      bool body_res_dump,
                      const std::string &body_res_format);

    // --------------------
    // --- HTTP METHODS ---
    // --------------------

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

    void dump_response(const std::string &body_res_format,
                       RestClient::Response &res);

    void move_file(const char *filename);
    void rm_file(const char *filename);

    // ------------------
    // --- Evaluators ---
    // ------------------

    std::optional<bool> eval_as_bool(Json::Value &from,
                                     const char *key,
                                     const std::optional<bool> default_value = std::nullopt);

    std::optional<uint32_t> eval_as_uint32_t(Json::Value &from,
                                             const char *key,
                                             const std::optional<uint32_t> default_value = std::nullopt);

    std::optional<std::string> eval_as_string(Json::Value &from,
                                              const char *key,
                                              const std::optional<std::string> &default_value = std::nullopt);

  public:
    cfg cfg_;

  private:
    std::unique_ptr<RestClient::Connection> resource_conn_;

    std::string access_key_;
    std::string secret_key_;
    std::string raw_host_;
    std::string host_;
    std::string x_amz_date_;
    std::string signed_headers_;
    std::string region_;
    std::string service_;

    // js environment
    js::js_env js_env_;

  public:
    std::string log_fmt_;
    std::shared_ptr<spdlog::logger> log_;
};

}
