#include "bytecode/compiler.h"

#include <utility>

#include "bytecode/opcode.h"

namespace lumen {

namespace {
constexpr int kUninitialized = -1;
}

Compiler::Compiler() = default;

void Compiler::error(int line, const std::string& message) {
  errors_.push_back({line, message});
}

void Compiler::emit(std::uint8_t byte, int line) {
  current_chunk().write(byte, line);
}

void Compiler::emit(OpCode op, int line) {
  emit(static_cast<std::uint8_t>(op), line);
}

void Compiler::emit_two(OpCode op, std::uint8_t operand, int line) {
  emit(op, line);
  emit(operand, line);
}

std::uint8_t Compiler::make_constant(Constant value) {
  int index = current_chunk().add_constant(std::move(value));
  if (index > 255) {
    error(0, "too many constants in one chunk");
    return 0;
  }
  return static_cast<std::uint8_t>(index);
}

void Compiler::emit_constant(Constant value, int line) {
  emit_two(OpCode::Constant, make_constant(std::move(value)), line);
}

int Compiler::emit_jump(OpCode op, int line) {
  emit(op, line);
  emit(0xff, line);
  emit(0xff, line);
  return static_cast<int>(current_chunk().code.size()) - 2;
}

void Compiler::patch_jump(int offset) {
  int jump = static_cast<int>(current_chunk().code.size()) - offset - 2;
  if (jump > 0xffff)
    error(0, "jump offset too large");
  current_chunk().code[offset] = (jump >> 8) & 0xff;
  current_chunk().code[offset + 1] = jump & 0xff;
}

void Compiler::emit_loop(int loop_start, int line) {
  emit(OpCode::Loop, line);
  int offset = static_cast<int>(current_chunk().code.size()) - loop_start + 2;
  if (offset > 0xffff)
    error(line, "loop body too large");
  emit((offset >> 8) & 0xff, line);
  emit(offset & 0xff, line);
}

void Compiler::emit_return(int line) {
  if (current_->kind == FunctionKind::Initializer) {
    // Initializers implicitly return the instance held in slot 0 ('this').
    emit_two(OpCode::GetLocal, 0, line);
  } else {
    emit(OpCode::Nil, line);
  }
  emit(OpCode::Return, line);
}

void Compiler::begin_scope() {
  current_->scope_depth++;
}

void Compiler::end_scope() {
  current_->scope_depth--;
  auto& locals = current_->locals;
  while (!locals.empty() && locals.back().depth > current_->scope_depth) {
    if (locals.back().is_captured) {
      emit(OpCode::CloseUpvalue, 0);
    } else {
      emit(OpCode::Pop, 0);
    }
    locals.pop_back();
  }
}

void Compiler::add_local(const Token& name) {
  if (current_->locals.size() >= 256) {
    error(name.line, "too many local variables in function");
    return;
  }
  current_->locals.push_back({name.lexeme, kUninitialized, false});
}

void Compiler::mark_initialized() {
  if (current_->scope_depth == 0)
    return;
  current_->locals.back().depth = current_->scope_depth;
}

int Compiler::resolve_local(FunctionState* state, const Token& name) {
  for (int i = static_cast<int>(state->locals.size()) - 1; i >= 0; --i) {
    if (state->locals[i].name == name.lexeme) {
      if (state->locals[i].depth == kUninitialized) {
        error(name.line, "cannot read local variable in its own initializer");
      }
      return i;
    }
  }
  return -1;
}

int Compiler::add_upvalue(FunctionState* state, std::uint8_t index, bool is_local) {
  for (std::size_t i = 0; i < state->upvalues.size(); ++i) {
    if (state->upvalues[i].index == index && state->upvalues[i].is_local == is_local) {
      return static_cast<int>(i);
    }
  }
  if (state->upvalues.size() >= 256) {
    error(0, "too many closure variables in function");
    return 0;
  }
  state->upvalues.push_back({index, is_local});
  state->function->upvalue_count = static_cast<int>(state->upvalues.size());
  return static_cast<int>(state->upvalues.size()) - 1;
}

int Compiler::resolve_upvalue(FunctionState* state, const Token& name) {
  if (state->enclosing == nullptr)
    return -1;
  int local = resolve_local(state->enclosing, name);
  if (local != -1) {
    state->enclosing->locals[local].is_captured = true;
    return add_upvalue(state, static_cast<std::uint8_t>(local), true);
  }
  int upvalue = resolve_upvalue(state->enclosing, name);
  if (upvalue != -1) {
    return add_upvalue(state, static_cast<std::uint8_t>(upvalue), false);
  }
  return -1;
}

void Compiler::declare_variable(const Token& name) {
  if (current_->scope_depth == 0)
    return;
  for (int i = static_cast<int>(current_->locals.size()) - 1; i >= 0; --i) {
    const Local& local = current_->locals[i];
    if (local.depth != kUninitialized && local.depth < current_->scope_depth)
      break;
    if (local.name == name.lexeme) {
      error(name.line, "variable '" + name.lexeme + "' already declared in this scope");
    }
  }
  add_local(name);
}

std::uint8_t Compiler::parse_variable(const Token& name) {
  declare_variable(name);
  if (current_->scope_depth > 0)
    return 0;  // local: no global constant needed
  return make_constant(name.lexeme);
}

void Compiler::define_variable(std::uint8_t global) {
  if (current_->scope_depth > 0) {
    mark_initialized();
    return;
  }
  emit_two(OpCode::DefineGlobal, global, 0);
}

FunctionObj Compiler::compile(const std::vector<StmtPtr>& statements) {
  auto script = std::make_shared<ObjFunction>();
  script->name = "<script>";
  FunctionState state;
  state.function = script;
  state.kind = FunctionKind::Script;
  state.enclosing = nullptr;
  // Slot 0 is reserved for the function being called (the script itself).
  state.locals.push_back({"", 0, false});
  current_ = &state;

  for (const auto& stmt : statements) {
    compile_stmt(stmt);
  }
  emit_return(0);

  current_ = nullptr;
  if (had_error())
    return nullptr;
  return script;
}

void Compiler::compile_function(const std::shared_ptr<FunctionStmt>& decl, FunctionKind kind) {
  auto fn = std::make_shared<ObjFunction>();
  fn->name = decl->name.lexeme;
  fn->arity = static_cast<int>(decl->params.size());

  FunctionState state;
  state.function = fn;
  state.kind = kind;
  state.enclosing = current_;
  // Slot 0 holds the callee; for methods it is 'this'.
  std::string slot0 = (kind == FunctionKind::Method || kind == FunctionKind::Initializer)
                          ? std::string("this")
                          : std::string("");
  state.locals.push_back({slot0, 0, false});
  current_ = &state;

  begin_scope();
  for (const auto& param : decl->params) {
    declare_variable(param);
    mark_initialized();
  }
  for (const auto& stmt : decl->body) {
    compile_stmt(stmt);
  }
  emit_return(decl->name.line);
  // No matching end_scope: the function's locals vanish with its frame.

  FunctionState* finished = current_;
  current_ = state.enclosing;

  emit_two(OpCode::Closure, make_constant(fn), decl->name.line);
  for (const auto& uv : finished->upvalues) {
    emit(uv.is_local ? 1 : 0, decl->name.line);
    emit(uv.index, decl->name.line);
  }
}

void Compiler::compile_class(const ClassStmt& stmt) {
  std::uint8_t name_const = make_constant(stmt.name.lexeme);
  declare_variable(stmt.name);
  emit_two(OpCode::Class, name_const, stmt.name.line);
  define_variable(name_const);

  ClassState class_state{current_class_, false};
  current_class_ = &class_state;

  if (stmt.superclass) {
    const auto& super_var = std::get<VariableExpr>(stmt.superclass->node);
    named_variable(super_var.name, false, nullptr);  // push superclass

    // A hidden scope holds 'super' so methods can capture it as an upvalue.
    begin_scope();
    Token super_tok{TokenKind::Super, "super", {}, stmt.name.line};
    add_local(super_tok);
    mark_initialized();

    named_variable(stmt.name, false, nullptr);  // push subclass
    emit(OpCode::Inherit, stmt.name.line);
    class_state.has_superclass = true;
  }

  // Load the class so methods can be bound onto it.
  named_variable(stmt.name, false, nullptr);
  for (const auto& method : stmt.methods) {
    FunctionKind kind =
        method->name.lexeme == "init" ? FunctionKind::Initializer : FunctionKind::Method;
    compile_function(method, kind);
    emit_two(OpCode::Method, make_constant(method->name.lexeme), method->name.line);
  }
  emit(OpCode::Pop, stmt.name.line);  // pop the class

  if (class_state.has_superclass) {
    end_scope();
  }
  current_class_ = class_state.enclosing;
}

void Compiler::named_variable(const Token& name, bool can_assign, const ExprPtr& value) {
  OpCode get_op, set_op;
  int arg = resolve_local(current_, name);
  if (arg != -1) {
    get_op = OpCode::GetLocal;
    set_op = OpCode::SetLocal;
  } else if ((arg = resolve_upvalue(current_, name)) != -1) {
    get_op = OpCode::GetUpvalue;
    set_op = OpCode::SetUpvalue;
  } else {
    arg = make_constant(name.lexeme);
    get_op = OpCode::GetGlobal;
    set_op = OpCode::SetGlobal;
  }
  if (can_assign && value) {
    compile_expr(value);
    emit_two(set_op, static_cast<std::uint8_t>(arg), name.line);
  } else {
    emit_two(get_op, static_cast<std::uint8_t>(arg), name.line);
  }
}

void Compiler::compile_stmt(const StmtPtr& stmt) {
  std::visit(
      [&](const auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, ExprStmt>) {
          compile_expr(s.expression);
          emit(OpCode::Pop, 0);
        } else if constexpr (std::is_same_v<T, PrintStmt>) {
          compile_expr(s.expression);
          emit(OpCode::Print, 0);
        } else if constexpr (std::is_same_v<T, VarStmt>) {
          std::uint8_t global = parse_variable(s.name);
          if (s.initializer) {
            compile_expr(s.initializer);
          } else {
            emit(OpCode::Nil, s.name.line);
          }
          define_variable(global);
        } else if constexpr (std::is_same_v<T, BlockStmt>) {
          begin_scope();
          for (const auto& inner : s.statements)
            compile_stmt(inner);
          end_scope();
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          compile_expr(s.condition);
          int then_jump = emit_jump(OpCode::JumpIfFalse, 0);
          emit(OpCode::Pop, 0);
          compile_stmt(s.then_branch);
          int else_jump = emit_jump(OpCode::Jump, 0);
          patch_jump(then_jump);
          emit(OpCode::Pop, 0);
          if (s.else_branch)
            compile_stmt(s.else_branch);
          patch_jump(else_jump);
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
          int loop_start = static_cast<int>(current_chunk().code.size());
          compile_expr(s.condition);
          int exit_jump = emit_jump(OpCode::JumpIfFalse, 0);
          emit(OpCode::Pop, 0);
          compile_stmt(s.body);
          emit_loop(loop_start, 0);
          patch_jump(exit_jump);
          emit(OpCode::Pop, 0);
        } else if constexpr (std::is_same_v<T, FunctionStmt>) {
          auto decl = std::make_shared<FunctionStmt>(s);
          std::uint8_t global = parse_variable(s.name);
          mark_initialized();
          compile_function(decl, FunctionKind::Function);
          define_variable(global);
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
          if (current_->kind == FunctionKind::Script) {
            error(s.keyword.line, "cannot return from top-level code");
          }
          if (s.value) {
            if (current_->kind == FunctionKind::Initializer) {
              error(s.keyword.line, "cannot return a value from an initializer");
            }
            compile_expr(s.value);
            emit(OpCode::Return, s.keyword.line);
          } else {
            emit_return(s.keyword.line);
          }
        } else if constexpr (std::is_same_v<T, ClassStmt>) {
          compile_class(s);
        }
      },
      stmt->node);
}

void Compiler::compile_expr(const ExprPtr& expr) {
  std::visit(
      [&](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, LiteralExpr>) {
          const Literal& lit = e.value;
          if (std::holds_alternative<double>(lit)) {
            emit_constant(std::get<double>(lit), e.line);
          } else if (std::holds_alternative<std::string>(lit)) {
            emit_constant(std::get<std::string>(lit), e.line);
          } else if (std::holds_alternative<bool>(lit)) {
            emit(std::get<bool>(lit) ? OpCode::True : OpCode::False, e.line);
          } else {
            emit(OpCode::Nil, e.line);
          }
        } else if constexpr (std::is_same_v<T, GroupingExpr>) {
          compile_expr(e.expr);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          compile_expr(e.right);
          emit(e.op.kind == TokenKind::Minus ? OpCode::Negate : OpCode::Not, e.op.line);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          compile_expr(e.left);
          compile_expr(e.right);
          switch (e.op.kind) {
            case TokenKind::Plus:
              emit(OpCode::Add, e.op.line);
              break;
            case TokenKind::Minus:
              emit(OpCode::Subtract, e.op.line);
              break;
            case TokenKind::Star:
              emit(OpCode::Multiply, e.op.line);
              break;
            case TokenKind::Slash:
              emit(OpCode::Divide, e.op.line);
              break;
            case TokenKind::EqualEqual:
              emit(OpCode::Equal, e.op.line);
              break;
            case TokenKind::BangEqual:
              emit(OpCode::Equal, e.op.line);
              emit(OpCode::Not, e.op.line);
              break;
            case TokenKind::Greater:
              emit(OpCode::Greater, e.op.line);
              break;
            case TokenKind::GreaterEqual:
              emit(OpCode::Less, e.op.line);
              emit(OpCode::Not, e.op.line);
              break;
            case TokenKind::Less:
              emit(OpCode::Less, e.op.line);
              break;
            case TokenKind::LessEqual:
              emit(OpCode::Greater, e.op.line);
              emit(OpCode::Not, e.op.line);
              break;
            default:
              error(e.op.line, "unsupported binary operator");
              break;
          }
        } else if constexpr (std::is_same_v<T, LogicalExpr>) {
          compile_expr(e.left);
          if (e.op.kind == TokenKind::And) {
            int end_jump = emit_jump(OpCode::JumpIfFalse, e.op.line);
            emit(OpCode::Pop, e.op.line);
            compile_expr(e.right);
            patch_jump(end_jump);
          } else {
            int else_jump = emit_jump(OpCode::JumpIfFalse, e.op.line);
            int end_jump = emit_jump(OpCode::Jump, e.op.line);
            patch_jump(else_jump);
            emit(OpCode::Pop, e.op.line);
            compile_expr(e.right);
            patch_jump(end_jump);
          }
        } else if constexpr (std::is_same_v<T, VariableExpr>) {
          named_variable(e.name, false, nullptr);
        } else if constexpr (std::is_same_v<T, AssignExpr>) {
          named_variable(e.name, true, e.value);
        } else if constexpr (std::is_same_v<T, CallExpr>) {
          if (auto* get = std::get_if<GetExpr>(&e.callee->node)) {
            // Method invocation fast path: compile receiver, then Invoke.
            compile_expr(get->object);
            for (const auto& arg : e.arguments)
              compile_expr(arg);
            emit_two(OpCode::Invoke, make_constant(get->name.lexeme), e.paren.line);
            emit(static_cast<std::uint8_t>(e.arguments.size()), e.paren.line);
          } else if (auto* sup = std::get_if<SuperExpr>(&e.callee->node)) {
            // super.method(args): load this + super, then SuperInvoke.
            Token this_tok{TokenKind::This, "this", {}, sup->keyword.line};
            named_variable(this_tok, false, nullptr);
            for (const auto& arg : e.arguments)
              compile_expr(arg);
            named_variable(sup->keyword, false, nullptr);
            emit_two(OpCode::SuperInvoke, make_constant(sup->method.lexeme), e.paren.line);
            emit(static_cast<std::uint8_t>(e.arguments.size()), e.paren.line);
          } else {
            compile_expr(e.callee);
            for (const auto& arg : e.arguments)
              compile_expr(arg);
            emit_two(OpCode::Call, static_cast<std::uint8_t>(e.arguments.size()), e.paren.line);
          }
        } else if constexpr (std::is_same_v<T, GetExpr>) {
          compile_expr(e.object);
          emit_two(OpCode::GetProperty, make_constant(e.name.lexeme), e.name.line);
        } else if constexpr (std::is_same_v<T, SetExpr>) {
          compile_expr(e.object);
          compile_expr(e.value);
          emit_two(OpCode::SetProperty, make_constant(e.name.lexeme), e.name.line);
        } else if constexpr (std::is_same_v<T, ThisExpr>) {
          if (current_class_ == nullptr) {
            error(e.keyword.line, "cannot use 'this' outside of a class");
            return;
          }
          named_variable(e.keyword, false, nullptr);
        } else if constexpr (std::is_same_v<T, SuperExpr>) {
          if (current_class_ == nullptr) {
            error(e.keyword.line, "cannot use 'super' outside of a class");
            return;
          }
          if (!current_class_->has_superclass) {
            error(e.keyword.line, "cannot use 'super' in a class with no superclass");
            return;
          }
          Token this_tok{TokenKind::This, "this", {}, e.keyword.line};
          named_variable(this_tok, false, nullptr);
          named_variable(e.keyword, false, nullptr);
          emit_two(OpCode::GetSuper, make_constant(e.method.lexeme), e.keyword.line);
        }
      },
      expr->node);
}

}  // namespace lumen
