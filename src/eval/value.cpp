#include "eval/value.h"

#include <cmath>
#include <sstream>

#include "eval/callable.h"

namespace lumen {

bool values_equal(const Value& a, const Value& b) {
  if (a.index() != b.index())
    return false;
  if (std::holds_alternative<std::monostate>(a))
    return true;
  if (std::holds_alternative<double>(a))
    return std::get<double>(a) == std::get<double>(b);
  if (std::holds_alternative<bool>(a))
    return std::get<bool>(a) == std::get<bool>(b);
  if (std::holds_alternative<std::string>(a))
    return std::get<std::string>(a) == std::get<std::string>(b);
  if (std::holds_alternative<CallablePtr>(a))
    return std::get<CallablePtr>(a) == std::get<CallablePtr>(b);
  if (std::holds_alternative<ClassPtr>(a))
    return std::get<ClassPtr>(a) == std::get<ClassPtr>(b);
  if (std::holds_alternative<InstancePtr>(a))
    return std::get<InstancePtr>(a) == std::get<InstancePtr>(b);
  return false;
}

namespace {

// Formats a double the way Lumen prints numbers: integers without a trailing
// ".0", and other values with the shortest round-trippable representation.
std::string format_number(double d) {
  if (std::isnan(d))
    return "nan";
  if (std::isinf(d))
    return d < 0 ? "-inf" : "inf";
  if (d == static_cast<long long>(d) && std::abs(d) < 1e15) {
    return std::to_string(static_cast<long long>(d));
  }
  std::ostringstream os;
  os.precision(15);
  os << d;
  return os.str();
}

}  // namespace

std::string stringify(const Value& v) {
  if (std::holds_alternative<std::monostate>(v))
    return "nil";
  if (std::holds_alternative<double>(v))
    return format_number(std::get<double>(v));
  if (std::holds_alternative<bool>(v))
    return std::get<bool>(v) ? "true" : "false";
  if (std::holds_alternative<std::string>(v))
    return std::get<std::string>(v);
  if (std::holds_alternative<CallablePtr>(v))
    return std::get<CallablePtr>(v)->to_string();
  if (std::holds_alternative<ClassPtr>(v))
    return std::get<ClassPtr>(v)->to_string();
  if (std::holds_alternative<InstancePtr>(v))
    return std::get<InstancePtr>(v)->to_string();
  return "nil";
}

}  // namespace lumen
