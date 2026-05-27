#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "eval/callable.h"
#include "eval/environment.h"
#include "eval/value.h"
#include "parser/ast.h"

namespace lumen {

// Thrown to unwind a function body when a return statement executes.
struct ReturnSignal {
  Value value;
};

// A user-defined function or method closing over its defining environment.
struct LumenFunction : Callable {
  LumenFunction(std::shared_ptr<FunctionStmt> decl, std::shared_ptr<Environment> closure,
                bool is_initializer)
      : declaration(std::move(decl)), closure(std::move(closure)), is_initializer(is_initializer) {
  }

  std::shared_ptr<FunctionStmt> declaration;
  std::shared_ptr<Environment> closure;
  bool is_initializer;

  // Returns a copy of this function with `this` bound to the given instance.
  CallablePtr bind(const InstancePtr& instance) const;

  int arity() const override {
    return static_cast<int>(declaration->params.size());
  }
  Value call(Interpreter& interp, const std::vector<Value>& args) override;
  std::string to_string() const override {
    return "<fn " + declaration->name.lexeme + ">";
  }
};

// A built-in function implemented in C++.
struct NativeFunction : Callable {
  using Fn = std::function<Value(const std::vector<Value>&)>;

  NativeFunction(std::string name, int arity, Fn fn)
      : name_(std::move(name)), arity_(arity), fn_(std::move(fn)) {
  }

  int arity() const override {
    return arity_;
  }
  Value call(Interpreter&, const std::vector<Value>& args) override {
    return fn_(args);
  }
  std::string to_string() const override {
    return "<native fn " + name_ + ">";
  }

 private:
  std::string name_;
  int arity_;
  Fn fn_;
};

}  // namespace lumen
