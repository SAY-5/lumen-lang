#pragma once

#include <memory>
#include <string>
#include <vector>

#include "eval/environment.h"
#include "eval/runtime_error.h"
#include "eval/value.h"
#include "parser/ast.h"

namespace lumen {

// Tree-walking evaluator. Writes program output to out_ (defaults to a string
// buffer that callers can inspect, which keeps tests hermetic).
class Interpreter {
 public:
  Interpreter();

  // Runs a program. Returns false and records the message if a RuntimeError
  // escapes; output produced before the error is retained.
  bool interpret(const std::vector<StmtPtr>& statements);

  const std::string& output() const {
    return output_;
  }
  const std::string& error_message() const {
    return error_message_;
  }
  bool had_runtime_error() const {
    return had_error_;
  }

  std::shared_ptr<Environment> globals() {
    return globals_;
  }

  // Executes a block of statements in the given environment. Public because
  // user functions invoke it during a call.
  void execute_block(const std::vector<StmtPtr>& statements, std::shared_ptr<Environment> env);

 private:
  // Statement execution.
  void execute(const StmtPtr& stmt);

  // Expression evaluation.
  Value evaluate(const ExprPtr& expr);

  Value call_value(const Value& callee, const std::vector<Value>& args, const Token& paren);

  void check_number_operand(const Token& op, const Value& operand);
  void check_number_operands(const Token& op, const Value& left, const Value& right);

  std::shared_ptr<Environment> globals_;
  std::shared_ptr<Environment> environment_;
  std::string output_;
  std::string error_message_;
  bool had_error_ = false;
};

}  // namespace lumen
