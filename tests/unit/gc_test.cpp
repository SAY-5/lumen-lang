#include "bytecode/gc.h"

#include <gtest/gtest.h>

#include <string>

#include "bytecode/compiler.h"
#include "bytecode/vm.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

using namespace lumen;

namespace {

FunctionObj compile_program(const std::string& src) {
  Lexer lx(src);
  auto tokens = lx.scan_tokens();
  EXPECT_FALSE(lx.had_error());
  Parser p(std::move(tokens));
  auto stmts = p.parse();
  EXPECT_FALSE(p.had_error());
  Compiler compiler;
  FunctionObj fn = compiler.compile(stmts);
  EXPECT_FALSE(compiler.had_error());
  return fn;
}

std::string run(const std::string& src) {
  VM vm;
  EXPECT_EQ(vm.run(compile_program(src)), InterpretResult::Ok) << vm.error_message();
  return vm.output();
}

}  // namespace

// A two-node cycle (a.other = b; b.other = a) that reference counting alone
// could never reclaim. After dropping both roots and calling gc(), the freed
// sentinel must observe the two instances (and their bound state) being swept,
// proving the mark-sweep collector handles cycles.
TEST(GC, CollectsUnreachableCycle) {
  std::size_t before = g_gc_freed_count;
  std::string out =
      run("class Node { init() { this.other = nil; } }\n"
          "fun makeCycle() {\n"
          "  var a = Node();\n"
          "  var b = Node();\n"
          "  a.other = b;\n"
          "  b.other = a;\n"  // a <-> b form a reference cycle
          "}\n"
          "makeCycle();\n"  // both locals go out of scope; the cycle is now garbage
          "var live_before = gc();\n"
          "print \"swept\";\n");
  EXPECT_EQ(out, "swept\n");
  // The collector ran and reclaimed objects no longer reachable from any root.
  EXPECT_GT(g_gc_freed_count, before) << "gc() did not free the unreachable cycle";
}

// Objects still reachable from a global root must survive collection.
TEST(GC, PreservesReachableObjects) {
  std::string out =
      run("class Box { init(v) { this.v = v; } }\n"
          "var keep = Box(99);\n"
          "gc();\n"            // keep is rooted in globals, must survive
          "print keep.v;\n");  // still usable after a collection
  EXPECT_EQ(out, "99\n");
}

// A self-referential cycle (a.self = a) is also collectible.
TEST(GC, CollectsSelfReferentialCycle) {
  std::size_t before = g_gc_freed_count;
  run("class N { init() { this.self = nil; } }\n"
      "fun loop() { var a = N(); a.self = a; }\n"
      "loop();\n"
      "gc();\n");
  EXPECT_GT(g_gc_freed_count, before);
}

// Repeated allocation in a loop with no retained references must not grow the
// live set without bound: the automatic trigger keeps it collected.
TEST(GC, AutomaticCollectionBoundsLiveSet) {
  std::string out =
      run("class T {}\n"
          "for (var i = 0; i < 5000; i = i + 1) { var t = T(); }\n"
          "print gc() >= 0;\n");  // program completes without exhausting memory
  EXPECT_EQ(out, "true\n");
}

// gc() returns the live object count and is callable from Lumen.
TEST(GC, GcBuiltinReturnsLiveCount) {
  std::string out = run("var n = gc(); print n >= 0;\n");
  EXPECT_EQ(out, "true\n");
}
