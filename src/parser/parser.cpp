#include "parser/parser.h"

#include <stdexcept>
#include <utility>

namespace lumen {

namespace {

// Thrown internally to unwind to the nearest synchronization point.
struct ParseAbort {};

}  // namespace

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {
}

const Token& Parser::advance() {
  if (!at_end())
    ++current_;
  return previous();
}

bool Parser::check(TokenKind kind) const {
  if (at_end())
    return kind == TokenKind::Eof;
  return peek().kind == kind;
}

bool Parser::match(std::initializer_list<TokenKind> kinds) {
  for (TokenKind kind : kinds) {
    if (check(kind)) {
      advance();
      return true;
    }
  }
  return false;
}

const Token& Parser::consume(TokenKind kind, const std::string& message) {
  if (check(kind))
    return advance();
  error(peek(), message);
}

void Parser::error(const Token& token, const std::string& message) {
  std::string where = token.kind == TokenKind::Eof ? " at end" : " at '" + token.lexeme + "'";
  errors_.push_back({token.line, message + where});
  throw ParseAbort{};
}

void Parser::synchronize() {
  advance();
  while (!at_end()) {
    if (previous().kind == TokenKind::Semicolon)
      return;
    switch (peek().kind) {
      case TokenKind::Class:
      case TokenKind::Fun:
      case TokenKind::Var:
      case TokenKind::For:
      case TokenKind::If:
      case TokenKind::While:
      case TokenKind::Print:
      case TokenKind::Return:
        return;
      default:
        advance();
    }
  }
}

std::vector<StmtPtr> Parser::parse() {
  std::vector<StmtPtr> statements;
  while (!at_end()) {
    try {
      statements.push_back(declaration());
    } catch (const ParseAbort&) {
      synchronize();
    }
  }
  return statements;
}

StmtPtr Parser::declaration() {
  if (match({TokenKind::Class}))
    return class_declaration();
  if (match({TokenKind::Fun}))
    return function_statement("function");
  if (match({TokenKind::Var}))
    return var_declaration();
  return statement();
}

StmtPtr Parser::class_declaration() {
  Token name = consume(TokenKind::Identifier, "expected class name");
  ExprPtr superclass = nullptr;
  if (match({TokenKind::Less})) {
    Token super_name = consume(TokenKind::Identifier, "expected superclass name");
    superclass = make_expr<VariableExpr>(super_name);
  }
  consume(TokenKind::LeftBrace, "expected '{' before class body");
  std::vector<std::shared_ptr<FunctionStmt>> methods;
  while (!check(TokenKind::RightBrace) && !at_end()) {
    StmtPtr method = function_statement("method");
    methods.push_back(std::make_shared<FunctionStmt>(std::get<FunctionStmt>(method->node)));
  }
  consume(TokenKind::RightBrace, "expected '}' after class body");
  return make_stmt<ClassStmt>(name, superclass, std::move(methods));
}

StmtPtr Parser::function_statement(const std::string& kind) {
  Token name = consume(TokenKind::Identifier, "expected " + kind + " name");
  consume(TokenKind::LeftParen, "expected '(' after " + kind + " name");
  std::vector<Token> params;
  if (!check(TokenKind::RightParen)) {
    do {
      if (params.size() >= 255) {
        error(peek(), "cannot have more than 255 parameters");
      }
      params.push_back(consume(TokenKind::Identifier, "expected parameter name"));
    } while (match({TokenKind::Comma}));
  }
  consume(TokenKind::RightParen, "expected ')' after parameters");
  consume(TokenKind::LeftBrace, "expected '{' before " + kind + " body");
  std::vector<StmtPtr> body = block();
  return make_stmt<FunctionStmt>(name, std::move(params), std::move(body));
}

StmtPtr Parser::var_declaration() {
  Token name = consume(TokenKind::Identifier, "expected variable name");
  ExprPtr initializer = nullptr;
  if (match({TokenKind::Equal})) {
    initializer = expression();
  }
  consume(TokenKind::Semicolon, "expected ';' after variable declaration");
  return make_stmt<VarStmt>(name, initializer);
}

StmtPtr Parser::statement() {
  if (match({TokenKind::If}))
    return if_statement();
  if (match({TokenKind::While}))
    return while_statement();
  if (match({TokenKind::For}))
    return for_statement();
  if (match({TokenKind::Print}))
    return print_statement();
  if (match({TokenKind::Return}))
    return return_statement();
  if (match({TokenKind::LeftBrace}))
    return block_statement();
  return expression_statement();
}

StmtPtr Parser::if_statement() {
  consume(TokenKind::LeftParen, "expected '(' after 'if'");
  ExprPtr condition = expression();
  consume(TokenKind::RightParen, "expected ')' after if condition");
  StmtPtr then_branch = statement();
  StmtPtr else_branch = nullptr;
  if (match({TokenKind::Else})) {
    else_branch = statement();
  }
  return make_stmt<IfStmt>(condition, then_branch, else_branch);
}

StmtPtr Parser::while_statement() {
  consume(TokenKind::LeftParen, "expected '(' after 'while'");
  ExprPtr condition = expression();
  consume(TokenKind::RightParen, "expected ')' after while condition");
  StmtPtr body = statement();
  return make_stmt<WhileStmt>(condition, body);
}

// Desugars 'for (init; cond; incr) body' into an equivalent while loop.
StmtPtr Parser::for_statement() {
  consume(TokenKind::LeftParen, "expected '(' after 'for'");

  StmtPtr initializer;
  if (match({TokenKind::Semicolon})) {
    initializer = nullptr;
  } else if (match({TokenKind::Var})) {
    initializer = var_declaration();
  } else {
    initializer = expression_statement();
  }

  ExprPtr condition = nullptr;
  if (!check(TokenKind::Semicolon)) {
    condition = expression();
  }
  consume(TokenKind::Semicolon, "expected ';' after loop condition");

  ExprPtr increment = nullptr;
  if (!check(TokenKind::RightParen)) {
    increment = expression();
  }
  consume(TokenKind::RightParen, "expected ')' after for clauses");

  StmtPtr body = statement();

  if (increment != nullptr) {
    std::vector<StmtPtr> stmts;
    stmts.push_back(body);
    stmts.push_back(make_stmt<ExprStmt>(increment));
    body = make_stmt<BlockStmt>(std::move(stmts));
  }

  if (condition == nullptr) {
    condition = make_expr<LiteralExpr>(Literal{true}, previous().line);
  }
  body = make_stmt<WhileStmt>(condition, body);

  if (initializer != nullptr) {
    std::vector<StmtPtr> stmts;
    stmts.push_back(initializer);
    stmts.push_back(body);
    body = make_stmt<BlockStmt>(std::move(stmts));
  }
  return body;
}

StmtPtr Parser::print_statement() {
  ExprPtr value = expression();
  consume(TokenKind::Semicolon, "expected ';' after value");
  return make_stmt<PrintStmt>(value);
}

StmtPtr Parser::return_statement() {
  Token keyword = previous();
  ExprPtr value = nullptr;
  if (!check(TokenKind::Semicolon)) {
    value = expression();
  }
  consume(TokenKind::Semicolon, "expected ';' after return value");
  return make_stmt<ReturnStmt>(keyword, value);
}

StmtPtr Parser::block_statement() {
  return make_stmt<BlockStmt>(block());
}

std::vector<StmtPtr> Parser::block() {
  std::vector<StmtPtr> statements;
  while (!check(TokenKind::RightBrace) && !at_end()) {
    statements.push_back(declaration());
  }
  consume(TokenKind::RightBrace, "expected '}' after block");
  return statements;
}

StmtPtr Parser::expression_statement() {
  ExprPtr expr = expression();
  consume(TokenKind::Semicolon, "expected ';' after expression");
  return make_stmt<ExprStmt>(expr);
}

ExprPtr Parser::expression() {
  return assignment();
}

ExprPtr Parser::assignment() {
  ExprPtr expr = logic_or();
  if (match({TokenKind::Equal})) {
    Token equals = previous();
    ExprPtr value = assignment();
    if (auto* var = std::get_if<VariableExpr>(&expr->node)) {
      return make_expr<AssignExpr>(var->name, value);
    }
    if (auto* get = std::get_if<GetExpr>(&expr->node)) {
      return make_expr<SetExpr>(get->object, get->name, value);
    }
    error(equals, "invalid assignment target");
  }
  return expr;
}

ExprPtr Parser::logic_or() {
  ExprPtr expr = logic_and();
  while (match({TokenKind::Or})) {
    Token op = previous();
    ExprPtr right = logic_and();
    expr = make_expr<LogicalExpr>(expr, op, right);
  }
  return expr;
}

ExprPtr Parser::logic_and() {
  ExprPtr expr = equality();
  while (match({TokenKind::And})) {
    Token op = previous();
    ExprPtr right = equality();
    expr = make_expr<LogicalExpr>(expr, op, right);
  }
  return expr;
}

ExprPtr Parser::equality() {
  ExprPtr expr = comparison();
  while (match({TokenKind::BangEqual, TokenKind::EqualEqual})) {
    Token op = previous();
    ExprPtr right = comparison();
    expr = make_expr<BinaryExpr>(expr, op, right);
  }
  return expr;
}

ExprPtr Parser::comparison() {
  ExprPtr expr = term();
  while (
      match({TokenKind::Greater, TokenKind::GreaterEqual, TokenKind::Less, TokenKind::LessEqual})) {
    Token op = previous();
    ExprPtr right = term();
    expr = make_expr<BinaryExpr>(expr, op, right);
  }
  return expr;
}

ExprPtr Parser::term() {
  ExprPtr expr = factor();
  while (match({TokenKind::Minus, TokenKind::Plus})) {
    Token op = previous();
    ExprPtr right = factor();
    expr = make_expr<BinaryExpr>(expr, op, right);
  }
  return expr;
}

ExprPtr Parser::factor() {
  ExprPtr expr = unary();
  while (match({TokenKind::Slash, TokenKind::Star})) {
    Token op = previous();
    ExprPtr right = unary();
    expr = make_expr<BinaryExpr>(expr, op, right);
  }
  return expr;
}

ExprPtr Parser::unary() {
  if (match({TokenKind::Bang, TokenKind::Minus})) {
    Token op = previous();
    ExprPtr right = unary();
    return make_expr<UnaryExpr>(op, right);
  }
  return call();
}

ExprPtr Parser::call() {
  ExprPtr expr = primary();
  while (true) {
    if (match({TokenKind::LeftParen})) {
      expr = finish_call(expr);
    } else if (match({TokenKind::Dot})) {
      Token name = consume(TokenKind::Identifier, "expected property name after '.'");
      expr = make_expr<GetExpr>(expr, name);
    } else {
      break;
    }
  }
  return expr;
}

ExprPtr Parser::finish_call(ExprPtr callee) {
  std::vector<ExprPtr> arguments;
  if (!check(TokenKind::RightParen)) {
    do {
      if (arguments.size() >= 255) {
        error(peek(), "cannot have more than 255 arguments");
      }
      arguments.push_back(expression());
    } while (match({TokenKind::Comma}));
  }
  Token paren = consume(TokenKind::RightParen, "expected ')' after arguments");
  return make_expr<CallExpr>(callee, paren, std::move(arguments));
}

ExprPtr Parser::primary() {
  if (match({TokenKind::False})) {
    return make_expr<LiteralExpr>(Literal{false}, previous().line);
  }
  if (match({TokenKind::True})) {
    return make_expr<LiteralExpr>(Literal{true}, previous().line);
  }
  if (match({TokenKind::Nil})) {
    return make_expr<LiteralExpr>(Literal{std::monostate{}}, previous().line);
  }
  if (match({TokenKind::Number, TokenKind::String})) {
    return make_expr<LiteralExpr>(previous().literal, previous().line);
  }
  if (match({TokenKind::This})) {
    return make_expr<ThisExpr>(previous());
  }
  if (match({TokenKind::Super})) {
    Token keyword = previous();
    consume(TokenKind::Dot, "expected '.' after 'super'");
    Token method = consume(TokenKind::Identifier, "expected superclass method name");
    return make_expr<SuperExpr>(keyword, method);
  }
  if (match({TokenKind::Identifier})) {
    return make_expr<VariableExpr>(previous());
  }
  if (match({TokenKind::LeftParen})) {
    ExprPtr expr = expression();
    consume(TokenKind::RightParen, "expected ')' after expression");
    return make_expr<GroupingExpr>(expr);
  }
  error(peek(), "expected expression");
}

}  // namespace lumen
