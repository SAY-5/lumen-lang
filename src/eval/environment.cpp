#include "eval/environment.h"

namespace lumen {

Value Environment::get(const Token& name) const {
  auto it = values_.find(name.lexeme);
  if (it != values_.end())
    return it->second;
  if (enclosing_)
    return enclosing_->get(name);
  throw RuntimeError(name, "undefined variable '" + name.lexeme + "'");
}

void Environment::assign(const Token& name, Value value) {
  auto it = values_.find(name.lexeme);
  if (it != values_.end()) {
    it->second = std::move(value);
    return;
  }
  if (enclosing_) {
    enclosing_->assign(name, std::move(value));
    return;
  }
  throw RuntimeError(name, "undefined variable '" + name.lexeme + "'");
}

Environment* Environment::ancestor(int distance) {
  Environment* env = this;
  for (int i = 0; i < distance; ++i) {
    env = env->enclosing_.get();
  }
  return env;
}

Value Environment::get_at(int distance, const std::string& name) {
  return ancestor(distance)->values_.at(name);
}

void Environment::assign_at(int distance, const std::string& name, Value value) {
  ancestor(distance)->values_[name] = std::move(value);
}

}  // namespace lumen
