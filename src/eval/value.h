#pragma once

#include <memory>
#include <string>
#include <variant>

namespace lumen {

struct Callable;       // functions, native functions, classes
struct ClassValue;     // class objects
struct InstanceValue;  // class instances

using CallablePtr = std::shared_ptr<Callable>;
using ClassPtr = std::shared_ptr<ClassValue>;
using InstancePtr = std::shared_ptr<InstanceValue>;

// A dynamically typed runtime value. nil is std::monostate.
using Value =
    std::variant<std::monostate, double, bool, std::string, CallablePtr, ClassPtr, InstancePtr>;

inline bool is_nil(const Value& v) {
  return std::holds_alternative<std::monostate>(v);
}

// Lumen truthiness: nil and false are falsey, everything else is truthy.
inline bool is_truthy(const Value& v) {
  if (std::holds_alternative<std::monostate>(v))
    return false;
  if (std::holds_alternative<bool>(v))
    return std::get<bool>(v);
  return true;
}

bool values_equal(const Value& a, const Value& b);
std::string stringify(const Value& v);

}  // namespace lumen
