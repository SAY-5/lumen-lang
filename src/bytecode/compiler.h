#pragma once

#include <memory>
#include <string>
#include <vector>

#include "bytecode/chunk.h"
#include "bytecode/opcode.h"
#include "bytecode/vm_value.h"
#include "parser/ast.h"

namespace lumen {

struct CompileError {
  int line;
  std::string message;
};

// Compiles a parsed program (the AST) into a top-level function whose chunk the
// VM executes. Resolves locals to stack slots and free variables to upvalues in
// a single recursive pass over the tree.
class Compiler {
 public:
  Compiler();

  // Returns the top-level function on success, or nullptr if a compile error
  // was recorded.
  FunctionObj compile(const std::vector<StmtPtr>& statements);

  const std::vector<CompileError>& errors() const {
    return errors_;
  }
  bool had_error() const {
    return !errors_.empty();
  }

 private:
  // A local variable in the function currently being compiled.
  struct Local {
    std::string name;
    int depth;         // scope depth where declared; -1 while uninitialized
    bool is_captured;  // whether an inner closure captured this local
  };

  struct UpvalueDesc {
    std::uint8_t index;  // slot in the enclosing function (or its upvalues)
    bool is_local;       // true: enclosing local; false: enclosing upvalue
  };

  enum class FunctionKind { Script, Function, Method, Initializer };

  // Per-function compilation state, linked to the enclosing function so
  // upvalue resolution can walk outward.
  struct FunctionState {
    FunctionObj function;
    FunctionKind kind;
    FunctionState* enclosing;
    std::vector<Local> locals;
    std::vector<UpvalueDesc> upvalues;
    int scope_depth = 0;
  };

  // Tracks the class nest so 'super' and method compilation know their context.
  struct ClassState {
    ClassState* enclosing;
    bool has_superclass;
  };

  // Statement and expression compilation.
  void compile_stmt(const StmtPtr& stmt);
  void compile_expr(const ExprPtr& expr);
  void compile_function(const std::shared_ptr<FunctionStmt>& decl, FunctionKind kind);
  void compile_class(const ClassStmt& stmt);
  void declare_variable(const Token& name);
  void define_variable(std::uint8_t global);
  std::uint8_t parse_variable(const Token& name);
  void named_variable(const Token& name, bool can_assign, const ExprPtr& value);

  // Scope and local management.
  void begin_scope();
  void end_scope();
  void add_local(const Token& name);
  void mark_initialized();
  int resolve_local(FunctionState* state, const Token& name);
  int resolve_upvalue(FunctionState* state, const Token& name);
  int add_upvalue(FunctionState* state, std::uint8_t index, bool is_local);

  // Emission helpers.
  Chunk& current_chunk() {
    return current_->function->chunk;
  }
  void emit(std::uint8_t byte, int line);
  void emit(OpCode op, int line);
  void emit_two(OpCode op, std::uint8_t operand, int line);
  std::uint8_t make_constant(Constant value);
  void emit_constant(Constant value, int line);
  int emit_jump(OpCode op, int line);
  void patch_jump(int offset);
  void emit_loop(int loop_start, int line);
  void emit_return(int line);

  void error(int line, const std::string& message);

  FunctionState* current_ = nullptr;
  ClassState* current_class_ = nullptr;
  std::vector<CompileError> errors_;
};

}  // namespace lumen
