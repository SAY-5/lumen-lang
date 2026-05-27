#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "bytecode/chunk.h"
#include "bytecode/gc.h"

namespace lumen {

struct ObjClosure;
struct ObjUpvalue;
struct ObjClass;
struct ObjInstance;
struct ObjBoundMethod;
struct ObjNative;

// Runtime objects that can form reference cycles are garbage collected and held
// by raw pointer (the collector owns them). Functions and natives are acyclic
// and rooted by the constant pool, so they stay shared_ptr.
using NativeObj = std::shared_ptr<ObjNative>;

// The VM's runtime value. nil is std::monostate.
using VmValue = std::variant<std::monostate, double, bool, std::string, ObjClosure*, ObjClass*,
                             ObjInstance*, ObjBoundMethod*, NativeObj>;

// A compiled function: its bytecode chunk, arity, name, and the number of
// upvalues it captures. Owned by shared_ptr (never collected).
struct ObjFunction {
  Chunk chunk;
  int arity = 0;
  int upvalue_count = 0;
  std::string name;
};

// A captured variable. While the enclosing frame is live the upvalue points at
// a stack slot (open); once that frame returns the value is moved inline
// (closed) so the closure keeps working.
struct ObjUpvalue : Obj {
  ObjUpvalue() : Obj(ObjType::Upvalue) {
  }
  VmValue* location = nullptr;  // points into the VM stack while open
  VmValue closed;               // holds the value once closed
  bool is_closed = false;

  VmValue get() const {
    return is_closed ? closed : *location;
  }
  void set(const VmValue& v) {
    if (is_closed)
      closed = v;
    else
      *location = v;
  }
};

// A function paired with the upvalues it closed over.
struct ObjClosure : Obj {
  ObjClosure() : Obj(ObjType::Closure) {
  }
  FunctionObj function;
  std::vector<ObjUpvalue*> upvalues;
};

// A class with its method table (methods are closures).
struct ObjClass : Obj {
  ObjClass() : Obj(ObjType::Class) {
  }
  std::string name;
  std::unordered_map<std::string, ObjClosure*> methods;
};

// A class instance with its mutable fields.
struct ObjInstance : Obj {
  ObjInstance() : Obj(ObjType::Instance) {
  }
  ObjClass* klass = nullptr;
  std::unordered_map<std::string, VmValue> fields;
};

// A method closure bound to the instance it was accessed on.
struct ObjBoundMethod : Obj {
  ObjBoundMethod() : Obj(ObjType::BoundMethod) {
  }
  VmValue receiver;
  ObjClosure* method = nullptr;
};

// A builtin implemented in C++.
struct ObjNative {
  std::string name;
  int arity = 0;
  std::function<VmValue(const std::vector<VmValue>&)> fn;
};

bool vm_values_equal(const VmValue& a, const VmValue& b);
bool vm_is_truthy(const VmValue& v);
std::string vm_stringify(const VmValue& v);

}  // namespace lumen
