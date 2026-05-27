#include <gtest/gtest.h>

#include <random>
#include <string>
#include <vector>

#include "lexer/lexer.h"

using namespace lumen;

namespace {

// A token spec the generator can emit: the source text to write and the kind
// the lexer should produce for it.
struct Spec {
  std::string text;
  TokenKind kind;
};

// The fixed-text tokens (punctuation and keywords). Identifiers, numbers, and
// strings are generated separately so their text can vary.
const std::vector<Spec>& fixed_specs() {
  static const std::vector<Spec> specs = {
      {"(", TokenKind::LeftParen},  {")", TokenKind::RightParen},    {"{", TokenKind::LeftBrace},
      {"}", TokenKind::RightBrace}, {",", TokenKind::Comma},         {".", TokenKind::Dot},
      {"-", TokenKind::Minus},      {"+", TokenKind::Plus},          {";", TokenKind::Semicolon},
      {"*", TokenKind::Star},       {"/", TokenKind::Slash},         {"!", TokenKind::Bang},
      {"!=", TokenKind::BangEqual}, {"=", TokenKind::Equal},         {"==", TokenKind::EqualEqual},
      {">", TokenKind::Greater},    {">=", TokenKind::GreaterEqual}, {"<", TokenKind::Less},
      {"<=", TokenKind::LessEqual}, {"and", TokenKind::And},         {"class", TokenKind::Class},
      {"else", TokenKind::Else},    {"false", TokenKind::False},     {"for", TokenKind::For},
      {"fun", TokenKind::Fun},      {"if", TokenKind::If},           {"nil", TokenKind::Nil},
      {"or", TokenKind::Or},        {"print", TokenKind::Print},     {"return", TokenKind::Return},
      {"super", TokenKind::Super},  {"this", TokenKind::This},       {"true", TokenKind::True},
      {"var", TokenKind::Var},      {"while", TokenKind::While},
  };
  return specs;
}

// Identifiers must not collide with keywords, so a fixed prefix is used.
Spec gen_identifier(std::mt19937& rng) {
  static const char alpha[] = "abcdefghijklmnopqrstuvwxyz";
  std::uniform_int_distribution<int> len_dist(1, 8);
  std::uniform_int_distribution<int> chr_dist(0, 25);
  std::string s = "v_";
  int n = len_dist(rng);
  for (int i = 0; i < n; ++i)
    s += alpha[chr_dist(rng)];
  return {s, TokenKind::Identifier};
}

Spec gen_number(std::mt19937& rng) {
  std::uniform_int_distribution<int> int_dist(0, 99999);
  std::uniform_int_distribution<int> frac(0, 1);
  std::string s = std::to_string(int_dist(rng));
  if (frac(rng))
    s += "." + std::to_string(int_dist(rng));
  return {s, TokenKind::Number};
}

// Strings avoid embedded quotes and newlines so the text round-trips simply.
Spec gen_string(std::mt19937& rng) {
  static const char alpha[] = "abcdef ABCDEF0123";
  std::uniform_int_distribution<int> len_dist(0, 10);
  std::uniform_int_distribution<int> chr_dist(0, 16);
  std::string inner;
  int n = len_dist(rng);
  for (int i = 0; i < n; ++i)
    inner += alpha[chr_dist(rng)];
  return {"\"" + inner + "\"", TokenKind::String};
}

Spec gen_token(std::mt19937& rng) {
  std::uniform_int_distribution<int> pick(0, 9);
  int p = pick(rng);
  if (p == 0)
    return gen_identifier(rng);
  if (p == 1)
    return gen_number(rng);
  if (p == 2)
    return gen_string(rng);
  const auto& fixed = fixed_specs();
  std::uniform_int_distribution<std::size_t> fdist(0, fixed.size() - 1);
  return fixed[fdist(rng)];
}

}  // namespace

// Property: a random sequence of valid tokens, joined by spaces, lexes back to
// exactly that kind sequence (plus a trailing Eof) with no lex errors.
TEST(LexerProperty, RandomTokenSequenceRoundTrips) {
  std::mt19937 rng(0xC0FFEE);
  std::uniform_int_distribution<int> count_dist(1, 40);

  for (int trial = 0; trial < 500; ++trial) {
    std::vector<TokenKind> expected;
    std::string source;
    int count = count_dist(rng);
    for (int i = 0; i < count; ++i) {
      Spec spec = gen_token(rng);
      if (!source.empty())
        source += ' ';
      source += spec.text;
      expected.push_back(spec.kind);
    }
    expected.push_back(TokenKind::Eof);

    Lexer lx(source);
    auto tokens = lx.scan_tokens();
    ASSERT_FALSE(lx.had_error()) << "trial " << trial << " source: " << source;

    std::vector<TokenKind> got;
    got.reserve(tokens.size());
    for (const auto& t : tokens)
      got.push_back(t.kind);
    ASSERT_EQ(got, expected) << "trial " << trial << " source: " << source;
  }
}

// Property: number literals round-trip to the same double value the source
// text denotes.
TEST(LexerProperty, NumberLiteralsPreserveValue) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<double> dist(0.0, 1000000.0);
  for (int trial = 0; trial < 500; ++trial) {
    double original = dist(rng);
    std::string text = std::to_string(original);
    Lexer lx(text);
    auto tokens = lx.scan_tokens();
    ASSERT_FALSE(lx.had_error()) << text;
    ASSERT_EQ(tokens.size(), 2u) << text;
    ASSERT_EQ(tokens[0].kind, TokenKind::Number);
    ASSERT_TRUE(std::holds_alternative<double>(tokens[0].literal));
    EXPECT_DOUBLE_EQ(std::get<double>(tokens[0].literal), std::stod(text));
  }
}

// Property: string literal contents survive lexing unchanged.
TEST(LexerProperty, StringContentsPreserved) {
  std::mt19937 rng(7);
  for (int trial = 0; trial < 500; ++trial) {
    Spec spec = gen_string(rng);
    std::string inner = spec.text.substr(1, spec.text.size() - 2);
    Lexer lx(spec.text);
    auto tokens = lx.scan_tokens();
    ASSERT_FALSE(lx.had_error()) << spec.text;
    ASSERT_EQ(tokens[0].kind, TokenKind::String);
    ASSERT_TRUE(std::holds_alternative<std::string>(tokens[0].literal));
    EXPECT_EQ(std::get<std::string>(tokens[0].literal), inner);
  }
}

// Property: arbitrary bytes never crash the lexer and always terminate with an
// Eof token, whether or not errors were reported.
TEST(LexerProperty, ArbitraryBytesNeverCrash) {
  std::mt19937 rng(123);
  std::uniform_int_distribution<int> len_dist(0, 200);
  std::uniform_int_distribution<int> byte_dist(1, 255);
  for (int trial = 0; trial < 1000; ++trial) {
    std::string source;
    int n = len_dist(rng);
    for (int i = 0; i < n; ++i)
      source += static_cast<char>(byte_dist(rng));
    Lexer lx(source);
    auto tokens = lx.scan_tokens();
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens.back().kind, TokenKind::Eof);
  }
}
