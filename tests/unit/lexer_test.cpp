#include "lexer/lexer.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace lumen;

static std::vector<Token> tok(const std::string& s) {
  Lexer lx(s);
  return lx.scan_tokens();
}

static std::vector<TokenKind> kinds(const std::vector<Token>& ts) {
  std::vector<TokenKind> ks;
  ks.reserve(ts.size());
  for (auto& t : ts)
    ks.push_back(t.kind);
  return ks;
}

TEST(Lexer, EmptyInputProducesOnlyEof) {
  auto ts = tok("");
  ASSERT_EQ(ts.size(), 1u);
  EXPECT_EQ(ts[0].kind, TokenKind::Eof);
}

TEST(Lexer, SingleCharPunctuation) {
  auto ts = tok("(){},.-+;*/");
  std::vector<TokenKind> exp = {TokenKind::LeftParen,  TokenKind::RightParen, TokenKind::LeftBrace,
                                TokenKind::RightBrace, TokenKind::Comma,      TokenKind::Dot,
                                TokenKind::Minus,      TokenKind::Plus,       TokenKind::Semicolon,
                                TokenKind::Star,       TokenKind::Slash,      TokenKind::Eof};
  EXPECT_EQ(kinds(ts), exp);
}

TEST(Lexer, TwoCharOperators) {
  auto ts = tok("!= == >= <= = ! > <");
  std::vector<TokenKind> exp = {
      TokenKind::BangEqual, TokenKind::EqualEqual, TokenKind::GreaterEqual,
      TokenKind::LessEqual, TokenKind::Equal,      TokenKind::Bang,
      TokenKind::Greater,   TokenKind::Less,       TokenKind::Eof};
  EXPECT_EQ(kinds(ts), exp);
}

TEST(Lexer, IntegerLiteral) {
  auto ts = tok("123");
  ASSERT_EQ(ts.size(), 2u);
  EXPECT_EQ(ts[0].kind, TokenKind::Number);
  EXPECT_EQ(std::get<double>(ts[0].literal), 123.0);
}

TEST(Lexer, FloatLiteral) {
  auto ts = tok("3.14");
  ASSERT_EQ(ts.size(), 2u);
  EXPECT_EQ(ts[0].kind, TokenKind::Number);
  EXPECT_DOUBLE_EQ(std::get<double>(ts[0].literal), 3.14);
}

TEST(Lexer, TrailingDotIsSeparateToken) {
  auto ts = tok("1.");
  ASSERT_EQ(ts.size(), 3u);
  EXPECT_EQ(ts[0].kind, TokenKind::Number);
  EXPECT_EQ(ts[1].kind, TokenKind::Dot);
}

TEST(Lexer, StringLiteralBasic) {
  auto ts = tok("\"hello\"");
  ASSERT_EQ(ts.size(), 2u);
  EXPECT_EQ(ts[0].kind, TokenKind::String);
  EXPECT_EQ(std::get<std::string>(ts[0].literal), "hello");
}

TEST(Lexer, StringLiteralWithSpaces) {
  auto ts = tok("\"hello world\"");
  ASSERT_EQ(ts[0].kind, TokenKind::String);
  EXPECT_EQ(std::get<std::string>(ts[0].literal), "hello world");
}

TEST(Lexer, UnterminatedStringReportsError) {
  Lexer lx("\"oops");
  lx.scan_tokens();
  EXPECT_TRUE(lx.had_error());
}

TEST(Lexer, MultilineStringTracksLine) {
  auto ts = tok("\"line1\nline2\"");
  EXPECT_EQ(ts[0].kind, TokenKind::String);
  EXPECT_EQ(ts[0].line, 2);
}

TEST(Lexer, Identifier) {
  auto ts = tok("foo bar_baz x1");
  EXPECT_EQ(ts[0].kind, TokenKind::Identifier);
  EXPECT_EQ(ts[0].lexeme, "foo");
  EXPECT_EQ(ts[1].kind, TokenKind::Identifier);
  EXPECT_EQ(ts[1].lexeme, "bar_baz");
  EXPECT_EQ(ts[2].kind, TokenKind::Identifier);
  EXPECT_EQ(ts[2].lexeme, "x1");
}

TEST(Lexer, AllKeywords) {
  auto ts = tok("and class else false for fun if nil or print return super this true var while");
  std::vector<TokenKind> exp = {
      TokenKind::And,    TokenKind::Class, TokenKind::Else, TokenKind::False, TokenKind::For,
      TokenKind::Fun,    TokenKind::If,    TokenKind::Nil,  TokenKind::Or,    TokenKind::Print,
      TokenKind::Return, TokenKind::Super, TokenKind::This, TokenKind::True,  TokenKind::Var,
      TokenKind::While,  TokenKind::Eof};
  EXPECT_EQ(kinds(ts), exp);
}

TEST(Lexer, BoolLiteralsCarryValue) {
  auto ts = tok("true false");
  EXPECT_EQ(std::get<bool>(ts[0].literal), true);
  EXPECT_EQ(std::get<bool>(ts[1].literal), false);
}

TEST(Lexer, LineCommentSkipped) {
  auto ts = tok("// nothing\nfoo");
  ASSERT_EQ(ts.size(), 2u);
  EXPECT_EQ(ts[0].kind, TokenKind::Identifier);
  EXPECT_EQ(ts[0].line, 2);
}

TEST(Lexer, WhitespaceSkipped) {
  auto ts = tok("  \t \n  foo  \n");
  EXPECT_EQ(ts[0].kind, TokenKind::Identifier);
  EXPECT_EQ(ts[0].line, 2);
}

TEST(Lexer, UnexpectedCharacterReportsError) {
  Lexer lx("@");
  lx.scan_tokens();
  EXPECT_TRUE(lx.had_error());
}

TEST(Lexer, MixedExpressionTokens) {
  auto ts = tok("var x = 1 + 2 * 3;");
  std::vector<TokenKind> exp = {TokenKind::Var,    TokenKind::Identifier, TokenKind::Equal,
                                TokenKind::Number, TokenKind::Plus,       TokenKind::Number,
                                TokenKind::Star,   TokenKind::Number,     TokenKind::Semicolon,
                                TokenKind::Eof};
  EXPECT_EQ(kinds(ts), exp);
}

TEST(Lexer, LineNumbersTracked) {
  auto ts = tok("a\nb\nc");
  EXPECT_EQ(ts[0].line, 1);
  EXPECT_EQ(ts[1].line, 2);
  EXPECT_EQ(ts[2].line, 3);
}

TEST(Lexer, NoErrorsOnValidProgram) {
  Lexer lx("class C { init() { this.x = 1; } } var c = C();");
  lx.scan_tokens();
  EXPECT_FALSE(lx.had_error());
}

TEST(Lexer, KeywordsVsIdentifierBoundary) {
  auto ts = tok("classy class");
  EXPECT_EQ(ts[0].kind, TokenKind::Identifier);
  EXPECT_EQ(ts[0].lexeme, "classy");
  EXPECT_EQ(ts[1].kind, TokenKind::Class);
}

TEST(Lexer, SlashVsLineComment) {
  auto ts = tok("/ //\n/");
  EXPECT_EQ(ts[0].kind, TokenKind::Slash);
  EXPECT_EQ(ts[1].kind, TokenKind::Slash);
  EXPECT_EQ(ts[2].kind, TokenKind::Eof);
}
