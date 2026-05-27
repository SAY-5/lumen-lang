#include "eval/function.h"

#include "eval/interpreter.h"

namespace lumen {

CallablePtr LumenFunction::bind(const InstancePtr& instance) const {
  auto env = std::make_shared<Environment>(closure);
  env->define("this", instance);
  return std::make_shared<LumenFunction>(declaration, env, is_initializer);
}

Value LumenFunction::call(Interpreter& interp, const std::vector<Value>& args) {
  auto env = std::make_shared<Environment>(closure);
  for (std::size_t i = 0; i < declaration->params.size(); ++i) {
    env->define(declaration->params[i].lexeme, args[i]);
  }
  try {
    interp.execute_block(declaration->body, env);
  } catch (const ReturnSignal& ret) {
    if (is_initializer)
      return closure->get_at(0, "this");
    return ret.value;
  }
  if (is_initializer)
    return closure->get_at(0, "this");
  return std::monostate{};
}

}  // namespace lumen
