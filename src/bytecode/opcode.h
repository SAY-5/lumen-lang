#pragma once

#include <cstdint>

namespace lumen {

// Stack-machine instruction set. Operands, where present, are the bytes that
// immediately follow the opcode in the chunk's code array.
enum class OpCode : std::uint8_t {
  Constant,      // operand: constant index -> push constants[idx]
  Nil,           // push nil
  True,          // push true
  False,         // push false
  Pop,           // discard top of stack
  GetLocal,      // operand: slot -> push frame slot
  SetLocal,      // operand: slot -> store top into frame slot (keeps top)
  GetGlobal,     // operand: name-constant -> push global
  DefineGlobal,  // operand: name-constant -> define global from top, pop
  SetGlobal,     // operand: name-constant -> assign global from top (keeps top)
  GetUpvalue,    // operand: upvalue index -> push captured variable
  SetUpvalue,    // operand: upvalue index -> store top into captured variable
  GetProperty,   // operand: name-constant -> instance field/method off top
  SetProperty,   // operand: name-constant -> set instance field
  GetSuper,      // operand: name-constant -> bound superclass method
  Equal,         // a == b
  Greater,       // a > b
  Less,          // a < b
  Add,           // a + b (numbers or strings)
  Subtract,      // a - b
  Multiply,      // a * b
  Divide,        // a / b
  Not,           // logical not of top
  Negate,        // arithmetic negate of top
  Print,         // pop and print
  Jump,          // operand: 16-bit offset -> unconditional forward jump
  JumpIfFalse,   // operand: 16-bit offset -> jump if top is falsey (top kept)
  Loop,          // operand: 16-bit offset -> unconditional backward jump
  Call,          // operand: arg count -> call value below the args
  Invoke,        // operands: name-constant, arg count -> method call fast path
  SuperInvoke,   // operands: name-constant, arg count -> super method call
  Closure,       // operand: fn-constant, then per-upvalue (isLocal, index)
  CloseUpvalue,  // close the upvalue at the top of the stack, then pop
  Return,        // return top from the current call
  Class,         // operand: name-constant -> push a fresh class
  Inherit,       // copy methods from superclass (below) into subclass (top)
  Method,        // operand: name-constant -> bind closure (top) as a method
};

}  // namespace lumen
