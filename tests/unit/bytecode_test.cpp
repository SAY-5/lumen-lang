#include <gtest/gtest.h>

#include <string>

#include "bytecode/compiler.h"
#include "bytecode/vm.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

using namespace lumen;

namespace {

// Compiles and runs source on the stack VM, returning captured output.
std::string run_bc(const std::string& src) {
  Lexer lx(src);
  auto tokens = lx.scan_tokens();
  EXPECT_FALSE(lx.had_error());
  Parser p(std::move(tokens));
  auto stmts = p.parse();
  EXPECT_FALSE(p.had_error());
  Compiler compiler;
  FunctionObj script = compiler.compile(stmts);
  EXPECT_FALSE(compiler.had_error());
  VM vm;
  InterpretResult r = vm.run(script);
  EXPECT_EQ(r, InterpretResult::Ok) << vm.error_message();
  return vm.output();
}

std::string run_bc_err(const std::string& src) {
  Lexer lx(src);
  auto tokens = lx.scan_tokens();
  Parser p(std::move(tokens));
  auto stmts = p.parse();
  Compiler compiler;
  FunctionObj script = compiler.compile(stmts);
  if (compiler.had_error())
    return "compile error";
  VM vm;
  InterpretResult r = vm.run(script);
  EXPECT_EQ(r, InterpretResult::RuntimeError);
  return vm.error_message();
}

}  // namespace

TEST(Bytecode, Arithmetic) {
  EXPECT_EQ(run_bc("print 1 + 2 * 3;"), "7\n");
}

TEST(Bytecode, Precedence) {
  EXPECT_EQ(run_bc("print (1 + 2) * 3;"), "9\n");
}

TEST(Bytecode, StringConcat) {
  EXPECT_EQ(run_bc("print \"a\" + \"b\";"), "ab\n");
}

TEST(Bytecode, Comparisons) {
  EXPECT_EQ(run_bc("print 1 < 2; print 2 >= 3; print 4 != 4;"), "true\nfalse\nfalse\n");
}

TEST(Bytecode, GlobalsAndLocals) {
  EXPECT_EQ(run_bc("var a = 1; { var b = 2; print a + b; } print a;"), "3\n1\n");
}

TEST(Bytecode, Assignment) {
  EXPECT_EQ(run_bc("var a = 1; a = a + 4; print a;"), "5\n");
}

TEST(Bytecode, IfElse) {
  EXPECT_EQ(run_bc("if (1 < 2) print \"y\"; else print \"n\";"), "y\n");
}

TEST(Bytecode, WhileLoop) {
  EXPECT_EQ(run_bc("var i = 0; while (i < 3) { print i; i = i + 1; }"), "0\n1\n2\n");
}

TEST(Bytecode, ForLoop) {
  EXPECT_EQ(run_bc("for (var i = 0; i < 3; i = i + 1) print i;"), "0\n1\n2\n");
}

TEST(Bytecode, LogicalShortCircuit) {
  EXPECT_EQ(run_bc("print false and 1; print true or 2;"), "false\ntrue\n");
}

TEST(Bytecode, Functions) {
  EXPECT_EQ(run_bc("fun add(a, b) { return a + b; } print add(2, 5);"), "7\n");
}

TEST(Bytecode, Recursion) {
  EXPECT_EQ(run_bc("fun fib(n) { if (n < 2) return n; return fib(n-1)+fib(n-2); } print fib(12);"),
            "144\n");
}

TEST(Bytecode, ClosureCapturesLocal) {
  EXPECT_EQ(run_bc("fun outer() { var x = 0; fun inc() { x = x + 1; return x; } return inc; } "
                   "var c = outer(); print c(); print c(); print c();"),
            "1\n2\n3\n");
}

TEST(Bytecode, IndependentClosures) {
  EXPECT_EQ(run_bc("fun mk() { var n = 0; fun f() { n = n + 1; return n; } return f; } "
                   "var a = mk(); var b = mk(); print a(); print a(); print b();"),
            "1\n2\n1\n");
}

TEST(Bytecode, ClassFieldsAndMethods) {
  EXPECT_EQ(run_bc("class C { init(v) { this.v = v; } get() { return this.v; } } "
                   "var c = C(7); print c.get();"),
            "7\n");
}

TEST(Bytecode, Inheritance) {
  EXPECT_EQ(run_bc("class A { who() { return \"A\"; } } class B < A {} print B().who();"), "A\n");
}

TEST(Bytecode, SuperCall) {
  EXPECT_EQ(run_bc("class A { g() { return \"A\"; } } "
                   "class B < A { g() { return \"B\" + super.g(); } } print B().g();"),
            "BA\n");
}

TEST(Bytecode, ThreeLevelSuper) {
  EXPECT_EQ(run_bc("class A { n() { return \"A\"; } } "
                   "class B < A { n() { return \"B\" + super.n(); } } "
                   "class C < B { n() { return \"C\" + super.n(); } } print C().n();"),
            "CBA\n");
}

TEST(Bytecode, ErrorUndefinedGlobal) {
  EXPECT_NE(run_bc_err("print missing;").find("undefined variable"), std::string::npos);
}

TEST(Bytecode, ErrorTypeMismatch) {
  EXPECT_NE(run_bc_err("print 1 + true;").find("must be"), std::string::npos);
}

TEST(Bytecode, ErrorArity) {
  EXPECT_NE(run_bc_err("fun f(a) {} f();").find("expected 1"), std::string::npos);
}

TEST(Bytecode, ErrorReturnFromTopLevel) {
  EXPECT_EQ(run_bc_err("return 1;"), "compile error");
}
