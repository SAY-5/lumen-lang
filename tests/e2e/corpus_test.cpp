#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "repl/driver.h"

#ifndef LUMEN_CORPUS_DIR
#define LUMEN_CORPUS_DIR "tests/e2e/corpus"
#endif

namespace fs = std::filesystem;
using lumen::Engine;

namespace {

std::string slurp(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Runs a program through the driver, capturing both stdout and stderr in
// process. Returns the driver's exit code; out/err receive the captured text.
int run_capturing(const std::string& source, Engine engine, std::string& out, std::string& err) {
  std::ostringstream out_buf;
  std::ostringstream err_buf;
  std::streambuf* old_out = std::cout.rdbuf(out_buf.rdbuf());
  std::streambuf* old_err = std::cerr.rdbuf(err_buf.rdbuf());
  int code = lumen::run_source(source, engine);
  std::cout.rdbuf(old_out);
  std::cerr.rdbuf(old_err);
  out = out_buf.str();
  err = err_buf.str();
  return code;
}

// Collects corpus base names with the given prefix.
std::vector<std::string> cases_with_prefix(const std::string& prefix) {
  std::vector<std::string> names;
  for (const auto& entry : fs::directory_iterator(LUMEN_CORPUS_DIR)) {
    if (entry.path().extension() != ".lum")
      continue;
    std::string stem = entry.path().stem().string();
    if (stem.rfind(prefix, 0) == 0)
      names.push_back(stem);
  }
  std::sort(names.begin(), names.end());
  return names;
}

}  // namespace

// Every ok_* program must run cleanly (exit 0, empty stderr) and match its
// stdout golden, under both engines.
TEST(Corpus, OkProgramsMatchStdoutGolden) {
  auto names = cases_with_prefix("ok_");
  ASSERT_GE(names.size(), 15u) << "expected a substantial ok corpus";
  for (const auto& name : names) {
    fs::path dir = LUMEN_CORPUS_DIR;
    std::string src = slurp(dir / (name + ".lum"));
    std::string golden = slurp(dir / (name + ".out"));
    for (Engine engine : {Engine::Tree, Engine::Bytecode}) {
      std::string out, err;
      int code = run_capturing(src, engine, out, err);
      EXPECT_EQ(code, 0) << name << " stderr: " << err;
      EXPECT_TRUE(err.empty()) << name << " unexpected stderr: " << err;
      EXPECT_EQ(out, golden) << name << " stdout diverged from golden";
    }
  }
}

// Every err_* program must fail with a non-zero exit and emit the exact
// diagnostic recorded in its .err golden.
TEST(Corpus, ErrorProgramsMatchStderrGolden) {
  auto names = cases_with_prefix("err_");
  ASSERT_GE(names.size(), 10u) << "expected coverage of every error path";
  for (const auto& name : names) {
    fs::path dir = LUMEN_CORPUS_DIR;
    std::string src = slurp(dir / (name + ".lum"));
    std::string golden_err = slurp(dir / (name + ".err"));
    for (Engine engine : {Engine::Tree, Engine::Bytecode}) {
      std::string out, err;
      int code = run_capturing(src, engine, out, err);
      EXPECT_NE(code, 0) << name << " should have failed";
      EXPECT_EQ(err, golden_err) << name << " stderr diverged from golden";
    }
  }
}
