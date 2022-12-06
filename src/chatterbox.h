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

    int execute_request(const std::string &method,
                        const std::string &auth,
                        const std::string &uri,
                        const std::string &query_string,
                        const std::string &data,
                        bool res_body_dump,
                        const std::string &res_body_format,
                        Json::Value &conversation_out);

    int on_response(const RestClient::Response &res,
                    const std::string &method,
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

    void build_v2(const char *method,
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

    void build_v4(const char *method,
                  const std::string &uri,
                  const std::string &query_string,
                  const std::string &data,
                  RestClient::HeaderFields &reqHF) const;

    // -------------
    // --- Utils ---
    // -------------

    void dump_hdr(const RestClient::HeaderFields &hdr) const;

    void dump_response(const std::string &method,
                       const std::string &auth,
                       const std::string &uri,
                       const std::string &query_string,
                       const std::string &data,
                       bool res_body_dump,
                       const std::string &res_body_format,
                       const RestClient::Response &res,
                       Json::Value &conversation_out);

    void move_file(const char *filename);
    void rm_file(const char *filename);

    template <typename T>
    struct Converter;

    // ------------------
    // --- Evaluators ---
    // ------------------

    template <typename T>
    std::optional<T> eval_as(const Json::Value &from,
                             const char *key,
                             const std::optional<T> default_value = std::nullopt) {
      Json::Value val = from.get(key, Json::Value::null);
      if(val) {
        if(Converter<T>::isType(val)) {
          //eval as primitive value
          return Converter<T>::asType(val);
        } else {
          //eval as javascript function
          Json::Value function = val.get("function", Json::Value::null);
          if(!function || !function.isString()) {
            event_log_->error("function name is not string type");
            return std::nullopt;
          }

          Json::Value j_args = val.get("args", Json::Value::null);
          std::vector<std::string> args;
          if(j_args) {
            if(!j_args.isArray()) {
              event_log_->error("function args are not array type");
              return std::nullopt;
            }
            for(uint32_t i = 0; i < j_args.size(); ++i) {
              Json::Value arg = j_args[i];
              if(!arg.isString()) {
                event_log_->error("function arg is not string type");
                return std::nullopt;
              }
              args.emplace_back(arg.asString());
            }
          }

          //finally invoke the javascript function
          T result;
          std::string error;

          bool js_res = js_env_.invoke_js_function(function.asCString(),
                                                   args,
          [&](v8::Isolate *isl, const std::vector<std::string> &args, v8::Local<v8::Value> argv[]) -> bool{
            std::transform(args.begin(), args.end(), argv, [&](const std::string &it) -> v8::Local<v8::String> {
              return v8::String::NewFromUtf8(isl, it.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
            });
            return true;
          },
          [&](v8::Isolate *isl, const v8::Local<v8::Value> &res) -> bool{
            if(!Converter<T>::isType(res)) {
              std::stringstream ss;
              ss << "function result is not " << Converter<T>::name() << " type";
              event_log_->error(ss.str());
              return false;
            }
            result = Converter<T>::asType(res, isl);
            return true;
          },
          error);

          if(!js_res) {
            event_log_->error("failure invoking function:{}, error:{}", function.asString(), error);
            return std::nullopt;
          }
          return result;
        }
      } else {
        return default_value;
      }
    }


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
    uint32_t conv_request_count_ = 0;
    std::unordered_map<std::string, int32_t> conv_res_code_categorization_;

    //scenario statistics
    uint32_t scen_conversation_count_ = 0;
    uint32_t scen_request_count_ = 0;
    std::unordered_map<std::string, int32_t> scen_res_code_categorization_;

    //js environment
    js::js_env js_env_;

    //output
    std::shared_ptr<spdlog::logger> output_;

  public:
    std::string event_log_fmt_;
    std::shared_ptr<spdlog::logger> event_log_;
};

template <>
struct chatterbox::Converter<bool> {
  static bool isType(const Json::Value &val) {
    return val.isBool();
  }
  static bool isType(const v8::Local<v8::Value> &val) {
    return val->IsBoolean();
  }
  static bool asType(const Json::Value &val) {
    return val.asBool();
  }
  static bool asType(const v8::Local<v8::Value> &val, v8::Isolate *isl) {
    return val->BooleanValue(isl);
  }
  static std::string name() {
    return "boolean";
  }
};

template <>
struct chatterbox::Converter<uint32_t> {
  static bool isType(const Json::Value &val) {
    return val.isUInt();
  }
  static bool isType(const v8::Local<v8::Value> &val) {
    return val->IsUint32();
  }
  static uint32_t asType(const Json::Value &val) {
    return val.asUInt();
  }
  static uint32_t asType(const v8::Local<v8::Value> &val, v8::Isolate *isl) {
    return val->Uint32Value(isl->GetCurrentContext()).FromMaybe(0);
  }
  static std::string name() {
    return "uint32_t";
  }
};

template <>
struct chatterbox::Converter<std::string> {
  static bool isType(const Json::Value &val) {
    return val.isString();
  }
  static bool isType(const v8::Local<v8::Value> &val) {
    return val->IsString();
  }
  static std::string asType(const Json::Value &val) {
    return val.asString();
  }
  static std::string asType(const v8::Local<v8::Value> &val, v8::Isolate *isl) {
    v8::String::Utf8Value utf_res(isl, val);
    return *utf_res;
  }
  static std::string name() {
    return "string";
  }
};

}
