#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lexer/token.h"
#include "parser/ast.h"

namespace lumen {

struct ParseError {
  int line;
  std::string message;
};

// Recursive-descent parser with precedence climbing for binary operators.
class Parser {
 public:
  explicit Parser(std::vector<Token> tokens);

  // Parses a full program (sequence of declarations). On error, the offending
  // statement is skipped via synchronization and parsing continues so that all
  // errors are reported together.
  std::vector<StmtPtr> parse();

  const std::vector<ParseError>& errors() const {
    return errors_;
  }
  bool had_error() const {
    return !errors_.empty();
  }

 private:
  // Statements.
  StmtPtr declaration();
  StmtPtr class_declaration();
  StmtPtr function_statement(const std::string& kind);
  StmtPtr var_declaration();
  StmtPtr statement();
  StmtPtr if_statement();
  StmtPtr while_statement();
  StmtPtr for_statement();
  StmtPtr print_statement();
  StmtPtr return_statement();
  StmtPtr block_statement();
  StmtPtr expression_statement();
  std::vector<StmtPtr> block();

  // Expressions, ordered by ascending precedence.
  ExprPtr expression();
  ExprPtr assignment();
  ExprPtr logic_or();
  ExprPtr logic_and();
  ExprPtr equality();
  ExprPtr comparison();
  ExprPtr term();
  ExprPtr factor();
  ExprPtr unary();
  ExprPtr call();
  ExprPtr finish_call(ExprPtr callee);
  ExprPtr primary();

  // Token cursor helpers.
  bool at_end() const {
    return peek().kind == TokenKind::Eof;
  }
  const Token& peek() const {
    return tokens_[current_];
  }
  const Token& previous() const {
    return tokens_[current_ - 1];
  }
  const Token& advance();
  bool check(TokenKind kind) const;
  bool match(std::initializer_list<TokenKind> kinds);
  const Token& consume(TokenKind kind, const std::string& message);

  // Error handling.
  [[noreturn]] void error(const Token& token, const std::string& message);
  void synchronize();

  std::vector<Token> tokens_;
  std::vector<ParseError> errors_;
  std::size_t current_ = 0;
};

}  // namespace lumen
