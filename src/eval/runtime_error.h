#pragma once

#include <stdexcept>
#include <string>

#include "lexer/token.h"

namespace lumen {

// Raised when evaluation hits a semantic error (type mismatch, undefined
// variable, etc). Carries the offending token's line for reporting.
class RuntimeError : public std::runtime_error {
 public:
  RuntimeError(Token token, const std::string& message)
      : std::runtime_error(message), token_(std::move(token)) {
  }

  const Token& token() const {
    return token_;
  }

 private:
  Token token_;
};

}  // namespace lumen
