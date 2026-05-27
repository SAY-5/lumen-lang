#include "lexer/lexer.h"

#include <unordered_map>
#include <utility>

namespace lumen {

namespace {

const std::unordered_map<std::string, TokenKind>& keywords() {
  static const std::unordered_map<std::string, TokenKind> kw{
      {"and", TokenKind::And},     {"class", TokenKind::Class},   {"else", TokenKind::Else},
      {"false", TokenKind::False}, {"for", TokenKind::For},       {"fun", TokenKind::Fun},
      {"if", TokenKind::If},       {"nil", TokenKind::Nil},       {"or", TokenKind::Or},
      {"print", TokenKind::Print}, {"return", TokenKind::Return}, {"super", TokenKind::Super},
      {"this", TokenKind::This},   {"true", TokenKind::True},     {"var", TokenKind::Var},
      {"while", TokenKind::While},
  };
  return kw;
}

}  // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {
}

void Lexer::add(TokenKind k, Literal lit) {
  std::string lex = source_.substr(start_, current_ - start_);
  tokens_.push_back(Token{k, std::move(lex), std::move(lit), line_});
}

bool Lexer::match(char expected) {
  if (at_end())
    return false;
  if (source_[current_] != expected)
    return false;
  ++current_;
  return true;
}

void Lexer::string_literal() {
  while (!at_end() && peek() != '"') {
    if (peek() == '\n')
      ++line_;
    advance();
  }
  if (at_end()) {
    errors_.push_back({line_, "unterminated string"});
    return;
  }
  // Consume closing quote.
  advance();
  std::string value = source_.substr(start_ + 1, current_ - start_ - 2);
  add(TokenKind::String, Literal{std::move(value)});
}

void Lexer::number_literal() {
  while (is_digit(peek()))
    advance();
  if (peek() == '.' && is_digit(peek_next())) {
    advance();
    while (is_digit(peek()))
      advance();
  }
  std::string text = source_.substr(start_, current_ - start_);
  double v = 0.0;
  try {
    v = std::stod(text);
  } catch (...) {
    errors_.push_back({line_, "invalid number literal: " + text});
    return;
  }
  add(TokenKind::Number, Literal{v});
}

void Lexer::identifier() {
  while (is_alnum(peek()))
    advance();
  std::string text = source_.substr(start_, current_ - start_);
  const auto& kw = keywords();
  auto it = kw.find(text);
  if (it == kw.end()) {
    add(TokenKind::Identifier);
    return;
  }
  TokenKind k = it->second;
  if (k == TokenKind::True) {
    add(k, Literal{true});
  } else if (k == TokenKind::False) {
    add(k, Literal{false});
  } else {
    add(k);
  }
}

void Lexer::scan_one() {
  char c = advance();
  switch (c) {
    case '(':
      add(TokenKind::LeftParen);
      break;
    case ')':
      add(TokenKind::RightParen);
      break;
    case '{':
      add(TokenKind::LeftBrace);
      break;
    case '}':
      add(TokenKind::RightBrace);
      break;
    case ',':
      add(TokenKind::Comma);
      break;
    case '.':
      add(TokenKind::Dot);
      break;
    case '-':
      add(TokenKind::Minus);
      break;
    case '+':
      add(TokenKind::Plus);
      break;
    case ';':
      add(TokenKind::Semicolon);
      break;
    case '*':
      add(TokenKind::Star);
      break;
    case '!':
      add(match('=') ? TokenKind::BangEqual : TokenKind::Bang);
      break;
    case '=':
      add(match('=') ? TokenKind::EqualEqual : TokenKind::Equal);
      break;
    case '<':
      add(match('=') ? TokenKind::LessEqual : TokenKind::Less);
      break;
    case '>':
      add(match('=') ? TokenKind::GreaterEqual : TokenKind::Greater);
      break;
    case '/':
      if (match('/')) {
        while (!at_end() && peek() != '\n')
          advance();
      } else {
        add(TokenKind::Slash);
      }
      break;
    case ' ':
    case '\r':
    case '\t':
      break;
    case '\n':
      ++line_;
      break;
    case '"':
      string_literal();
      break;
    default:
      if (is_digit(c)) {
        number_literal();
      } else if (is_alpha(c)) {
        identifier();
      } else {
        errors_.push_back({line_, std::string("unexpected character: ") + c});
      }
      break;
  }
}

std::vector<Token> Lexer::scan_tokens() {
  while (!at_end()) {
    start_ = current_;
    scan_one();
  }
  tokens_.push_back(Token{TokenKind::Eof, "", Literal{}, line_});
  return tokens_;
}

}  // namespace lumen
