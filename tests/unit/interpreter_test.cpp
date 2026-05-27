#include "eval/interpreter.h"

#include <gtest/gtest.h>

#include <string>

#include "lexer/lexer.h"
#include "parser/parser.h"

using namespace lumen;

namespace {

// Runs source and returns captured stdout. Asserts no runtime error.
std::string run(const std::string& src) {
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

// Runs source expecting a runtime error; returns the error message.
std::string run_err(const std::string& src) {
  Lexer lx(src);
  auto tokens = lx.scan_tokens();
  Parser p(std::move(tokens));
  auto stmts = p.parse();
  Interpreter interp;
  bool ok = interp.interpret(stmts);
  EXPECT_FALSE(ok);
  return interp.error_message();
}

}  // namespace

TEST(Interp, PrintNumber) {
  EXPECT_EQ(run("print 42;"), "42\n");
}

TEST(Interp, PrintString) {
  EXPECT_EQ(run("print \"hello\";"), "hello\n");
}

TEST(Interp, PrintBooleans) {
  EXPECT_EQ(run("print true; print false;"), "true\nfalse\n");
}

TEST(Interp, PrintNil) {
  EXPECT_EQ(run("print nil;"), "nil\n");
}

TEST(Interp, Arithmetic) {
  EXPECT_EQ(run("print 1 + 2 * 3;"), "7\n");
}

TEST(Interp, Division) {
  EXPECT_EQ(run("print 10 / 4;"), "2.5\n");
}

TEST(Interp, Subtraction) {
  EXPECT_EQ(run("print 5 - 8;"), "-3\n");
}

TEST(Interp, UnaryNegation) {
  EXPECT_EQ(run("print -7;"), "-7\n");
}

TEST(Interp, Grouping) {
  EXPECT_EQ(run("print (1 + 2) * 3;"), "9\n");
}

TEST(Interp, StringConcatenation) {
  EXPECT_EQ(run("print \"foo\" + \"bar\";"), "foobar\n");
}

TEST(Interp, Comparisons) {
  EXPECT_EQ(run("print 1 < 2; print 2 <= 2; print 3 > 4; print 5 >= 5;"),
            "true\ntrue\nfalse\ntrue\n");
}

TEST(Interp, Equality) {
  EXPECT_EQ(run("print 1 == 1; print 1 != 2; print \"a\" == \"a\"; print nil == nil;"),
            "true\ntrue\ntrue\ntrue\n");
}

TEST(Interp, EqualityAcrossTypes) {
  EXPECT_EQ(run("print 1 == \"1\"; print true == 1;"), "false\nfalse\n");
}

TEST(Interp, NotOperator) {
  EXPECT_EQ(run("print !true; print !nil; print !0;"), "false\ntrue\nfalse\n");
}

TEST(Interp, VariableDeclarationAndUse) {
  EXPECT_EQ(run("var a = 10; var b = 20; print a + b;"), "30\n");
}

TEST(Interp, VariableDefaultsToNil) {
  EXPECT_EQ(run("var a; print a;"), "nil\n");
}

TEST(Interp, Assignment) {
  EXPECT_EQ(run("var a = 1; a = 2; print a;"), "2\n");
}

TEST(Interp, AssignmentReturnsValue) {
  EXPECT_EQ(run("var a; print a = 5;"), "5\n");
}

TEST(Interp, BlockScoping) {
  EXPECT_EQ(run("var a = 1; { var a = 2; print a; } print a;"), "2\n1\n");
}

TEST(Interp, OuterAssignmentFromInnerScope) {
  EXPECT_EQ(run("var a = 1; { a = 2; } print a;"), "2\n");
}

TEST(Interp, IfTrueBranch) {
  EXPECT_EQ(run("if (true) print 1; else print 2;"), "1\n");
}

TEST(Interp, IfFalseBranch) {
  EXPECT_EQ(run("if (false) print 1; else print 2;"), "2\n");
}

TEST(Interp, LogicalAndShortCircuit) {
  EXPECT_EQ(run("print false and 1; print true and 2;"), "false\n2\n");
}

TEST(Interp, LogicalOrShortCircuit) {
  EXPECT_EQ(run("print true or 1; print false or 3;"), "true\n3\n");
}

TEST(Interp, WhileLoop) {
  EXPECT_EQ(run("var i = 0; while (i < 3) { print i; i = i + 1; }"), "0\n1\n2\n");
}

TEST(Interp, ForLoop) {
  EXPECT_EQ(run("for (var i = 0; i < 3; i = i + 1) print i;"), "0\n1\n2\n");
}

TEST(Interp, FunctionCall) {
  EXPECT_EQ(run("fun add(a, b) { return a + b; } print add(3, 4);"), "7\n");
}

TEST(Interp, FunctionNoReturnYieldsNil) {
  EXPECT_EQ(run("fun f() {} print f();"), "nil\n");
}

TEST(Interp, FunctionEarlyReturn) {
  EXPECT_EQ(run("fun f(x) { if (x > 0) return 1; return -1; } print f(5); print f(-5);"),
            "1\n-1\n");
}

TEST(Interp, Recursion) {
  EXPECT_EQ(run("fun fib(n) { if (n < 2) return n; return fib(n-1) + fib(n-2); } print fib(10);"),
            "55\n");
}

TEST(Interp, ClosureCapturesEnvironment) {
  EXPECT_EQ(run("fun counter() { var c = 0; fun inc() { c = c + 1; return c; } return inc; } "
                "var n = counter(); print n(); print n(); print n();"),
            "1\n2\n3\n");
}

TEST(Interp, ClosureIndependentInstances) {
  EXPECT_EQ(run("fun make() { var x = 0; fun f() { x = x + 1; return x; } return f; } "
                "var a = make(); var b = make(); print a(); print a(); print b();"),
            "1\n2\n1\n");
}

TEST(Interp, NativeClockReturnsNumber) {
  // clock() should be callable and produce a numeric value (non-negative).
  EXPECT_EQ(run("var t = clock(); print t >= 0;"), "true\n");
}

TEST(Interp, ClassInstantiationAndField) {
  EXPECT_EQ(run("class Box {} var b = Box(); b.value = 42; print b.value;"), "42\n");
}

TEST(Interp, MethodCall) {
  EXPECT_EQ(run("class Greeter { hello() { return \"hi\"; } } print Greeter().hello();"), "hi\n");
}

TEST(Interp, ThisBinding) {
  EXPECT_EQ(run("class Box { setv(v) { this.v = v; } getv() { return this.v; } } "
                "var b = Box(); b.setv(7); print b.getv();"),
            "7\n");
}

TEST(Interp, Initializer) {
  EXPECT_EQ(run("class Point { init(x, y) { this.x = x; this.y = y; } } "
                "var p = Point(3, 4); print p.x; print p.y;"),
            "3\n4\n");
}

TEST(Interp, SingleInheritanceInheritsMethod) {
  EXPECT_EQ(run("class A { greet() { return \"A\"; } } class B < A {} print B().greet();"), "A\n");
}

TEST(Interp, MethodOverride) {
  EXPECT_EQ(run("class A { who() { return \"A\"; } } class B < A { who() { return \"B\"; } } "
                "print B().who();"),
            "B\n");
}

TEST(Interp, SuperCall) {
  EXPECT_EQ(run("class A { greet() { return \"A\"; } } "
                "class B < A { greet() { return \"B+\" + super.greet(); } } print B().greet();"),
            "B+A\n");
}

TEST(Interp, ThreeLevelInheritanceSuperChain) {
  EXPECT_EQ(run("class A { name() { return \"A\"; } } "
                "class B < A { name() { return \"B\" + super.name(); } } "
                "class C < B { name() { return \"C\" + super.name(); } } print C().name();"),
            "CBA\n");
}

TEST(Interp, ErrorUndefinedVariable) {
  EXPECT_NE(run_err("print x;").find("undefined variable"), std::string::npos);
}

TEST(Interp, ErrorTypeMismatchInArithmetic) {
  EXPECT_NE(run_err("print 1 + \"a\";").find("must be"), std::string::npos);
}

TEST(Interp, ErrorCallNonCallable) {
  EXPECT_NE(run_err("var x = 5; x();").find("can only call"), std::string::npos);
}

TEST(Interp, ErrorWrongArgCount) {
  EXPECT_NE(run_err("fun f(a) {} f(1, 2);").find("expected"), std::string::npos);
}

TEST(Interp, ErrorUndefinedProperty) {
  EXPECT_NE(run_err("class A {} A().missing;").find("undefined property"), std::string::npos);
}

TEST(Interp, ErrorSuperclassNotClass) {
  EXPECT_NE(run_err("var x = 1; class B < x {} ").find("must be a class"), std::string::npos);
}
