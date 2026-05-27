#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "eval/runtime_error.h"
#include "eval/value.h"
#include "lexer/token.h"

namespace lumen {

// A lexical scope mapping names to values, chained to an enclosing scope.
class Environment : public std::enable_shared_from_this<Environment> {
 public:
  Environment() = default;
  explicit Environment(std::shared_ptr<Environment> enclosing) : enclosing_(std::move(enclosing)) {
  }

  void define(const std::string& name, Value value) {
    values_[name] = std::move(value);
  }

  Value get(const Token& name) const;
  void assign(const Token& name, Value value);

  // Direct distance-based access used by the resolver-free fast path: walk up
  // exactly `distance` enclosing scopes.
  Value get_at(int distance, const std::string& name);
  void assign_at(int distance, const std::string& name, Value value);

  const std::shared_ptr<Environment>& enclosing() const {
    return enclosing_;
  }

 private:
  Environment* ancestor(int distance);

  std::unordered_map<std::string, Value> values_;
  std::shared_ptr<Environment> enclosing_;
};

}  // namespace lumen
