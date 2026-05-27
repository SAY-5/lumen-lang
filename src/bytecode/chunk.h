#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace lumen {

struct ObjFunction;
using FunctionObj = std::shared_ptr<ObjFunction>;

// A compile-time constant stored in a chunk. The VM's full runtime value type
// (which also includes closures, classes, and instances) lives in vm_value.h;
// only these literal kinds can appear as compiled constants.
using Constant = std::variant<std::monostate, double, bool, std::string, FunctionObj>;

// A sequence of bytecode with its constant pool and per-instruction line table.
struct Chunk {
  std::vector<std::uint8_t> code;
  std::vector<int> lines;  // parallel to code; source line per byte
  std::vector<Constant> constants;

  void write(std::uint8_t byte, int line) {
    code.push_back(byte);
    lines.push_back(line);
  }

  // Adds a constant and returns its index. Constants are not deduplicated;
  // the compiler may emit the same literal more than once.
  int add_constant(Constant value) {
    constants.push_back(std::move(value));
    return static_cast<int>(constants.size()) - 1;
  }
};

}  // namespace lumen
