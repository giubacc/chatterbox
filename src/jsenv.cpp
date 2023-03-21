#include "scenario.h"

namespace js {

// --------------
// --- js_env ---
// --------------

std::unique_ptr<v8::Platform> js_env::platform;

bool js_env::init_V8(int argc, const char *argv[])
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

js_env::js_env(cbox::scenario &parent) : parent_(parent)
{}

js_env::~js_env()
{
  if(isolate_) {
    if(!ryml_noderef_template_.IsEmpty()) {
      ryml_noderef_template_.Clear();
    }
    if(!scenario_context_.IsEmpty()) {
      scenario_context_.Clear();
    }
    isolate_->Dispose();
    delete params_.array_buffer_allocator;
  }
  event_log_.reset();
}

int js_env::init(std::shared_ptr<spdlog::logger> &event_log)
{
  event_log_ = event_log;

  //init V8 isolate
  params_.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  if(!(isolate_ = v8::Isolate::New(params_))) {
    return 1;
  }

  return 0;
}

int js_env::reset()
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

  //bind cbk_assert.
  global->Set(v8::String::NewFromUtf8(isolate_,
                                      "assert",
                                      v8::NewStringType::kNormal).ToLocalChecked(),
              v8::FunctionTemplate::New(isolate_, cbk_assert));

  //create new context and set this js_env object with it.
  v8::Local<v8::Context> context = v8::Context::New(isolate_, nullptr, global);
  context->SetAlignedPointerInEmbedderData(1, this);

  scenario_context_.Reset(isolate_, context);
  nodes_holder_.clear();
  processed_scripts_.clear();

  //install scenario objects in the current context
  if(!install_scenario_objects()) {
    event_log_->error("failed to install scenario's objects");
    return 1;
  }

  //load, compile, run scripts
  int res = load_scripts();

  return res;
}

// -----------------------
// --- Context objects ---
// -----------------------

v8::Local<v8::ObjectTemplate> js_env::make_ryml_noderef_template()
{
  v8::EscapableHandleScope handle_scope(isolate_);

  v8::Local<v8::ObjectTemplate> result = v8::ObjectTemplate::New(isolate_);
  result->SetInternalFieldCount(1);
  result->SetHandler(v8::NamedPropertyHandlerConfiguration(ryml_noderef_get_by_name,
                                                           ryml_noderef_set_by_name));

  result->SetHandler(v8::IndexedPropertyHandlerConfiguration(ryml_noderef_get_by_idx,
                                                             ryml_noderef_set_by_idx));

  // Return the result through the current handle scope.
  return handle_scope.Escape(result);
}

bool js_env::install_scenario_objects()
{
  v8::HandleScope handle_scope(isolate_);

  auto &holder = nodes_holder_[&parent_.scenario_out_] = std::unordered_map<size_t, ryml::NodeRef>();
  auto &root = holder[parent_.scenario_out_.root_id()] = parent_.scenario_out_.rootref();
  v8::Local<v8::Object> scenario_out_obj = wrap_ryml_noderef(root);

  // Set the chatterbox_.scenario_out_ object as a property on the global object.
  scenario_context_.Get(isolate_)->Global()
  ->Set(scenario_context_.Get(isolate_),
        v8::String::NewFromUtf8(isolate_, "out", v8::NewStringType::kNormal)
        .ToLocalChecked(),
        scenario_out_obj)
  .FromJust();

  return true;
}

//ryml::NodeRef

v8::Local<v8::Object> js_env::wrap_ryml_noderef(ryml::NodeRef &obj)
{
  v8::EscapableHandleScope handle_scope(isolate_);

  // Fetch the template for creating ryml::NodeRef wrappers.
  // It only has to be created once, which we do on demand.
  if(ryml_noderef_template_.IsEmpty()) {
    v8::Local<v8::ObjectTemplate> raw_template = make_ryml_noderef_template();
    ryml_noderef_template_.Reset(isolate_, raw_template);
  }

  v8::Local<v8::ObjectTemplate> templ =
    v8::Local<v8::ObjectTemplate>::New(isolate_, ryml_noderef_template_);

  // Create an empty ryml::NodeRef wrapper.
  v8::Local<v8::Object> result =
    templ->NewInstance(scenario_context_.Get(isolate_)).ToLocalChecked();

  std::unordered_map<size_t, ryml::NodeRef> *holder = nullptr;
  ryml::NodeRef *target = nullptr;

  auto tree_it = nodes_holder_.find(obj.tree());
  if(tree_it == nodes_holder_.end()) {
    auto ins_pair = nodes_holder_.emplace(obj.tree(), std::unordered_map<size_t, ryml::NodeRef>());
    holder = &ins_pair.first->second;
  } else {
    holder = &tree_it->second;
  }

  auto node_it = holder->find(obj.id());
  if(node_it == holder->end()) {
    auto ins_pair = holder->emplace(obj.id(), obj);
    target = &ins_pair.first->second;
  } else {
    target = &node_it->second;
  }

  // Wrap the raw C++ pointer in an External so it can be referenced
  // from within JavaScript.
  v8::Local<v8::External> ryml_noderef_ptr = v8::External::New(isolate_, target);

  // Store the ryml::NodeRef pointer in the JavaScript wrapper.
  result->SetInternalField(0, ryml_noderef_ptr);

  // Return the result through the current handle scope.
  return handle_scope.Escape(result);
}

ryml::NodeRef *js_env::unwrap_ryml_noderef(v8::Local<v8::Object> obj)
{
  v8::Local<v8::External> field = v8::Local<v8::External>::Cast(obj->GetInternalField(0));
  return static_cast<ryml::NodeRef *>(field->Value());
}

bool js_env::js_value_from_ryml_noderef(ryml::NodeRef &obj_val,
                                        v8::Local<v8::Value> &js_obj_val,
                                        js_env &self)
{
  if(obj_val.is_val() && obj_val.val_is_null()) {
    js_obj_val = v8::Null(self.isolate_);
  } else if(obj_val.is_val() || obj_val.is_keyval()) {
    js_obj_val = v8::String::NewFromUtf8(self.isolate_,
                                         utils::converter<std::string>::asType(obj_val).c_str(),
                                         v8::NewStringType::kNormal).ToLocalChecked();
  } else if(obj_val.is_map() || obj_val.is_seq()) {
    js_obj_val = self.wrap_ryml_noderef(obj_val);
  } else {
    self.event_log_->error("unhandled ryml::NodeRef type {}", obj_val.type());
    return false;
  }

  return true;
}

bool js_env::ryml_noderef_from_js_value(ryml::NodeRef &obj_val,
                                        v8::Local<v8::Value> &js_obj_val,
                                        js_env &self)
{
  v8::HandleScope scope(self.isolate_);
  v8::Local<v8::Context> ctx = self.scenario_context_.Get(self.isolate_);

  if(js_obj_val->IsNull()) {
  } else if(js_obj_val->IsInt32()) {
    obj_val << utils::converter<int32_t>::asType(js_obj_val, self.isolate_);
  } else if(js_obj_val->IsUint32()) {
    obj_val << utils::converter<uint32_t>::asType(js_obj_val, self.isolate_);
  } else if(js_obj_val->IsNumber()) {
    obj_val << utils::converter<double>::asType(js_obj_val, self.isolate_);
  } else if(js_obj_val->IsString()) {
    obj_val << utils::converter<std::string>::asType(js_obj_val, self.isolate_);
  } else if(js_obj_val->IsBoolean()) {
    obj_val << utils::converter<bool>::asType(js_obj_val, self.isolate_);
  } else if(js_obj_val->IsArray()) {
    if(obj_val.is_val()) {
      obj_val.clear_val();
      obj_val.set_type(ryml::SEQ);
    } else {
      obj_val |= ryml::SEQ;
    }
    auto array = v8::Array::Cast(*js_obj_val);
    for(uint32_t i = 0; i<array->Length(); ++i) {
      v8::Local<v8::Value> js_item = array->Get(ctx, i).ToLocalChecked();
      ryml::NodeRef item = obj_val.append_child();
      if(!ryml_noderef_from_js_value(item, js_item, self)) {
        return false;
      }
    }
  } else if(js_obj_val->IsObject()) {
    ryml::NodeRef *obj_ptr = unwrap_ryml_noderef(js_obj_val->ToObject(ctx).ToLocalChecked());
    if(obj_ptr) {
      obj_val |= ryml::MAP;
      utils::set_tree_node(*obj_val.tree(),
                           obj_val,
                           *obj_ptr,
                           self.parent_.ryml_scenario_out_buf_);
    } else {
      self.event_log_->error("failed to unwrap ryml::NodeRef");
      return false;
    }
  } else {
    self.event_log_->error("unhandled v8::Value");
    return false;
  }
  return true;
}

void js_env::ryml_noderef_get_by_name(v8::Local<v8::Name> key,
                                      const v8::PropertyCallbackInfo<v8::Value> &pci)
{
  if(key->IsSymbol()) {
    return;
  }

  v8::HandleScope scope(pci.GetIsolate());

  v8::Local<v8::Context> context = pci.GetIsolate()->GetCurrentContext();
  js_env *self = static_cast<js_env *>(context->GetAlignedPointerFromEmbedderData(1));
  if(!self) {
    return;
  }

  std::string str_key = self->js_obj_to_std_string(v8::Local<v8::String>::Cast(key));
  if(str_key.empty()) {
    self->event_log_->error("empty key");
    return;
  }

  // Fetch the ryml::NodeRef wrapped by this object.
  ryml::NodeRef *obj_ptr = unwrap_ryml_noderef(pci.Holder());
  if(!obj_ptr) {
    self->event_log_->error("failed to unwrap ryml::NodeRef");
    return;
  }

  auto cstr = ryml::to_substr(str_key);
  if(!obj_ptr->has_child(cstr)) {
    return;
  }
  ryml::NodeRef obj_val = (*obj_ptr)[cstr];
  v8::Local<v8::Value> js_obj_val;

  if(js_value_from_ryml_noderef(obj_val, js_obj_val, *self)) {
    pci.GetReturnValue().Set(js_obj_val);
  }
}

void js_env::ryml_noderef_set_by_name(v8::Local<v8::Name> key,
                                      v8::Local<v8::Value> value,
                                      const v8::PropertyCallbackInfo<v8::Value> &pci)
{
  if(key->IsSymbol()) {
    return;
  }

  v8::HandleScope scope(pci.GetIsolate());

  v8::Local<v8::Context> context = pci.GetIsolate()->GetCurrentContext();
  js_env *self = static_cast<js_env *>(context->GetAlignedPointerFromEmbedderData(1));
  if(!self) {
    return;
  }

  std::string str_key = self->js_obj_to_std_string(v8::Local<v8::String>::Cast(key));
  if(str_key.empty()) {
    self->event_log_->error("empty key");
    return;
  }

  // Fetch the ryml::NodeRef wrapped by this object.
  ryml::NodeRef *obj_ptr = unwrap_ryml_noderef(pci.Holder());
  if(!obj_ptr) {
    self->event_log_->error("failed to unwrap ryml::NodeRef");
    return;
  }

  ryml::NodeRef obj_val;
  auto cstr = ryml::to_substr(str_key);
  if(!obj_ptr->has_child(cstr)) {
    ryml::csubstr arena_key = obj_ptr->to_arena(str_key);
    obj_val = (*obj_ptr)[arena_key];
  } else {
    obj_val = (*obj_ptr)[cstr];
  }

  if(ryml_noderef_from_js_value(obj_val, value, *self)) {
    pci.GetReturnValue().Set(value);
  }
}

void js_env::ryml_noderef_get_by_idx(uint32_t index,
                                     const v8::PropertyCallbackInfo<v8::Value> &pci)
{
  v8::HandleScope scope(pci.GetIsolate());

  v8::Local<v8::Context> context = pci.GetIsolate()->GetCurrentContext();
  js_env *self = static_cast<js_env *>(context->GetAlignedPointerFromEmbedderData(1));
  if(!self) {
    return;
  }

  // Fetch the ryml::NodeRef wrapped by this object.
  ryml::NodeRef *obj_ptr = unwrap_ryml_noderef(pci.Holder());
  if(!obj_ptr) {
    self->event_log_->error("failed to unwrap ryml::NodeRef");
    return;
  }

  ryml::NodeRef obj = *obj_ptr;
  if(!obj.is_seq() || index >= obj.num_children()) {
    self->event_log_->error("ryml::NodeRef not sequence or index out of range");
    return;
  }

  ryml::NodeRef obj_val = obj[index];
  v8::Local<v8::Value> js_obj_val;

  if(js_value_from_ryml_noderef(obj_val, js_obj_val, *self)) {
    pci.GetReturnValue().Set(js_obj_val);
  }
}

void js_env::ryml_noderef_set_by_idx(uint32_t index,
                                     v8::Local<v8::Value> value,
                                     const v8::PropertyCallbackInfo<v8::Value> &pci)
{
  v8::HandleScope scope(pci.GetIsolate());

  v8::Local<v8::Context> context = pci.GetIsolate()->GetCurrentContext();
  js_env *self = static_cast<js_env *>(context->GetAlignedPointerFromEmbedderData(1));
  if(!self) {
    return;
  }

  // Fetch the ryml::NodeRef wrapped by this object.
  ryml::NodeRef *obj_ptr = unwrap_ryml_noderef(pci.Holder());
  if(!obj_ptr) {
    self->event_log_->error("failed to unwrap ryml::NodeRef");
    return;
  }

  ryml::NodeRef obj = *obj_ptr;
  if(!obj.is_seq() || index >= obj.num_children()) {
    self->event_log_->error("ryml::NodeRef not sequence or index out of range");
    return;
  }

  ryml::NodeRef obj_val = obj[index];
  if(ryml_noderef_from_js_value(obj_val, value, *self)) {
    pci.GetReturnValue().Set(value);
  }
}

// -------------------------
// --- C++ -> Javascript ---
// -------------------------

bool js_env::invoke_js_function(ryml::NodeRef *ctx,
                                const char *js_function_name,
                                ryml::NodeRef js_args,
                                const std::function <bool (v8::Isolate *isolate,
                                                           ryml::NodeRef js_args,
                                                           v8::Local<v8::Value> argv[])> &prepare_argv,
                                const std::function <bool (v8::Isolate *isolate,
                                                           const v8::Local<v8::Value>& result)> &process_result,
                                std::string &error)
{
  if(!js_function_name || !js_function_name[0]) {
    error = "bad function name";
    return false;
  }

  v8::HandleScope scope(isolate_);
  v8::Local<v8::Context> context = scenario_context_.Get(isolate_);
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
    error = "no such function or it is not a function";
    return false;
  }

  // It is a function; cast it to a Function
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(function_val);

  // Prepare arguments
  const int argc = js_args.num_children() + (ctx ? 1 : 0);
  v8::Local<v8::Value> argv[argc];

  if(ctx) {
    argv[0] = wrap_ryml_noderef(*ctx);
  }

  if(!prepare_argv(isolate_, js_args, &argv[ctx ? 1 : 0])) {
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

  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Value> log_level_arg = args[0];
  uint32_t log_level = log_level_arg->Uint32Value(context).FromMaybe(spdlog::level::level_enum::off);

  v8::Local<v8::Value> marker_arg = args[1];
  v8::String::Utf8Value marker(isolate, marker_arg);

  v8::Local<v8::Value> message_arg = args[2];
  v8::String::Utf8Value message(isolate, message_arg);

  js_env *self = static_cast<js_env *>(context->GetAlignedPointerFromEmbedderData(1));
  if(!self) {
    return;
  }

  self->event_log_->log(utils::get_spdloglvl(log_level), "[{}] {}", *marker, *message);
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
  fpath << self->parent_.ctx_.cfg_.in_path << "/" << *script_path;
  if(self->processed_scripts_.find(fpath.str()) == self->processed_scripts_.end()) {
    self->processed_scripts_.insert(fpath.str());
  } else {
    return;
  }

  //read the script's source
  v8::Local<v8::String> source;
  if(!self->read_script_file(fpath.str().c_str()).ToLocal(&source)) {
    return;
  }
  self->event_log_->trace("cbk_load:{}", *script_path);

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

void js_env::cbk_assert(const v8::FunctionCallbackInfo<v8::Value> &args)
{
  if(args.Length() != 2) {
    return;
  }

  v8::Isolate *isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  js_env *self = static_cast<js_env *>(context->GetAlignedPointerFromEmbedderData(1));
  if(!self) {
    return;
  }

  v8::Local<v8::Value> description_arg = args[0];
  v8::String::Utf8Value description(isolate, description_arg);

  v8::Local<v8::Value> assert_val_arg = args[1];
  if(!assert_val_arg->IsBoolean()) {
    self->event_log_->error("assert:{} - expression cannot be evaluated as boolean", *description);
    return;
  }
  bool assert_val = assert_val_arg->BooleanValue(isolate);

  self->event_log_->set_pattern(ASSERT_LOG_PATTERN);
  self->event_log_->log(assert_val ? spdlog::level::info : spdlog::level::err,
                        "[{}] {}", utils::get_formatted_string(assert_val ? "OK" : "KO",
                                                               (assert_val ? fmt::terminal_color::green : fmt::terminal_color::red),
                                                               fmt::emphasis::bold),
                        utils::get_formatted_string(*description,
                                                    (assert_val ? fmt::terminal_color::green : fmt::terminal_color::red),
                                                    fmt::emphasis::bold));
  self->event_log_->set_pattern(DEF_EVT_LOG_PATTERN);

  if(!assert_val) {
    self->parent_.set_assert_failure();
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

  if((dir = opendir(parent_.ctx_.cfg_.in_path.c_str())) != nullptr) {
    while((ent = readdir(dir)) != nullptr) {
      if(strcmp(ent->d_name,".") && strcmp(ent->d_name,"..")) {
        struct stat info;
        std::ostringstream fpath;
        fpath << parent_.ctx_.cfg_.in_path << "/" << ent->d_name;
        if(processed_scripts_.find(fpath.str()) == processed_scripts_.end()) {
          processed_scripts_.insert(fpath.str());
        } else {
          continue;
        }

        stat(fpath.str().c_str(), &info);
        if(!S_ISDIR(info.st_mode)) {
          if(!utils::ends_with(ent->d_name, ".js")) {
            continue;
          }
          event_log_->trace("load_scripts:{}", ent->d_name);

          //read the script's source
          v8::Local<v8::String> source;
          if(!read_script_file(fpath.str().c_str()).ToLocal(&source)) {
            res = 1;
            break;
          }

          //compile the script
          std::string error;
          v8::Local<v8::Script> script;
          if(!compile_script(source, script, error)) {
            event_log_->error("error compiling script file:{} {}", ent->d_name, error);
            res = 1;
            break;
          }

          v8::Local<v8::Value> result;
          if(!run_script(script, result, error)) {
            event_log_->error("error running script: {}", error);
            res = 1;
            break;
          }
        }
      }
    }

    if(closedir(dir)) {
      event_log_->error("{}", strerror(errno));
    }
  } else {
    event_log_->error("{}", strerror(errno));
    res = 1;
  }

  return res;
}

bool js_env::compile_script(const v8::Local<v8::String> &script_src,
                            v8::Local<v8::Script> &script,
                            std::string &error)
{
  v8::Local<v8::Context> context = scenario_context_.Get(isolate_);
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
  v8::Local<v8::Context> context = scenario_context_.Get(isolate_);
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate_);

  if(!script->Run(scenario_context_.Get(isolate_)).ToLocal(&result)) {
    v8::String::Utf8Value utf8err(isolate_, try_catch.Exception());
    error = *utf8err;
    return false;
  }
  return true;
}

// Reads a script into a v8 string.
v8::MaybeLocal<v8::String> js_env::read_script_file(const std::string &name)
{
  std::vector<char> content;
  if(!utils::file_get_contents(name.c_str(), content, event_log_.get())) {
    return v8::MaybeLocal<v8::String>();
  }

  v8::MaybeLocal<v8::String> result = v8::String::NewFromUtf8(isolate_,
                                                              content.data(),
                                                              v8::NewStringType::kNormal,
                                                              static_cast<int>(content.size()));

  return result;
}

bool js_env::exec_as_function(ryml::NodeRef from,
                              const char *key,
                              bool optional)
{
  if(!from.has_child(ryml::to_csubstr(key))) {
    return optional;
  }
  ryml::NodeRef val = from[ryml::to_csubstr(key)];

  if(!val.has_child("function")) {
    event_log_->error("failed to read 'function'");
    return false;
  }

  ryml::ConstNodeRef function = val["function"];

  ryml::NodeRef js_args;
  if(val.has_child("args")) {
    js_args = val["args"];
    if(!js_args.is_seq()) {
      event_log_->error("function args are not sequence type");
      return false;
    }
  }

  // finally invoke the javascript function
  std::string result;
  std::string error;

  std::string fun_str;
  function >> fun_str;

  bool js_res = invoke_js_function(&from,
                                   fun_str.c_str(),
                                   js_args,
  [&](v8::Isolate *isl, ryml::NodeRef js_args, v8::Local<v8::Value> argv[]) -> bool {
    for(uint32_t i = 0; i < js_args.num_children(); ++i)
    {
      ryml::NodeRef js_arg = js_args[i];
      if(!js_value_from_ryml_noderef(js_arg, argv[i], *this)) {
        return false;
      }
    }
    return true;
  },
  [&](v8::Isolate *isl, const v8::Local<v8::Value> &res) -> bool {
    return true;
  },
  error);

  if(!js_res) {
    event_log_->error("failure invoking function:{}, error:{}", fun_str, error);
    return false;
  }
  return true;
}

std::string js_env::js_obj_to_std_string(v8::Local<v8::Value> value)
{
  v8::String::Utf8Value utf8_value(isolate_, value);
  return std::string(*utf8_value);
}

}
