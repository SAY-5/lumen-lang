#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "eval/interpreter.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#ifndef LUMEN_EXAMPLES_DIR
#define LUMEN_EXAMPLES_DIR "examples"
#endif
#ifndef LUMEN_GOLDEN_DIR
#define LUMEN_GOLDEN_DIR "tests/e2e/golden"
#endif

using namespace lumen;

namespace {

std::string slurp(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  EXPECT_TRUE(in.good()) << "missing file: " << path;
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Executes a Lumen program through the full pipeline and returns stdout.
std::string interpret_program(const std::string& src) {
  Lexer lx(src);
  auto tokens = lx.scan_tokens();
  EXPECT_FALSE(lx.had_error());
  Parser p(std::move(tokens));
  auto stmts = p.parse();
  EXPECT_FALSE(p.had_error());
  Interpreter interp;
  bool ok = interp.interpret(stmts);
  EXPECT_TRUE(ok) << interp.error_message();
  return interp.output();
}

void check_example(const std::string& name) {
  std::string src = slurp(std::string(LUMEN_EXAMPLES_DIR) + "/" + name + ".lum");
  std::string golden = slurp(std::string(LUMEN_GOLDEN_DIR) + "/" + name + ".out");
  EXPECT_EQ(interpret_program(src), golden) << "example " << name << " diverged from golden";
}

}  // namespace

TEST(Golden, Hello) {
  check_example("hello");
}
TEST(Golden, Fib) {
  check_example("fib");
}
TEST(Golden, Counter) {
  check_example("counter");
}
TEST(Golden, Closures) {
  check_example("closures");
}
TEST(Golden, Inheritance) {
  check_example("inheritance");
}
TEST(Golden, Mandelbrot) {
  check_example("mandelbrot");
}
TEST(Golden, GcCycle) {
  check_example("gc_cycle");
}
