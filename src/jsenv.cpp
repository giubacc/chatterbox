#include "chatterbox.h"

namespace js {

std::unique_ptr<v8::Platform> js_env::platform;

bool js_env::init_V8(int argc, char *argv[])
{
  // Initialize V8.
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  return v8::V8::Initialize();
}

void js_env::stop_V8()
{
  // Destroy V8.
  v8::V8::Dispose();
}

js_env::js_env(rest::chatterbox &chatterbox) : chatterbox_(chatterbox)
{}

js_env::~js_env()
{
  if(isolate_) {
    if(!current_scenario_context_.IsEmpty()) {
      current_scenario_context_.Empty();
    }
    isolate_->Dispose();
    delete params_.array_buffer_allocator;
  }
}

int js_env::init(std::shared_ptr<spdlog::logger> &event_log)
{
  event_log_ = event_log;

  //init V8 isolate
  params_.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  if(!(isolate_ = v8::Isolate::New(params_))) {
    return -1;
  }

  return 0;
}

int js_env::renew_scenario_context()
{
  // Create a handle scope to hold the temporary references.
  v8::HandleScope renew_scenario_context_scope(isolate_);

  // Create a template for the global object where we set the
  // built-in global functions.
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate_);

  //bind cbk_log.
  global->Set(v8::String::NewFromUtf8(isolate_,
                                      "log",
                                      v8::NewStringType::kNormal).ToLocalChecked(),
              v8::FunctionTemplate::New(isolate_, cbk_log));


  //bind cbk_load.
  global->Set(v8::String::NewFromUtf8(isolate_,
                                      "load",
                                      v8::NewStringType::kNormal).ToLocalChecked(),
              v8::FunctionTemplate::New(isolate_, cbk_load));

  //create new context and set this js_env object with it.
  v8::Local<v8::Context> context = v8::Context::New(isolate_, nullptr, global);
  context->SetAlignedPointerInEmbedderData(1, this);

  //reset current_scenario_context_
  current_scenario_context_.Reset(isolate_, context);

  //load, compile, run scripts
  int res = load_scripts();

  return res;
}

// -------------------------
// --- C++ -> Javascript ---
// -------------------------

bool js_env::invoke_js_function(const char *js_function_name,
                                const std::vector<std::string> &args,
                                const std::function <bool (v8::Isolate *,
                                                           const std::vector<std::string>&,
                                                           v8::Local<v8::Value> [])> &prepare_argv,
                                const std::function <bool (v8::Isolate *,
                                                           const v8::Local<v8::Value>&)> &process_result,
                                std::string &error)
{
  if(!js_function_name || !js_function_name[0]) {
    error = "bad function name";
    return false;
  }

  v8::HandleScope scope(isolate_);
  v8::Local<v8::Context> context = current_scenario_context_.Get(isolate_);
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate_);

  // fetch out the function from the global object.
  v8::Local<v8::String> function_name = v8::String::NewFromUtf8(isolate_,
                                                                js_function_name,
                                                                v8::NewStringType::kNormal).ToLocalChecked();

  v8::Local<v8::Value> function_val;

  // If there is no such function, or if it is not a function, bail out.
  if(!context->Global()->Get(context, function_name).ToLocal(&function_val) ||
      !function_val->IsFunction()) {
    return false;
  }

  // It is a function; cast it to a Function
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(function_val);

  // Prepare arguments
  const int argc = args.size();
  v8::Local<v8::Value> argv[argc];

  if(!prepare_argv(isolate_, args, argv)) {
    error = "failed to prepare argv";
    return false;
  }

  // Invoke the function
  v8::Local<v8::Value> result;
  if(!function->Call(context, context->Global(), argc, argv).ToLocal(&result)) {
    v8::String::Utf8Value utf8err(isolate_, try_catch.Exception());
    error = *utf8err;
    return false;
  }

  // Process result and return
  return process_result(isolate_, result);
}

// -------------------------
// --- Javascript -> C++ ---
// -------------------------

void js_env::cbk_log(const v8::FunctionCallbackInfo<v8::Value> &args)
{
  if(args.Length() < 3) {
    return;
  }

  v8::Isolate *isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::Value> log_level_arg = args[0];
  v8::String::Utf8Value log_level(isolate, log_level_arg);

  v8::Local<v8::Value> marker_arg = args[1];
  v8::String::Utf8Value marker(isolate, marker_arg);

  v8::Local<v8::Value> message_arg = args[2];
  v8::String::Utf8Value message(isolate, message_arg);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  js_env *self = static_cast<js_env *>(context->GetAlignedPointerFromEmbedderData(1));
  if(!self) {
    return;
  }

  self->event_log_->log(utils::get_spdloglvl(*log_level), "[{}]{}", *marker, *message);
}

void js_env::cbk_load(const v8::FunctionCallbackInfo<v8::Value> &args)
{
  if(args.Length() != 1) {
    return;
  }

  v8::Isolate *isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::Value> script_path_arg = args[0];
  v8::String::Utf8Value script_path(isolate, script_path_arg);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  js_env *self = static_cast<js_env *>(context->GetAlignedPointerFromEmbedderData(1));
  if(!self) {
    return;
  }

  std::ostringstream fpath;
  fpath << self->chatterbox_.cfg_.source_path << "/" << *script_path;

  //read the script's source
  v8::Local<v8::String> source;
  if(!self->read_script_file(fpath.str().c_str()).ToLocal(&source)) {
    self->event_log_->error("error reading script:{}", *script_path);
    return;
  }

  std::string error;

  //compile the script
  v8::Local<v8::Script> script;
  if(!self->compile_script(source, script, error)) {
    self->event_log_->error("error compiling script:{} {}", *script_path, error);
  }

  //run the script
  v8::Local<v8::Value> result;
  if(!self->run_script(script, result, error)) {
    self->event_log_->error("error running script:{} {}", *script_path, error);
  }
}

// -------------
// --- Utils ---
// -------------

int js_env::load_scripts()
{
  int res = 0;
  DIR *dir;
  struct dirent *ent;

  if((dir = opendir(chatterbox_.cfg_.source_path.c_str())) != nullptr) {
    std::vector<v8::Local<v8::String>> sources;
    std::vector<v8::Local<v8::Script>> scripts;

    while((ent = readdir(dir)) != nullptr) {
      if(strcmp(ent->d_name,".") && strcmp(ent->d_name,"..")) {
        struct stat info;
        std::ostringstream fpath;
        fpath << chatterbox_.cfg_.source_path << "/" << ent->d_name;
        stat(fpath.str().c_str(), &info);
        if(!S_ISDIR(info.st_mode)) {
          if(!utils::ends_with(ent->d_name, ".js")) {
            continue;
          }
          event_log_->trace("loading script:{}", ent->d_name);

          //read the script's source
          sources.emplace_back();
          if(!read_script_file(fpath.str().c_str()).ToLocal(&sources.back())) {
            event_log_->error("error reading script file:{}", ent->d_name);
            res = -1;
            break;
          }

          //compile the script
          std::string error;
          scripts.emplace_back();
          if(!compile_script(sources.back(), scripts.back(), error)) {
            event_log_->error("error compiling script file:{} {}", ent->d_name, error);
            res = -1;
            break;
          }
        }
      }
    }

    if(!res) {
      //run the scripts
      for(auto &script : scripts) {
        v8::Local<v8::Value> result;
        std::string error;
        if(!run_script(script, result, error)) {
          event_log_->error("error running script: {}", error);
          res = -1;
          break;
        }
      }
    }

    if(closedir(dir)) {
      event_log_->error("closedir: {}", strerror(errno));
    }
  } else {
    event_log_->error("opendir: {}", strerror(errno));
    res = -1;
  }

  return res;
}

bool js_env::compile_script(const v8::Local<v8::String> &script_src,
                            v8::Local<v8::Script> &script,
                            std::string &error)
{
  v8::Local<v8::Context> context = current_scenario_context_.Get(isolate_);
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate_);

  if(!v8::Script::Compile(context, script_src).ToLocal(&script)) {
    v8::String::Utf8Value utf8err(isolate_, try_catch.Exception());
    error = *utf8err;
    return false;
  }
  return true;
}

bool js_env::run_script(const v8::Local<v8::Script> &script,
                        v8::Local<v8::Value> &result,
                        std::string &error)
{
  v8::Local<v8::Context> context = current_scenario_context_.Get(isolate_);
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate_);

  if(!script->Run(current_scenario_context_.Get(isolate_)).ToLocal(&result)) {
    v8::String::Utf8Value utf8err(isolate_, try_catch.Exception());
    error = *utf8err;
    return false;
  }
  return true;
}

// Reads a script into a v8 string.
v8::MaybeLocal<v8::String> js_env::read_script_file(const std::string &name)
{
  std::stringstream content;
  if(utils::read_file(name.c_str(), content, event_log_.get())) {
    event_log_->error("[read_file] {}", name.c_str());
    return v8::MaybeLocal<v8::String>();
  }

  v8::MaybeLocal<v8::String> result = v8::String::NewFromUtf8(isolate_,
                                                              content.str().c_str(),
                                                              v8::NewStringType::kNormal,
                                                              static_cast<int>(content.str().size()));

  return result;
}

}
