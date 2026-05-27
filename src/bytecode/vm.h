#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "bytecode/vm_value.h"

namespace lumen {

// Result of running a chunk.
enum class InterpretResult { Ok, RuntimeError };

// A stack-based virtual machine executing compiled Lumen bytecode.
class VM {
 public:
  VM();

  // Runs the top-level function. Program output accumulates in output(); on a
  // runtime error, error_message() holds the diagnostic.
  InterpretResult run(const FunctionObj& script);

  const std::string& output() const {
    return output_;
  }
  const std::string& error_message() const {
    return error_message_;
  }

 private:
  struct CallFrame {
    ClosureObj closure;
    std::size_t ip;         // index into closure->function->chunk.code
    std::size_t slot_base;  // index of this frame's slot 0 in the value stack
  };

  InterpretResult execute();

  bool call_value(const VmValue& callee, int arg_count);
  bool call(const ClosureObj& closure, int arg_count);
  bool invoke(const std::string& name, int arg_count);
  bool invoke_from_class(const ClassObj& klass, const std::string& name, int arg_count);
  bool bind_method(const ClassObj& klass, const std::string& name);
  UpvalueObj capture_upvalue(VmValue* local);
  void close_upvalues(VmValue* last);
  void define_native(const std::string& name, int arity,
                     std::function<VmValue(const std::vector<VmValue>&)> fn);

  void push(VmValue value) {
    stack_.push_back(std::move(value));
  }
  VmValue pop() {
    VmValue v = std::move(stack_.back());
    stack_.pop_back();
    return v;
  }
  VmValue& peek(int distance) {
    return stack_[stack_.size() - 1 - distance];
  }

  void runtime_error(const std::string& message);

  std::vector<VmValue> stack_;
  std::vector<CallFrame> frames_;
  std::unordered_map<std::string, VmValue> globals_;
  std::vector<UpvalueObj> open_upvalues_;  // sorted by descending stack address
  std::string output_;
  std::string error_message_;
  bool errored_ = false;
};

}  // namespace lumen
