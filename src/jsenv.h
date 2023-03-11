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
  static bool isType(ryml::ConstNodeRef val) {
    bool bval;
    val >> bval;
    return true;
  }
  static bool isType(const v8::Local<v8::Value> &val) {
    return val->IsBoolean();
  }
  static bool asType(ryml::ConstNodeRef val) {
    bool bval;
    val >> bval;
    return bval;
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
  static bool isType(ryml::ConstNodeRef val) {
    int32_t ival;
    val >> ival;
    return true;
  }
  static bool isType(const v8::Local<v8::Value> &val) {
    return val->IsInt32();
  }
  static int32_t asType(ryml::ConstNodeRef val) {
    int32_t ival;
    val >> ival;
    return ival;
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
  static bool isType(ryml::ConstNodeRef val) {
    int32_t uival;
    val >> uival;
    return true;
  }
  static bool isType(const v8::Local<v8::Value> &val) {
    return val->IsUint32();
  }
  static uint32_t asType(ryml::ConstNodeRef val) {
    int32_t uival;
    val >> uival;
    return uival;
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
  static bool isType(ryml::ConstNodeRef val) {
    double dval;
    val >> dval;
    return true;
  }
  static bool isType(const v8::Local<v8::Value> &val) {
    return val->IsNumber();
  }
  static double asType(ryml::ConstNodeRef val) {
    double dval;
    val >> dval;
    return dval;
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
  static bool isType(ryml::ConstNodeRef val) {
    std::string sval;
    val >> sval;
    return true;
  }
  static bool isType(const v8::Local<v8::Value> &val) {
    return val->IsString();
  }
  static std::string asType(ryml::ConstNodeRef val) {
    std::string sval;
    val >> sval;
    return sval;
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

  int reset();

  // -----------------------
  // --- Context objects ---
  // -----------------------

  bool install_scenario_objects();

  //ryml::NodeRef

  v8::Local<v8::ObjectTemplate> make_ryml_noderef_template();

  v8::Local<v8::Object> wrap_ryml_noderef(ryml::NodeRef &obj);
  static ryml::NodeRef *unwrap_ryml_noderef(v8::Local<v8::Object> obj);

  static bool js_value_from_ryml_noderef(ryml::NodeRef &obj_val,
                                         v8::Local<v8::Value> &js_obj_val,
                                         js_env &self);

  static bool ryml_noderef_from_js_value(ryml::NodeRef &obj_val,
                                         v8::Local<v8::Value> &js_obj_val,
                                         js_env &self);

  static void ryml_noderef_get_by_name(v8::Local<v8::Name> key,
                                       const v8::PropertyCallbackInfo<v8::Value> &pci);

  static void ryml_noderef_set_by_name(v8::Local<v8::Name> key,
                                       v8::Local<v8::Value> value,
                                       const v8::PropertyCallbackInfo<v8::Value> &pci);

  static void ryml_noderef_get_by_idx(uint32_t index,
                                      const v8::PropertyCallbackInfo<v8::Value> &pci);

  static void ryml_noderef_set_by_idx(uint32_t index,
                                      v8::Local<v8::Value> value,
                                      const v8::PropertyCallbackInfo<v8::Value> &pci);

  // -------------------------
  // --- C++ -> Javascript ---
  // -------------------------

  bool invoke_js_function(ryml::NodeRef *ctx,
                          const char *js_function_name,
                          ryml::NodeRef js_args,
                          const std::function <bool (v8::Isolate *,
                                                     ryml::NodeRef,
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
  std::optional<T> eval_as(ryml::NodeRef from,
                           const char *key,
                           const std::optional<T> default_value = std::nullopt) {
    if(!from.has_child(ryml::to_csubstr(key))) {
      return default_value;
    }
    ryml::NodeRef val = from[ryml::to_csubstr(key)];
    if(utils::converter<T>::isType(val)) {
      //eval as primitive value
      return utils::converter<T>::asType(val);
    } else {
      //eval as javascript function

      if(!val.has_child("function")) {
        event_log_->error("failed to read 'function'");
        return std::nullopt;
      }

      ryml::NodeRef function = val["function"];
      ryml::NodeRef js_args;
      if(val.has_child("args")) {
        js_args = val["args"];
        if(!js_args.is_seq()) {
          event_log_->error("function args are not sequence type");
          return std::nullopt;
        }
      }

      //finally invoke the javascript function
      T result;
      std::string error;

      std::string fun_str;
      function >> fun_str;

      bool js_res = invoke_js_function(nullptr,
                                       fun_str.c_str(),
                                       js_args,
      [&](v8::Isolate *isl, ryml::NodeRef js_args, v8::Local<v8::Value> argv[]) -> bool{
        for(uint32_t i = 0; i < js_args.num_children(); ++i) {
          ryml::NodeRef js_arg = js_args[i];
          if(!js_value_from_ryml_noderef(js_arg, argv[i], *this)) {
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
        event_log_->error("failure invoking function:{}, error:{}", fun_str, error);
        return std::nullopt;
      }
      return result;
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

  bool exec_as_function(ryml::NodeRef from,
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

  //the global object template for ryml::NodeRef
  v8::Global<v8::ObjectTemplate> ryml_noderef_template_;

  //support map to keep alive ryml::NodeRef objects inside the js context
  std::unordered_map<void *, std::unordered_map<size_t, ryml::NodeRef>> nodes_holder_;

  //event logger
  std::string event_log_fmt_;
  std::shared_ptr<spdlog::logger> event_log_;
};

}
