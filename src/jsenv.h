#pragma once

#include "libplatform/libplatform.h"
#include "v8-context.h"
#include "v8-initialization.h"
#include "v8-isolate.h"
#include "v8-local-handle.h"
#include "v8-primitive.h"
#include "v8-script.h"
#include "v8-object.h"
#include "v8-template.h"
#include "v8-exception.h"
#include "v8-function.h"
#include "globals.h"

namespace rest {
struct chatterbox;
}

namespace js {

struct js_env {

  //global V8 platform
  static std::unique_ptr<v8::Platform> platform;

  static bool init_V8(int argc, char *argv[]);
  static void stop_V8();

  js_env(rest::chatterbox &chatterbox);
  ~js_env();

  int init();

  int renew_scenario_context();

  // -------------------------
  // --- C++ -> Javascript ---
  // -------------------------

  bool invoke_js_function(const char *js_function_name,
                          const std::vector<std::string> &args,
                          const std::function <bool (v8::Isolate *,
                                                     const std::vector<std::string>&,
                                                     v8::Local<v8::Value> [])> &prepare_argv,
                          const std::function <bool (v8::Isolate *,
                                                     const v8::Local<v8::Value>&)> &process_result,
                          std::string &error);

  // -------------------------
  // --- Javascript -> C++ ---
  // -------------------------

  static void cbk_log(const v8::FunctionCallbackInfo<v8::Value> &args);
  static void cbk_load(const v8::FunctionCallbackInfo<v8::Value> &args);

  // -------------
  // --- Utils ---
  // -------------

  int load_scripts();
  v8::MaybeLocal<v8::String> read_script_file(const std::string &name);

  bool compile_script(const v8::Local<v8::String> &script_src,
                      v8::Local<v8::Script> &script,
                      std::string &error);

  bool run_script(const v8::Local<v8::Script> &script,
                  v8::Local<v8::Value> &result,
                  std::string &error);

  // -----------
  // --- rep ---
  // -----------

  //chatterbox
  rest::chatterbox &chatterbox_;

  //the parameters used to instantiate the Isolate.
  v8::Isolate::CreateParams params_;

  //the Isolate associated with this object.
  v8::Isolate *isolate_ = nullptr;

  //the context associated with this object,
  //it is disposed and then recreated on every scenario.
  v8::Global<v8::Context> current_scenario_context_;

  //logger
  std::string log_fmt_;
  std::shared_ptr<spdlog::logger> log_;
};

}
