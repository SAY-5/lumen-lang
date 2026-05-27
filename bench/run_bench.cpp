// Lumen benchmark harness.
//
// Runs a fixed set of Lumen programs under the selected engine, measuring
// wall-clock time, derived ops/sec, and peak resident memory. Supports three
// modes:
//   (default)             human-readable table to stdout
//   --json                machine-readable JSON to stdout
//   --check FILE --tolerance T
//                         compare current run against a saved JSON baseline and
//                         fail (exit 1) if any benchmark is slower by > T (a
//                         fraction, e.g. 0.30 for 30%).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif

#include "eval/interpreter.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

namespace {

struct Benchmark {
  std::string name;
  std::string source;
  // Nominal operation count used to derive ops/sec. Chosen per program so the
  // metric is stable and comparable across runs.
  double ops;
};

// Builds the fib source for a given n with its exact call count as the op
// metric (number of fib invocations = 2*fib(n+1) - 1).
std::string fib_source(int n) {
  return "fun fib(n) { if (n < 2) return n; return fib(n - 1) + fib(n - 2); }\n"
         "print fib(" +
         std::to_string(n) + ");\n";
}

double fib_calls(int n) {
  double a = 0, b = 1;  // fib(0), fib(1)
  for (int i = 2; i <= n + 1; ++i) {
    double c = a + b;
    a = b;
    b = c;
  }
  // calls(n) = 2*fib(n+1) - 1
  return 2.0 * b - 1.0;
}

struct Result {
  std::string name;
  double seconds = 0.0;
  double ops_per_sec = 0.0;
  long peak_kb = 0;
};

// bubble_sort(n): O(n^2) comparisons/swaps over a pseudo-random list built with
// a linear congruential generator (the language has no arrays, so the list is
// modelled as a chain of instances).
std::string bubble_source(int n) {
  return "class Node { init(v, next) { this.v = v; this.next = next; } }\n"
         "var n = " +
         std::to_string(n) +
         ";\n"
         "var seed = 12345;\n"
         "var head = nil;\n"
         "for (var i = 0; i < n; i = i + 1) {\n"
         "  seed = seed * 1103515245 + 12345;\n"
         "  if (seed < 0) seed = -seed;\n"
         "  var bounded = seed / 1000000 - (seed / 1000000000) * 1000;\n"
         "  head = Node(bounded, head);\n"
         "}\n"
         "var swapped = true;\n"
         "while (swapped) {\n"
         "  swapped = false;\n"
         "  var cur = head;\n"
         "  while (cur != nil and cur.next != nil) {\n"
         "    if (cur.v > cur.next.v) {\n"
         "      var t = cur.v; cur.v = cur.next.v; cur.next.v = t;\n"
         "      swapped = true;\n"
         "    }\n"
         "    cur = cur.next;\n"
         "  }\n"
         "}\n"
         "print head.v;\n";
}

// mandelbrot(w,h,limit): escape-time over a w x h grid, `limit` iterations max.
std::string mandel_source(int w, int h, int limit) {
  return "var rows = " + std::to_string(h) + "; var cols = " + std::to_string(w) +
         "; var limit = " + std::to_string(limit) +
         "; var total = 0;\n"
         "for (var py = 0; py < rows; py = py + 1) {\n"
         "  var y0 = py / rows * 2 - 1;\n"
         "  for (var px = 0; px < cols; px = px + 1) {\n"
         "    var x0 = px / cols * 3 - 2;\n"
         "    var x = 0; var y = 0; var iter = 0;\n"
         "    while (x * x + y * y <= 4 and iter < limit) {\n"
         "      var xt = x * x - y * y + x0; y = 2 * x * y + y0; x = xt; iter = iter + 1;\n"
         "    }\n"
         "    total = total + iter;\n"
         "  }\n"
         "}\n"
         "print total;\n";
}

long peak_rss_kb() {
#if defined(__APPLE__) || defined(__linux__)
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(__APPLE__)
    return usage.ru_maxrss / 1024;  // bytes on macOS
#else
    return usage.ru_maxrss;  // kilobytes on Linux
#endif
  }
#endif
  return 0;
}

Result run_one(const Benchmark& b) {
  using clock = std::chrono::steady_clock;
  lumen::Lexer lexer(b.source);
  auto tokens = lexer.scan_tokens();
  lumen::Parser parser(std::move(tokens));
  auto stmts = parser.parse();
  if (parser.had_error()) {
    std::cerr << "bench: parse error in " << b.name << "\n";
    std::exit(2);
  }

  auto start = clock::now();
  lumen::Interpreter interp;
  bool ok = interp.interpret(stmts);
  auto end = clock::now();
  if (!ok) {
    std::cerr << "bench: runtime error in " << b.name << ": " << interp.error_message() << "\n";
    std::exit(2);
  }

  Result r;
  r.name = b.name;
  r.seconds = std::chrono::duration<double>(end - start).count();
  r.ops_per_sec = r.seconds > 0 ? b.ops / r.seconds : 0.0;
  r.peak_kb = peak_rss_kb();
  return r;
}

// Full-size benchmarks per the v2 spec: fib(30), bubble_sort(1000),
// mandelbrot(40,40,20). The smoke variants run a fraction of the work so CI can
// exercise the harness end-to-end in well under a second.
std::vector<Benchmark> benchmarks(bool smoke) {
  if (smoke) {
    return {
        {"fib30", fib_source(22), fib_calls(22)},
        {"bubble_sort1000", bubble_source(120), 120.0 * 120.0},
        {"mandelbrot40", mandel_source(20, 20, 20), 20.0 * 20.0 * 20.0},
    };
  }
  return {
      {"fib30", fib_source(30), fib_calls(30)},
      {"bubble_sort1000", bubble_source(1000), 1000.0 * 1000.0},
      {"mandelbrot40", mandel_source(40, 40, 20), 40.0 * 40.0 * 20.0},
  };
}

void print_table(const std::vector<Result>& results) {
  std::printf("%-18s %12s %16s %12s\n", "benchmark", "wall (s)", "ops/sec", "peak (KB)");
  for (const auto& r : results) {
    std::printf("%-18s %12.4f %16.0f %12ld\n", r.name.c_str(), r.seconds, r.ops_per_sec, r.peak_kb);
  }
}

void print_json(const std::vector<Result>& results) {
  std::cout << "{\n";
  for (std::size_t i = 0; i < results.size(); ++i) {
    const auto& r = results[i];
    std::cout << "  \"" << r.name << "\": {\"seconds\": " << r.seconds
              << ", \"ops_per_sec\": " << r.ops_per_sec << ", \"peak_kb\": " << r.peak_kb << "}";
    std::cout << (i + 1 < results.size() ? ",\n" : "\n");
  }
  std::cout << "}\n";
}

// Minimal JSON baseline reader: extracts each "name": {... "ops_per_sec": N ...}.
std::map<std::string, double> load_baseline_ops(const std::string& path) {
  std::ifstream in(path);
  std::map<std::string, double> ops;
  if (!in)
    return ops;
  std::stringstream ss;
  ss << in.rdbuf();
  std::string text = ss.str();
  for (const auto& b : benchmarks(false)) {
    auto key = "\"" + b.name + "\"";
    auto pos = text.find(key);
    if (pos == std::string::npos)
      continue;
    auto ops_pos = text.find("\"ops_per_sec\":", pos);
    if (ops_pos == std::string::npos)
      continue;
    ops_pos += std::strlen("\"ops_per_sec\":");
    ops[b.name] = std::strtod(text.c_str() + ops_pos, nullptr);
  }
  return ops;
}

}  // namespace

int main(int argc, char** argv) {
  bool json = false;
  bool check = false;
  bool smoke = false;
  std::string baseline;
  double tolerance = 0.30;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--json") {
      json = true;
    } else if (arg == "--smoke") {
      smoke = true;
    } else if (arg == "--check" && i + 1 < argc) {
      check = true;
      baseline = argv[++i];
    } else if (arg == "--tolerance" && i + 1 < argc) {
      tolerance = std::strtod(argv[++i], nullptr);
    }
  }

  std::vector<Result> results;
  for (const auto& b : benchmarks(smoke)) {
    results.push_back(run_one(b));
  }

  if (check) {
    auto base = load_baseline_ops(baseline);
    if (base.empty()) {
      std::cerr << "bench-regress: no usable baseline at '" << baseline
                << "', skipping (run `make bench` to create one)\n";
      return 0;
    }
    bool regressed = false;
    for (const auto& r : results) {
      auto it = base.find(r.name);
      if (it == base.end())
        continue;
      double baseline_ops = it->second;
      // A drop in ops/sec beyond tolerance is a regression.
      double drift = (baseline_ops - r.ops_per_sec) / baseline_ops;
      std::printf("%-18s baseline=%.0f current=%.0f drift=%+.1f%%\n", r.name.c_str(), baseline_ops,
                  r.ops_per_sec, drift * 100.0);
      if (drift > tolerance) {
        std::cerr << "bench-regress: " << r.name << " regressed " << drift * 100.0
                  << "% (tolerance " << tolerance * 100.0 << "%)\n";
        regressed = true;
      }
    }
    return regressed ? 1 : 0;
  }

  if (json) {
    print_json(results);
  } else {
    print_table(results);
  }
  return 0;
}
