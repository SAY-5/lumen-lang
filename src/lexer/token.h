#pragma once

#include <string>
#include <variant>

namespace lumen {

enum class TokenKind {
  // Single-character punctuation
  LeftParen,
  RightParen,
  LeftBrace,
  RightBrace,
  Comma,
  Dot,
  Minus,
  Plus,
  Semicolon,
  Slash,
  Star,

  // One or two character punctuation
  Bang,
  BangEqual,
  Equal,
  EqualEqual,
  Greater,
  GreaterEqual,
  Less,
  LessEqual,

  // Literals
  Identifier,
  String,
  Number,

  // Keywords
  And,
  Class,
  Else,
  False,
  Fun,
  For,
  If,
  Nil,
  Or,
  Print,
  Return,
  Super,
  This,
  True,
  Var,
  While,

  Eof,
};

using Literal = std::variant<std::monostate, double, std::string, bool>;

struct Token {
  TokenKind kind;
  std::string lexeme;
  Literal literal;
  int line;
};

inline const char* token_kind_name(TokenKind k) {
  switch (k) {
    case TokenKind::LeftParen:
      return "LeftParen";
    case TokenKind::RightParen:
      return "RightParen";
    case TokenKind::LeftBrace:
      return "LeftBrace";
    case TokenKind::RightBrace:
      return "RightBrace";
    case TokenKind::Comma:
      return "Comma";
    case TokenKind::Dot:
      return "Dot";
    case TokenKind::Minus:
      return "Minus";
    case TokenKind::Plus:
      return "Plus";
    case TokenKind::Semicolon:
      return "Semicolon";
    case TokenKind::Slash:
      return "Slash";
    case TokenKind::Star:
      return "Star";
    case TokenKind::Bang:
      return "Bang";
    case TokenKind::BangEqual:
      return "BangEqual";
    case TokenKind::Equal:
      return "Equal";
    case TokenKind::EqualEqual:
      return "EqualEqual";
    case TokenKind::Greater:
      return "Greater";
    case TokenKind::GreaterEqual:
      return "GreaterEqual";
    case TokenKind::Less:
      return "Less";
    case TokenKind::LessEqual:
      return "LessEqual";
    case TokenKind::Identifier:
      return "Identifier";
    case TokenKind::String:
      return "String";
    case TokenKind::Number:
      return "Number";
    case TokenKind::And:
      return "And";
    case TokenKind::Class:
      return "Class";
    case TokenKind::Else:
      return "Else";
    case TokenKind::False:
      return "False";
    case TokenKind::Fun:
      return "Fun";
    case TokenKind::For:
      return "For";
    case TokenKind::If:
      return "If";
    case TokenKind::Nil:
      return "Nil";
    case TokenKind::Or:
      return "Or";
    case TokenKind::Print:
      return "Print";
    case TokenKind::Return:
      return "Return";
    case TokenKind::Super:
      return "Super";
    case TokenKind::This:
      return "This";
    case TokenKind::True:
      return "True";
    case TokenKind::Var:
      return "Var";
    case TokenKind::While:
      return "While";
    case TokenKind::Eof:
      return "Eof";
  }
  return "Unknown";
}

}  // namespace lumen
