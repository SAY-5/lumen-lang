#include "bytecode/vm_value.h"

#include <cmath>
#include <sstream>

namespace lumen {

bool vm_is_truthy(const VmValue& v) {
  if (std::holds_alternative<std::monostate>(v))
    return false;
  if (std::holds_alternative<bool>(v))
    return std::get<bool>(v);
  return true;
}

bool vm_values_equal(const VmValue& a, const VmValue& b) {
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
  if (std::holds_alternative<ClosureObj>(a))
    return std::get<ClosureObj>(a) == std::get<ClosureObj>(b);
  if (std::holds_alternative<ClassObj>(a))
    return std::get<ClassObj>(a) == std::get<ClassObj>(b);
  if (std::holds_alternative<InstanceObj>(a))
    return std::get<InstanceObj>(a) == std::get<InstanceObj>(b);
  if (std::holds_alternative<BoundMethodObj>(a))
    return std::get<BoundMethodObj>(a) == std::get<BoundMethodObj>(b);
  if (std::holds_alternative<NativeObj>(a))
    return std::get<NativeObj>(a) == std::get<NativeObj>(b);
  return false;
}

namespace {

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

std::string vm_stringify(const VmValue& v) {
  if (std::holds_alternative<std::monostate>(v))
    return "nil";
  if (std::holds_alternative<double>(v))
    return format_number(std::get<double>(v));
  if (std::holds_alternative<bool>(v))
    return std::get<bool>(v) ? "true" : "false";
  if (std::holds_alternative<std::string>(v))
    return std::get<std::string>(v);
  if (std::holds_alternative<ClosureObj>(v)) {
    return "<fn " + std::get<ClosureObj>(v)->function->name + ">";
  }
  if (std::holds_alternative<ClassObj>(v))
    return std::get<ClassObj>(v)->name;
  if (std::holds_alternative<InstanceObj>(v)) {
    return std::get<InstanceObj>(v)->klass->name + " instance";
  }
  if (std::holds_alternative<BoundMethodObj>(v)) {
    return "<fn " + std::get<BoundMethodObj>(v)->method->function->name + ">";
  }
  if (std::holds_alternative<NativeObj>(v))
    return "<native fn " + std::get<NativeObj>(v)->name + ">";
  return "nil";
}

}  // namespace lumen
