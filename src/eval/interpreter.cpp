#include "eval/interpreter.h"

#include <chrono>
#include <utility>

#include "eval/callable.h"
#include "eval/function.h"

namespace lumen {

namespace {

// Restores the interpreter's current environment when leaving a scope, even if
// an exception (return signal, runtime error) unwinds through it.
struct ScopeGuard {
  std::shared_ptr<Environment>& slot;
  std::shared_ptr<Environment> previous;
  ScopeGuard(std::shared_ptr<Environment>& s, std::shared_ptr<Environment> next)
      : slot(s), previous(s) {
    slot = std::move(next);
  }
  ~ScopeGuard() {
    slot = previous;
  }
};

}  // namespace

Interpreter::Interpreter() {
  globals_ = std::make_shared<Environment>();
  environment_ = globals_;

  // Native: clock() returns seconds since an arbitrary epoch as a double.
  globals_->define("clock", CallablePtr(std::make_shared<NativeFunction>(
                                "clock", 0, [](const std::vector<Value>&) -> Value {
                                  auto now = std::chrono::steady_clock::now().time_since_epoch();
                                  double s = std::chrono::duration<double>(now).count();
                                  return s;
                                })));
}

bool Interpreter::interpret(const std::vector<StmtPtr>& statements) {
  try {
    for (const auto& stmt : statements) {
      execute(stmt);
    }
    return true;
  } catch (const RuntimeError& err) {
    error_message_ = "[line " + std::to_string(err.token().line) + "] runtime error: " + err.what();
    had_error_ = true;
    return false;
  }
}

void Interpreter::execute_block(const std::vector<StmtPtr>& statements,
                                std::shared_ptr<Environment> env) {
  ScopeGuard guard(environment_, std::move(env));
  for (const auto& stmt : statements) {
    execute(stmt);
  }
}

void Interpreter::check_number_operand(const Token& op, const Value& operand) {
  if (!std::holds_alternative<double>(operand)) {
    throw RuntimeError(op, "operand must be a number");
  }
}

void Interpreter::check_number_operands(const Token& op, const Value& left, const Value& right) {
  if (!std::holds_alternative<double>(left) || !std::holds_alternative<double>(right)) {
    throw RuntimeError(op, "operands must be numbers");
  }
}

Value Interpreter::call_value(const Value& callee, const std::vector<Value>& args,
                              const Token& paren) {
  CallablePtr fn;
  if (std::holds_alternative<CallablePtr>(callee)) {
    fn = std::get<CallablePtr>(callee);
  } else if (std::holds_alternative<ClassPtr>(callee)) {
    fn = std::get<ClassPtr>(callee);
  } else {
    throw RuntimeError(paren, "can only call functions and classes");
  }
  if (static_cast<int>(args.size()) != fn->arity()) {
    throw RuntimeError(paren, "expected " + std::to_string(fn->arity()) + " arguments but got " +
                                  std::to_string(args.size()));
  }
  return fn->call(*this, args);
}

void Interpreter::execute(const StmtPtr& stmt) {
  std::visit(
      [&](const auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, ExprStmt>) {
          evaluate(s.expression);
        } else if constexpr (std::is_same_v<T, PrintStmt>) {
          Value v = evaluate(s.expression);
          output_ += stringify(v);
          output_ += '\n';
        } else if constexpr (std::is_same_v<T, VarStmt>) {
          Value v = std::monostate{};
          if (s.initializer)
            v = evaluate(s.initializer);
          environment_->define(s.name.lexeme, std::move(v));
        } else if constexpr (std::is_same_v<T, BlockStmt>) {
          execute_block(s.statements, std::make_shared<Environment>(environment_));
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          if (is_truthy(evaluate(s.condition))) {
            execute(s.then_branch);
          } else if (s.else_branch) {
            execute(s.else_branch);
          }
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
          while (is_truthy(evaluate(s.condition))) {
            execute(s.body);
          }
        } else if constexpr (std::is_same_v<T, FunctionStmt>) {
          auto decl = std::make_shared<FunctionStmt>(s);
          auto fn = std::make_shared<LumenFunction>(decl, environment_, false);
          environment_->define(s.name.lexeme, CallablePtr(fn));
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
          Value v = std::monostate{};
          if (s.value)
            v = evaluate(s.value);
          throw ReturnSignal{std::move(v)};
        } else if constexpr (std::is_same_v<T, ClassStmt>) {
          ClassPtr superklass = nullptr;
          if (s.superclass) {
            Value sv = evaluate(s.superclass);
            if (!std::holds_alternative<ClassPtr>(sv)) {
              const auto& var = std::get<VariableExpr>(s.superclass->node);
              throw RuntimeError(var.name, "superclass must be a class");
            }
            superklass = std::get<ClassPtr>(sv);
          }
          environment_->define(s.name.lexeme, std::monostate{});

          std::shared_ptr<Environment> method_env = environment_;
          if (superklass) {
            method_env = std::make_shared<Environment>(environment_);
            method_env->define("super", superklass);
          }

          auto klass = std::make_shared<ClassValue>();
          klass->name = s.name.lexeme;
          klass->superclass = superklass;
          for (const auto& method : s.methods) {
            bool is_init = method->name.lexeme == "init";
            auto fn = std::make_shared<LumenFunction>(method, method_env, is_init);
            klass->methods[method->name.lexeme] = CallablePtr(fn);
          }
          environment_->assign(s.name, ClassPtr(klass));
        }
      },
      stmt->node);
}

Value Interpreter::evaluate(const ExprPtr& expr) {
  return std::visit(
      [&](const auto& e) -> Value {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, LiteralExpr>) {
          const Literal& lit = e.value;
          if (std::holds_alternative<double>(lit))
            return std::get<double>(lit);
          if (std::holds_alternative<std::string>(lit))
            return std::get<std::string>(lit);
          if (std::holds_alternative<bool>(lit))
            return std::get<bool>(lit);
          return std::monostate{};
        } else if constexpr (std::is_same_v<T, GroupingExpr>) {
          return evaluate(e.expr);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          Value right = evaluate(e.right);
          if (e.op.kind == TokenKind::Minus) {
            check_number_operand(e.op, right);
            return -std::get<double>(right);
          }
          return !is_truthy(right);  // Bang
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          Value left = evaluate(e.left);
          Value right = evaluate(e.right);
          switch (e.op.kind) {
            case TokenKind::Plus:
              if (std::holds_alternative<double>(left) && std::holds_alternative<double>(right))
                return std::get<double>(left) + std::get<double>(right);
              if (std::holds_alternative<std::string>(left) &&
                  std::holds_alternative<std::string>(right))
                return std::get<std::string>(left) + std::get<std::string>(right);
              throw RuntimeError(e.op, "operands must be two numbers or two strings");
            case TokenKind::Minus:
              check_number_operands(e.op, left, right);
              return std::get<double>(left) - std::get<double>(right);
            case TokenKind::Star:
              check_number_operands(e.op, left, right);
              return std::get<double>(left) * std::get<double>(right);
            case TokenKind::Slash:
              check_number_operands(e.op, left, right);
              return std::get<double>(left) / std::get<double>(right);
            case TokenKind::Greater:
              check_number_operands(e.op, left, right);
              return std::get<double>(left) > std::get<double>(right);
            case TokenKind::GreaterEqual:
              check_number_operands(e.op, left, right);
              return std::get<double>(left) >= std::get<double>(right);
            case TokenKind::Less:
              check_number_operands(e.op, left, right);
              return std::get<double>(left) < std::get<double>(right);
            case TokenKind::LessEqual:
              check_number_operands(e.op, left, right);
              return std::get<double>(left) <= std::get<double>(right);
            case TokenKind::EqualEqual:
              return values_equal(left, right);
            case TokenKind::BangEqual:
              return !values_equal(left, right);
            default:
              throw RuntimeError(e.op, "unsupported binary operator");
          }
        } else if constexpr (std::is_same_v<T, LogicalExpr>) {
          Value left = evaluate(e.left);
          if (e.op.kind == TokenKind::Or) {
            if (is_truthy(left))
              return left;
          } else {
            if (!is_truthy(left))
              return left;
          }
          return evaluate(e.right);
        } else if constexpr (std::is_same_v<T, VariableExpr>) {
          return environment_->get(e.name);
        } else if constexpr (std::is_same_v<T, AssignExpr>) {
          Value v = evaluate(e.value);
          environment_->assign(e.name, v);
          return v;
        } else if constexpr (std::is_same_v<T, CallExpr>) {
          Value callee = evaluate(e.callee);
          std::vector<Value> args;
          args.reserve(e.arguments.size());
          for (const auto& a : e.arguments)
            args.push_back(evaluate(a));
          return call_value(callee, args, e.paren);
        } else if constexpr (std::is_same_v<T, GetExpr>) {
          Value object = evaluate(e.object);
          if (std::holds_alternative<InstancePtr>(object)) {
            auto inst = std::get<InstancePtr>(object);
            return inst->get(e.name, inst);
          }
          throw RuntimeError(e.name, "only instances have properties");
        } else if constexpr (std::is_same_v<T, SetExpr>) {
          Value object = evaluate(e.object);
          if (!std::holds_alternative<InstancePtr>(object)) {
            throw RuntimeError(e.name, "only instances have fields");
          }
          Value v = evaluate(e.value);
          std::get<InstancePtr>(object)->set(e.name, v);
          return v;
        } else if constexpr (std::is_same_v<T, ThisExpr>) {
          return environment_->get(e.keyword);
        } else if constexpr (std::is_same_v<T, SuperExpr>) {
          // 'super' and 'this' live in the method's lexical chain. The closure
          // defines super one scope above the bound this, so this is one deeper.
          Value super_val = environment_->get(e.keyword);
          auto superklass = std::get<ClassPtr>(super_val);
          Token this_tok{TokenKind::This, "this", {}, e.keyword.line};
          Value this_val = environment_->get(this_tok);
          auto instance = std::get<InstancePtr>(this_val);
          CallablePtr method = superklass->find_method(e.method.lexeme);
          if (!method) {
            throw RuntimeError(e.method, "undefined property '" + e.method.lexeme + "'");
          }
          return CallablePtr(std::static_pointer_cast<LumenFunction>(method)->bind(instance));
        }
        return std::monostate{};
      },
      expr->node);
}

}  // namespace lumen
