#include "parser/parser.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "lexer/lexer.h"

using namespace lumen;

namespace {

std::vector<StmtPtr> parse_ok(const std::string& src) {
  Lexer lx(src);
  auto tokens = lx.scan_tokens();
  EXPECT_FALSE(lx.had_error()) << "unexpected lex error for: " << src;
  Parser p(std::move(tokens));
  auto stmts = p.parse();
  EXPECT_FALSE(p.had_error()) << "unexpected parse error for: " << src;
  return stmts;
}

Parser parse_err(const std::string& src) {
  Lexer lx(src);
  auto tokens = lx.scan_tokens();
  Parser p(std::move(tokens));
  p.parse();
  return p;
}

// Renders an expression into an unambiguous prefix string so the parse tree
// structure can be asserted directly.
std::string sexpr(const ExprPtr& e);

std::string literal_str(const Literal& lit) {
  if (std::holds_alternative<double>(lit)) {
    double d = std::get<double>(lit);
    if (d == static_cast<long long>(d))
      return std::to_string(static_cast<long long>(d));
    return std::to_string(d);
  }
  if (std::holds_alternative<std::string>(lit))
    return "\"" + std::get<std::string>(lit) + "\"";
  if (std::holds_alternative<bool>(lit))
    return std::get<bool>(lit) ? "true" : "false";
  return "nil";
}

std::string sexpr(const ExprPtr& e) {
  return std::visit(
      [](const auto& n) -> std::string {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, LiteralExpr>) {
          return literal_str(n.value);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          return "(" + n.op.lexeme + " " + sexpr(n.right) + ")";
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          return "(" + n.op.lexeme + " " + sexpr(n.left) + " " + sexpr(n.right) + ")";
        } else if constexpr (std::is_same_v<T, LogicalExpr>) {
          return "(" + n.op.lexeme + " " + sexpr(n.left) + " " + sexpr(n.right) + ")";
        } else if constexpr (std::is_same_v<T, GroupingExpr>) {
          return "(group " + sexpr(n.expr) + ")";
        } else if constexpr (std::is_same_v<T, VariableExpr>) {
          return n.name.lexeme;
        } else if constexpr (std::is_same_v<T, AssignExpr>) {
          return "(= " + n.name.lexeme + " " + sexpr(n.value) + ")";
        } else if constexpr (std::is_same_v<T, CallExpr>) {
          std::string s = "(call " + sexpr(n.callee);
          for (const auto& a : n.arguments)
            s += " " + sexpr(a);
          return s + ")";
        } else if constexpr (std::is_same_v<T, GetExpr>) {
          return "(. " + sexpr(n.object) + " " + n.name.lexeme + ")";
        } else if constexpr (std::is_same_v<T, SetExpr>) {
          return "(set " + sexpr(n.object) + " " + n.name.lexeme + " " + sexpr(n.value) + ")";
        } else if constexpr (std::is_same_v<T, ThisExpr>) {
          return "this";
        } else if constexpr (std::is_same_v<T, SuperExpr>) {
          return "(super " + n.method.lexeme + ")";
        }
        return "?";
      },
      e->node);
}

// Extracts the single expression from a program of one expression statement.
std::string single_expr(const std::string& src) {
  auto stmts = parse_ok(src);
  EXPECT_EQ(stmts.size(), 1u);
  const auto& es = std::get<ExprStmt>(stmts[0]->node);
  return sexpr(es.expression);
}

}  // namespace

TEST(Parser, NumberLiteral) {
  EXPECT_EQ(single_expr("42;"), "42");
}

TEST(Parser, StringLiteral) {
  EXPECT_EQ(single_expr("\"hi\";"), "\"hi\"");
}

TEST(Parser, BooleanAndNilLiterals) {
  EXPECT_EQ(single_expr("true;"), "true");
  EXPECT_EQ(single_expr("false;"), "false");
  EXPECT_EQ(single_expr("nil;"), "nil");
}

TEST(Parser, UnaryNegation) {
  EXPECT_EQ(single_expr("-5;"), "(- 5)");
}

TEST(Parser, UnaryNot) {
  EXPECT_EQ(single_expr("!true;"), "(! true)");
}

TEST(Parser, DoubleNegation) {
  EXPECT_EQ(single_expr("--5;"), "(- (- 5))");
}

TEST(Parser, AdditionLeftAssociative) {
  EXPECT_EQ(single_expr("1 + 2 + 3;"), "(+ (+ 1 2) 3)");
}

TEST(Parser, SubtractionLeftAssociative) {
  EXPECT_EQ(single_expr("10 - 2 - 3;"), "(- (- 10 2) 3)");
}

TEST(Parser, MultiplicationBindsTighterThanAddition) {
  EXPECT_EQ(single_expr("1 + 2 * 3;"), "(+ 1 (* 2 3))");
}

TEST(Parser, DivisionAndMultiplicationLeftAssociative) {
  EXPECT_EQ(single_expr("8 / 4 * 2;"), "(* (/ 8 4) 2)");
}

TEST(Parser, UnaryBindsTighterThanFactor) {
  EXPECT_EQ(single_expr("-2 * 3;"), "(* (- 2) 3)");
}

TEST(Parser, GroupingOverridesPrecedence) {
  EXPECT_EQ(single_expr("(1 + 2) * 3;"), "(* (group (+ 1 2)) 3)");
}

TEST(Parser, ComparisonPrecedence) {
  EXPECT_EQ(single_expr("1 + 2 < 3 * 4;"), "(< (+ 1 2) (* 3 4))");
}

TEST(Parser, EqualityLowerThanComparison) {
  EXPECT_EQ(single_expr("1 < 2 == 3 > 4;"), "(== (< 1 2) (> 3 4))");
}

TEST(Parser, NotEqual) {
  EXPECT_EQ(single_expr("1 != 2;"), "(!= 1 2)");
}

TEST(Parser, GreaterEqualLessEqual) {
  EXPECT_EQ(single_expr("1 >= 2;"), "(>= 1 2)");
  EXPECT_EQ(single_expr("1 <= 2;"), "(<= 1 2)");
}

TEST(Parser, LogicalAndOrPrecedence) {
  EXPECT_EQ(single_expr("a or b and c;"), "(or a (and b c))");
}

TEST(Parser, LogicalLowerThanEquality) {
  EXPECT_EQ(single_expr("a == b and c;"), "(and (== a b) c)");
}

TEST(Parser, Assignment) {
  EXPECT_EQ(single_expr("x = 5;"), "(= x 5)");
}

TEST(Parser, AssignmentRightAssociative) {
  EXPECT_EQ(single_expr("a = b = 3;"), "(= a (= b 3))");
}

TEST(Parser, VariableReference) {
  EXPECT_EQ(single_expr("foo;"), "foo");
}

TEST(Parser, CallNoArgs) {
  EXPECT_EQ(single_expr("f();"), "(call f)");
}

TEST(Parser, CallWithArgs) {
  EXPECT_EQ(single_expr("f(1, 2, 3);"), "(call f 1 2 3)");
}

TEST(Parser, CallChaining) {
  EXPECT_EQ(single_expr("f()();"), "(call (call f))");
}

TEST(Parser, PropertyAccess) {
  EXPECT_EQ(single_expr("a.b.c;"), "(. (. a b) c)");
}

TEST(Parser, PropertySet) {
  EXPECT_EQ(single_expr("a.b = 1;"), "(set a b 1)");
}

TEST(Parser, MethodCall) {
  EXPECT_EQ(single_expr("obj.method(1);"), "(call (. obj method) 1)");
}

TEST(Parser, ThisExpression) {
  EXPECT_EQ(single_expr("this;"), "this");
}

TEST(Parser, SuperMethod) {
  EXPECT_EQ(single_expr("super.init;"), "(super init)");
}

TEST(Parser, VarDeclarationWithInitializer) {
  auto stmts = parse_ok("var x = 10;");
  ASSERT_EQ(stmts.size(), 1u);
  const auto& v = std::get<VarStmt>(stmts[0]->node);
  EXPECT_EQ(v.name.lexeme, "x");
  ASSERT_NE(v.initializer, nullptr);
  EXPECT_EQ(sexpr(v.initializer), "10");
}

TEST(Parser, VarDeclarationWithoutInitializer) {
  auto stmts = parse_ok("var x;");
  ASSERT_EQ(stmts.size(), 1u);
  const auto& v = std::get<VarStmt>(stmts[0]->node);
  EXPECT_EQ(v.initializer, nullptr);
}

TEST(Parser, PrintStatement) {
  auto stmts = parse_ok("print 1 + 2;");
  ASSERT_EQ(stmts.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<PrintStmt>(stmts[0]->node));
}

TEST(Parser, BlockStatement) {
  auto stmts = parse_ok("{ var a = 1; print a; }");
  ASSERT_EQ(stmts.size(), 1u);
  const auto& b = std::get<BlockStmt>(stmts[0]->node);
  EXPECT_EQ(b.statements.size(), 2u);
}

TEST(Parser, IfStatement) {
  auto stmts = parse_ok("if (x) print 1;");
  ASSERT_EQ(stmts.size(), 1u);
  const auto& i = std::get<IfStmt>(stmts[0]->node);
  EXPECT_EQ(i.else_branch, nullptr);
}

TEST(Parser, IfElseStatement) {
  auto stmts = parse_ok("if (x) print 1; else print 2;");
  ASSERT_EQ(stmts.size(), 1u);
  const auto& i = std::get<IfStmt>(stmts[0]->node);
  EXPECT_NE(i.else_branch, nullptr);
}

TEST(Parser, WhileStatement) {
  auto stmts = parse_ok("while (x < 10) x = x + 1;");
  ASSERT_EQ(stmts.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<WhileStmt>(stmts[0]->node));
}

TEST(Parser, ForDesugarsToBlockWithWhile) {
  // for is desugared: { init; while (cond) { body; incr; } }
  auto stmts = parse_ok("for (var i = 0; i < 3; i = i + 1) print i;");
  ASSERT_EQ(stmts.size(), 1u);
  const auto& outer = std::get<BlockStmt>(stmts[0]->node);
  ASSERT_EQ(outer.statements.size(), 2u);
  EXPECT_TRUE(std::holds_alternative<VarStmt>(outer.statements[0]->node));
  EXPECT_TRUE(std::holds_alternative<WhileStmt>(outer.statements[1]->node));
}

TEST(Parser, ForInfiniteLoopHasTrueCondition) {
  auto stmts = parse_ok("for (;;) print 1;");
  ASSERT_EQ(stmts.size(), 1u);
  const auto& w = std::get<WhileStmt>(stmts[0]->node);
  EXPECT_EQ(sexpr(w.condition), "true");
}

TEST(Parser, FunctionDeclaration) {
  auto stmts = parse_ok("fun add(a, b) { return a + b; }");
  ASSERT_EQ(stmts.size(), 1u);
  const auto& f = std::get<FunctionStmt>(stmts[0]->node);
  EXPECT_EQ(f.name.lexeme, "add");
  EXPECT_EQ(f.params.size(), 2u);
  EXPECT_EQ(f.body.size(), 1u);
}

TEST(Parser, FunctionNoParams) {
  auto stmts = parse_ok("fun greet() { print \"hi\"; }");
  const auto& f = std::get<FunctionStmt>(stmts[0]->node);
  EXPECT_EQ(f.params.size(), 0u);
}

TEST(Parser, ReturnWithoutValue) {
  auto stmts = parse_ok("fun f() { return; }");
  const auto& f = std::get<FunctionStmt>(stmts[0]->node);
  const auto& r = std::get<ReturnStmt>(f.body[0]->node);
  EXPECT_EQ(r.value, nullptr);
}

TEST(Parser, ClassDeclaration) {
  auto stmts = parse_ok("class Foo { bar() { return 1; } baz() { return 2; } }");
  ASSERT_EQ(stmts.size(), 1u);
  const auto& c = std::get<ClassStmt>(stmts[0]->node);
  EXPECT_EQ(c.name.lexeme, "Foo");
  EXPECT_EQ(c.superclass, nullptr);
  EXPECT_EQ(c.methods.size(), 2u);
}

TEST(Parser, ClassWithSuperclass) {
  auto stmts = parse_ok("class Dog < Animal { speak() { return 1; } }");
  const auto& c = std::get<ClassStmt>(stmts[0]->node);
  ASSERT_NE(c.superclass, nullptr);
  EXPECT_EQ(sexpr(c.superclass), "Animal");
}

TEST(Parser, EmptyClass) {
  auto stmts = parse_ok("class Empty {}");
  const auto& c = std::get<ClassStmt>(stmts[0]->node);
  EXPECT_EQ(c.methods.size(), 0u);
}

TEST(Parser, MultipleStatements) {
  auto stmts = parse_ok("var a = 1; var b = 2; print a + b;");
  EXPECT_EQ(stmts.size(), 3u);
}

TEST(Parser, ErrorMissingSemicolon) {
  Parser p = parse_err("var x = 5");
  EXPECT_TRUE(p.had_error());
}

TEST(Parser, ErrorUnclosedParen) {
  Parser p = parse_err("print (1 + 2;");
  EXPECT_TRUE(p.had_error());
}

TEST(Parser, ErrorInvalidAssignmentTarget) {
  Parser p = parse_err("1 + 2 = 3;");
  EXPECT_TRUE(p.had_error());
}

TEST(Parser, ErrorExpectedExpression) {
  Parser p = parse_err("print ;");
  EXPECT_TRUE(p.had_error());
}

TEST(Parser, ErrorRecoverySynchronizes) {
  // First statement is broken; parser should recover and report the error.
  Parser p = parse_err("var = ; print 1;");
  EXPECT_TRUE(p.had_error());
}

TEST(Parser, ErrorMissingClassBrace) {
  Parser p = parse_err("class Foo bar() {}");
  EXPECT_TRUE(p.had_error());
}
