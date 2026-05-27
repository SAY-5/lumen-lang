#include "eval/callable.h"

#include "eval/function.h"
#include "eval/interpreter.h"

namespace lumen {

CallablePtr ClassValue::find_method(const std::string& method_name) const {
  auto it = methods.find(method_name);
  if (it != methods.end())
    return it->second;
  if (superclass)
    return superclass->find_method(method_name);
  return nullptr;
}

int ClassValue::arity() const {
  CallablePtr init = find_method("init");
  if (!init)
    return 0;
  return init->arity();
}

Value ClassValue::call(Interpreter& interp, const std::vector<Value>& args) {
  auto instance = std::make_shared<InstanceValue>();
  instance->klass = std::static_pointer_cast<ClassValue>(shared_from_this());
  CallablePtr init = find_method("init");
  if (init) {
    auto bound = std::static_pointer_cast<LumenFunction>(init)->bind(instance);
    bound->call(interp, args);
  }
  return instance;
}

Value InstanceValue::get(const Token& name, const InstancePtr& self) {
  auto it = fields.find(name.lexeme);
  if (it != fields.end())
    return it->second;
  CallablePtr method = klass->find_method(name.lexeme);
  if (method) {
    return std::static_pointer_cast<LumenFunction>(method)->bind(self);
  }
  throw RuntimeError(name, "undefined property '" + name.lexeme + "'");
}

}  // namespace lumen
