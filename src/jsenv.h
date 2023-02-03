#pragma once

#include "libplatform/libplatform.h"
#include "v8.h"
#include "globals.h"

namespace cbox {
struct scenario;
}

namespace utils {

template <typename T>
struct converter;

template <>
struct converter<bool> {
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
struct converter<int32_t> {
  static bool isType(const Json::Value &val) {
    return val.isUInt();
  }
  static bool isType(const v8::Local<v8::Value> &val) {
    return val->IsInt32();
  }
  static int32_t asType(const Json::Value &val) {
    return val.asInt();
  }
  static int32_t asType(const v8::Local<v8::Value> &val, v8::Isolate *isl) {
    return val->Int32Value(isl->GetCurrentContext()).FromMaybe(0);
  }
  static std::string name() {
    return "int32_t";
  }
};

template <>
struct converter<uint32_t> {
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
struct converter<double> {
  static bool isType(const Json::Value &val) {
    return val.isDouble();
  }
  static bool isType(const v8::Local<v8::Value> &val) {
    return val->IsNumber();
  }
  static double asType(const Json::Value &val) {
    return val.asDouble();
  }
  static double asType(const v8::Local<v8::Value> &val, v8::Isolate *isl) {
    return val->NumberValue(isl->GetCurrentContext()).FromMaybe(0.0);
  }
  static std::string name() {
    return "double";
  }
};

template <>
struct converter<std::string> {
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

namespace js {

struct js_env {

  //global V8 platform
  static std::unique_ptr<v8::Platform> platform;

  static bool init_V8(int argc, const char *argv[]);
  static void stop_V8();

  js_env(cbox::scenario &parent);
  ~js_env();

  int init(std::shared_ptr<spdlog::logger> &event_log);

  int renew_scenario_context();

  // -----------------------
  // --- Context objects ---
  // -----------------------

  bool install_scenario_objects();

  //Json::Value

  v8::Local<v8::ObjectTemplate> make_json_value_template();

  v8::Local<v8::Object> wrap_json_value(Json::Value &obj);
  static Json::Value *unwrap_json_value(v8::Local<v8::Object> obj);

  static bool js_value_from_json_value(Json::Value &obj_val,
                                       v8::Local<v8::Value> &js_obj_val,
                                       js_env &self);

  static bool json_value_from_js_value(Json::Value &obj_val,
                                       v8::Local<v8::Value> &js_obj_val,
                                       js_env &self);

  static void json_value_get_by_name(v8::Local<v8::Name> key,
                                     const v8::PropertyCallbackInfo<v8::Value> &pci);

  static void json_value_set_by_name(v8::Local<v8::Name> key,
                                     v8::Local<v8::Value> value,
                                     const v8::PropertyCallbackInfo<v8::Value> &pci);

  static void json_value_get_by_idx(uint32_t index,
                                    const v8::PropertyCallbackInfo<v8::Value> &pci);

  static void json_value_set_by_idx(uint32_t index,
                                    v8::Local<v8::Value> value,
                                    const v8::PropertyCallbackInfo<v8::Value> &pci);

  // -------------------------
  // --- C++ -> Javascript ---
  // -------------------------

  bool invoke_js_function(Json::Value *ctx_json,
                          const char *js_function_name,
                          Json::Value &js_args,
                          const std::function <bool (v8::Isolate *,
                                                     Json::Value &,
                                                     v8::Local<v8::Value> [])> &prepare_argv,
                          const std::function <bool (v8::Isolate *,
                                                     const v8::Local<v8::Value>&)> &process_result,
                          std::string &error);

  // -------------------------
  // --- Javascript -> C++ ---
  // -------------------------

  static void cbk_log(const v8::FunctionCallbackInfo<v8::Value> &args);
  static void cbk_load(const v8::FunctionCallbackInfo<v8::Value> &args);
  static void cbk_assert(const v8::FunctionCallbackInfo<v8::Value> &args);

  // ----------------------------
  // --- Json Field Evaluator ---
  // ----------------------------

  template <typename T>
  std::optional<T> eval_as(const Json::Value &from,
                           const char *key,
                           const std::optional<T> default_value = std::nullopt) {
    Json::Value val = from.get(key, Json::Value::null);
    if(val) {
      if(utils::converter<T>::isType(val)) {
        //eval as primitive value
        return utils::converter<T>::asType(val);
      } else {
        //eval as javascript function
        Json::Value function = val.get("function", Json::Value::null);
        if(!function || !function.isString()) {
          event_log_->error("function name is not string type");
          return std::nullopt;
        }

        Json::Value js_args = val.get("args", Json::Value::null);
        if(js_args) {
          if(!js_args.isArray()) {
            event_log_->error("function args are not array type");
            return std::nullopt;
          }
        }

        //finally invoke the javascript function
        T result;
        std::string error;

        bool js_res = invoke_js_function(nullptr,
                                         function.asCString(),
                                         js_args,
        [&](v8::Isolate *isl, Json::Value &js_args, v8::Local<v8::Value> argv[]) -> bool{
          for(uint32_t i = 0; i < js_args.size(); ++i) {
            if(!js_value_from_json_value(js_args[i], argv[i], *this)) {
              return false;
            }
          }
          return true;
        },
        [&](v8::Isolate *isl, const v8::Local<v8::Value> &res) -> bool{
          if(!utils::converter<T>::isType(res)) {
            std::stringstream ss;
            ss << "function result is not " << utils::converter<T>::name() << " type";
            event_log_->error(ss.str());
            return false;
          }
          result = utils::converter<T>::asType(res, isl);
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

  // ---------------------
  // --- Generic Utils ---
  // ---------------------

  int load_scripts();
  v8::MaybeLocal<v8::String> read_script_file(const std::string &name);

  bool compile_script(const v8::Local<v8::String> &script_src,
                      v8::Local<v8::Script> &script,
                      std::string &error);

  bool run_script(const v8::Local<v8::Script> &script,
                  v8::Local<v8::Value> &result,
                  std::string &error);

  bool exec_as_function(Json::Value &from,
                        const char *key,
                        bool optional = true);

  std::string js_obj_to_std_string(v8::Local<v8::Value> value);

  // -----------
  // --- rep ---
  // -----------

  //parent
  cbox::scenario &parent_;

  //processed scripts
  std::unordered_set<std::string> processed_scripts_;

  //the parameters used to instantiate the Isolate.
  v8::Isolate::CreateParams params_;

  //the Isolate associated with this object.
  v8::Isolate *isolate_ = nullptr;

  //the context associated with this object,
  //it is disposed and then recreated on every scenario.
  v8::Global<v8::Context> scenario_context_;

  //the global object template for json_value
  v8::Global<v8::ObjectTemplate> json_value_template_;

  //event logger
  std::string event_log_fmt_;
  std::shared_ptr<spdlog::logger> event_log_;
};

}
