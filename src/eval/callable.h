#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "eval/runtime_error.h"
#include "eval/value.h"
#include "lexer/token.h"

namespace lumen {

class Interpreter;

// Anything that can be invoked with arguments: user functions, native
// builtins, and classes (whose call constructs an instance).
struct Callable {
  virtual ~Callable() = default;
  virtual int arity() const = 0;
  virtual Value call(Interpreter& interp, const std::vector<Value>& args) = 0;
  virtual std::string to_string() const = 0;
};

struct ClassValue : Callable, std::enable_shared_from_this<ClassValue> {
  std::string name;
  ClassPtr superclass;  // may be null
  std::unordered_map<std::string, CallablePtr> methods;

  CallablePtr find_method(const std::string& method_name) const;

  int arity() const override;
  Value call(Interpreter& interp, const std::vector<Value>& args) override;
  std::string to_string() const override {
    return name;
  }
};

struct InstanceValue {
  ClassPtr klass;
  std::unordered_map<std::string, Value> fields;

  Value get(const Token& name, const InstancePtr& self);
  void set(const Token& name, Value value) {
    fields[name.lexeme] = std::move(value);
  }
  std::string to_string() const {
    return klass->name + " instance";
  }
};

}  // namespace lumen
