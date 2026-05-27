#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "lexer/token.h"

namespace lumen {

struct Expr;
struct Stmt;
using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;

// Expression node variants ----------------------------------------------------

struct LiteralExpr {
  Literal value;
  int line;
};

struct UnaryExpr {
  Token op;
  ExprPtr right;
};

struct BinaryExpr {
  ExprPtr left;
  Token op;
  ExprPtr right;
};

struct LogicalExpr {
  ExprPtr left;
  Token op;  // And / Or
  ExprPtr right;
};

struct GroupingExpr {
  ExprPtr expr;
};

struct VariableExpr {
  Token name;
};

struct AssignExpr {
  Token name;
  ExprPtr value;
};

struct CallExpr {
  ExprPtr callee;
  Token paren;  // for error reporting
  std::vector<ExprPtr> arguments;
};

struct GetExpr {
  ExprPtr object;
  Token name;
};

struct SetExpr {
  ExprPtr object;
  Token name;
  ExprPtr value;
};

struct ThisExpr {
  Token keyword;
};

struct SuperExpr {
  Token keyword;
  Token method;
};

struct Expr {
  using Node =
      std::variant<LiteralExpr, UnaryExpr, BinaryExpr, LogicalExpr, GroupingExpr, VariableExpr,
                   AssignExpr, CallExpr, GetExpr, SetExpr, ThisExpr, SuperExpr>;
  Node node;
};

// Statement node variants -----------------------------------------------------

struct ExprStmt {
  ExprPtr expression;
};

struct PrintStmt {
  ExprPtr expression;
};

struct VarStmt {
  Token name;
  ExprPtr initializer;  // may be null
};

struct BlockStmt {
  std::vector<StmtPtr> statements;
};

struct IfStmt {
  ExprPtr condition;
  StmtPtr then_branch;
  StmtPtr else_branch;  // may be null
};

struct WhileStmt {
  ExprPtr condition;
  StmtPtr body;
};

struct FunctionStmt {
  Token name;
  std::vector<Token> params;
  std::vector<StmtPtr> body;
};

struct ReturnStmt {
  Token keyword;
  ExprPtr value;  // may be null
};

struct ClassStmt {
  Token name;
  ExprPtr superclass;  // VariableExpr or null
  std::vector<std::shared_ptr<FunctionStmt>> methods;
};

struct Stmt {
  using Node = std::variant<ExprStmt, PrintStmt, VarStmt, BlockStmt, IfStmt, WhileStmt,
                            FunctionStmt, ReturnStmt, ClassStmt>;
  Node node;
};

// Convenience helpers ---------------------------------------------------------

template <typename T, typename... Args>
inline ExprPtr make_expr(Args&&... args) {
  auto e = std::make_shared<Expr>();
  e->node = T{std::forward<Args>(args)...};
  return e;
}

template <typename T, typename... Args>
inline StmtPtr make_stmt(Args&&... args) {
  auto s = std::make_shared<Stmt>();
  s->node = T{std::forward<Args>(args)...};
  return s;
}

}  // namespace lumen
