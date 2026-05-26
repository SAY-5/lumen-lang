#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "lexer/token.h"

namespace lumen {

struct LexError {
  int line;
  std::string message;
};

class Lexer {
 public:
  explicit Lexer(std::string source);
  std::vector<Token> scan_tokens();
  const std::vector<LexError>& errors() const { return errors_; }
  bool had_error() const { return !errors_.empty(); }

 private:
  void scan_one();
  bool at_end() const { return current_ >= source_.size(); }
  char advance() { return source_[current_++]; }
  bool match(char expected);
  char peek() const { return at_end() ? '\0' : source_[current_]; }
  char peek_next() const {
    return current_ + 1 >= source_.size() ? '\0' : source_[current_ + 1];
  }

  void add(TokenKind k) { add(k, Literal{}); }
  void add(TokenKind k, Literal lit);

  void string_literal();
  void number_literal();
  void identifier();

  static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  }
  static bool is_digit(char c) { return c >= '0' && c <= '9'; }
  static bool is_alnum(char c) { return is_alpha(c) || is_digit(c); }

  std::string source_;
  std::vector<Token> tokens_;
  std::vector<LexError> errors_;
  std::size_t start_ = 0;
  std::size_t current_ = 0;
  int line_ = 1;
};

}  // namespace lumen
