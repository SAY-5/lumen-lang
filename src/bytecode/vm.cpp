#include "bytecode/vm.h"

#include <chrono>
#include <cmath>

#include "bytecode/opcode.h"

namespace lumen {

namespace {
// The value stack is reserved once and never reallocated, so raw pointers into
// it (used by open upvalues) stay valid for the lifetime of a run.
constexpr std::size_t kStackMax = 1 << 16;
constexpr std::size_t kFramesMax = 1024;
}  // namespace

VM::VM() {
  stack_.reserve(kStackMax);
  frames_.reserve(kFramesMax);
  define_native("clock", 0, [](const std::vector<VmValue>&) -> VmValue {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
  });
}

void VM::define_native(const std::string& name, int arity,
                       std::function<VmValue(const std::vector<VmValue>&)> fn) {
  auto native = std::make_shared<ObjNative>();
  native->name = name;
  native->arity = arity;
  native->fn = std::move(fn);
  globals_[name] = native;
}

void VM::runtime_error(const std::string& message) {
  int line = 0;
  if (!frames_.empty()) {
    const CallFrame& frame = frames_.back();
    std::size_t instr = frame.ip - 1;
    const auto& lines = frame.closure->function->chunk.lines;
    if (instr < lines.size())
      line = lines[instr];
  }
  error_message_ = "[line " + std::to_string(line) + "] runtime error: " + message;
  errored_ = true;
}

InterpretResult VM::run(const FunctionObj& script) {
  auto closure = std::make_shared<ObjClosure>();
  closure->function = script;
  push(closure);
  call(closure, 0);
  return execute();
}

bool VM::call(const ClosureObj& closure, int arg_count) {
  if (arg_count != closure->function->arity) {
    runtime_error("expected " + std::to_string(closure->function->arity) + " arguments but got " +
                  std::to_string(arg_count));
    return false;
  }
  if (frames_.size() >= kFramesMax) {
    runtime_error("stack overflow");
    return false;
  }
  CallFrame frame;
  frame.closure = closure;
  frame.ip = 0;
  frame.slot_base = stack_.size() - arg_count - 1;
  frames_.push_back(std::move(frame));
  return true;
}

bool VM::call_value(const VmValue& callee, int arg_count) {
  if (std::holds_alternative<ClosureObj>(callee)) {
    return call(std::get<ClosureObj>(callee), arg_count);
  }
  if (std::holds_alternative<BoundMethodObj>(callee)) {
    auto bound = std::get<BoundMethodObj>(callee);
    stack_[stack_.size() - arg_count - 1] = bound->receiver;  // bind 'this' in slot 0
    return call(bound->method, arg_count);
  }
  if (std::holds_alternative<ClassObj>(callee)) {
    auto klass = std::get<ClassObj>(callee);
    auto instance = std::make_shared<ObjInstance>();
    instance->klass = klass;
    stack_[stack_.size() - arg_count - 1] = instance;
    auto it = klass->methods.find("init");
    if (it != klass->methods.end()) {
      return call(it->second, arg_count);
    }
    if (arg_count != 0) {
      runtime_error("expected 0 arguments but got " + std::to_string(arg_count));
      return false;
    }
    return true;
  }
  if (std::holds_alternative<NativeObj>(callee)) {
    auto native = std::get<NativeObj>(callee);
    if (arg_count != native->arity) {
      runtime_error("expected " + std::to_string(native->arity) + " arguments but got " +
                    std::to_string(arg_count));
      return false;
    }
    std::vector<VmValue> args(stack_.end() - arg_count, stack_.end());
    VmValue result = native->fn(args);
    stack_.resize(stack_.size() - arg_count - 1);
    push(std::move(result));
    return true;
  }
  runtime_error("can only call functions and classes");
  return false;
}

bool VM::bind_method(const ClassObj& klass, const std::string& name) {
  auto it = klass->methods.find(name);
  if (it == klass->methods.end()) {
    runtime_error("undefined property '" + name + "'");
    return false;
  }
  auto bound = std::make_shared<ObjBoundMethod>();
  bound->receiver = peek(0);
  bound->method = it->second;
  pop();
  push(bound);
  return true;
}

bool VM::invoke_from_class(const ClassObj& klass, const std::string& name, int arg_count) {
  auto it = klass->methods.find(name);
  if (it == klass->methods.end()) {
    runtime_error("undefined property '" + name + "'");
    return false;
  }
  return call(it->second, arg_count);
}

bool VM::invoke(const std::string& name, int arg_count) {
  VmValue receiver = peek(arg_count);
  if (!std::holds_alternative<InstanceObj>(receiver)) {
    runtime_error("only instances have methods");
    return false;
  }
  auto instance = std::get<InstanceObj>(receiver);
  auto field = instance->fields.find(name);
  if (field != instance->fields.end()) {
    stack_[stack_.size() - arg_count - 1] = field->second;
    return call_value(field->second, arg_count);
  }
  return invoke_from_class(instance->klass, name, arg_count);
}

UpvalueObj VM::capture_upvalue(VmValue* local) {
  for (const auto& uv : open_upvalues_) {
    if (uv->location == local)
      return uv;
  }
  auto created = std::make_shared<ObjUpvalue>();
  created->location = local;
  open_upvalues_.push_back(created);
  return created;
}

void VM::close_upvalues(VmValue* last) {
  for (auto it = open_upvalues_.begin(); it != open_upvalues_.end();) {
    if ((*it)->location >= last) {
      (*it)->closed = *(*it)->location;
      (*it)->is_closed = true;
      it = open_upvalues_.erase(it);
    } else {
      ++it;
    }
  }
}

InterpretResult VM::execute() {
  CallFrame* frame = &frames_.back();
  const std::vector<std::uint8_t>* code = &frame->closure->function->chunk.code;
  const std::vector<Constant>* constants = &frame->closure->function->chunk.constants;

  auto read_byte = [&]() -> std::uint8_t { return (*code)[frame->ip++]; };
  auto read_short = [&]() -> std::uint16_t {
    frame->ip += 2;
    return static_cast<std::uint16_t>(((*code)[frame->ip - 2] << 8) | (*code)[frame->ip - 1]);
  };
  auto read_constant = [&]() -> const Constant& { return (*constants)[read_byte()]; };
  auto read_string = [&]() -> std::string { return std::get<std::string>(read_constant()); };
  auto refresh = [&]() {
    frame = &frames_.back();
    code = &frame->closure->function->chunk.code;
    constants = &frame->closure->function->chunk.constants;
  };

  while (true) {
    OpCode op = static_cast<OpCode>(read_byte());
    switch (op) {
      case OpCode::Constant: {
        const Constant& c = read_constant();
        if (std::holds_alternative<double>(c)) {
          push(std::get<double>(c));
        } else if (std::holds_alternative<std::string>(c)) {
          push(std::get<std::string>(c));
        } else if (std::holds_alternative<bool>(c)) {
          push(std::get<bool>(c));
        } else {
          push(std::monostate{});
        }
        break;
      }
      case OpCode::Nil:
        push(std::monostate{});
        break;
      case OpCode::True:
        push(true);
        break;
      case OpCode::False:
        push(false);
        break;
      case OpCode::Pop:
        pop();
        break;
      case OpCode::GetLocal: {
        std::uint8_t slot = read_byte();
        push(stack_[frame->slot_base + slot]);
        break;
      }
      case OpCode::SetLocal: {
        std::uint8_t slot = read_byte();
        stack_[frame->slot_base + slot] = peek(0);
        break;
      }
      case OpCode::GetGlobal: {
        std::string name = read_string();
        auto it = globals_.find(name);
        if (it == globals_.end()) {
          runtime_error("undefined variable '" + name + "'");
          return InterpretResult::RuntimeError;
        }
        push(it->second);
        break;
      }
      case OpCode::DefineGlobal: {
        std::string name = read_string();
        globals_[name] = peek(0);
        pop();
        break;
      }
      case OpCode::SetGlobal: {
        std::string name = read_string();
        if (globals_.find(name) == globals_.end()) {
          runtime_error("undefined variable '" + name + "'");
          return InterpretResult::RuntimeError;
        }
        globals_[name] = peek(0);
        break;
      }
      case OpCode::GetUpvalue: {
        std::uint8_t slot = read_byte();
        push(frame->closure->upvalues[slot]->get());
        break;
      }
      case OpCode::SetUpvalue: {
        std::uint8_t slot = read_byte();
        frame->closure->upvalues[slot]->set(peek(0));
        break;
      }
      case OpCode::GetProperty: {
        if (!std::holds_alternative<InstanceObj>(peek(0))) {
          runtime_error("only instances have properties");
          return InterpretResult::RuntimeError;
        }
        auto instance = std::get<InstanceObj>(peek(0));
        std::string name = read_string();
        auto field = instance->fields.find(name);
        if (field != instance->fields.end()) {
          pop();
          push(field->second);
          break;
        }
        if (!bind_method(instance->klass, name))
          return InterpretResult::RuntimeError;
        break;
      }
      case OpCode::SetProperty: {
        if (!std::holds_alternative<InstanceObj>(peek(1))) {
          runtime_error("only instances have fields");
          return InterpretResult::RuntimeError;
        }
        auto instance = std::get<InstanceObj>(peek(1));
        std::string name = read_string();
        VmValue value = peek(0);
        instance->fields[name] = value;
        pop();  // value
        pop();  // instance
        push(value);
        break;
      }
      case OpCode::GetSuper: {
        std::string name = read_string();
        auto superclass = std::get<ClassObj>(pop());
        if (!bind_method(superclass, name))
          return InterpretResult::RuntimeError;
        break;
      }
      case OpCode::Equal: {
        VmValue b = pop();
        VmValue a = pop();
        push(vm_values_equal(a, b));
        break;
      }
      case OpCode::Greater:
      case OpCode::Less:
      case OpCode::Subtract:
      case OpCode::Multiply:
      case OpCode::Divide: {
        if (!std::holds_alternative<double>(peek(0)) || !std::holds_alternative<double>(peek(1))) {
          runtime_error("operands must be numbers");
          return InterpretResult::RuntimeError;
        }
        double b = std::get<double>(pop());
        double a = std::get<double>(pop());
        switch (op) {
          case OpCode::Greater:
            push(a > b);
            break;
          case OpCode::Less:
            push(a < b);
            break;
          case OpCode::Subtract:
            push(a - b);
            break;
          case OpCode::Multiply:
            push(a * b);
            break;
          case OpCode::Divide:
            push(a / b);
            break;
          default:
            break;
        }
        break;
      }
      case OpCode::Add: {
        if (std::holds_alternative<double>(peek(0)) && std::holds_alternative<double>(peek(1))) {
          double b = std::get<double>(pop());
          double a = std::get<double>(pop());
          push(a + b);
        } else if (std::holds_alternative<std::string>(peek(0)) &&
                   std::holds_alternative<std::string>(peek(1))) {
          std::string b = std::get<std::string>(pop());
          std::string a = std::get<std::string>(pop());
          push(a + b);
        } else {
          runtime_error("operands must be two numbers or two strings");
          return InterpretResult::RuntimeError;
        }
        break;
      }
      case OpCode::Not:
        push(!vm_is_truthy(pop()));
        break;
      case OpCode::Negate: {
        if (!std::holds_alternative<double>(peek(0))) {
          runtime_error("operand must be a number");
          return InterpretResult::RuntimeError;
        }
        push(-std::get<double>(pop()));
        break;
      }
      case OpCode::Print: {
        output_ += vm_stringify(pop());
        output_ += '\n';
        break;
      }
      case OpCode::Jump: {
        std::uint16_t offset = read_short();
        frame->ip += offset;
        break;
      }
      case OpCode::JumpIfFalse: {
        std::uint16_t offset = read_short();
        if (!vm_is_truthy(peek(0)))
          frame->ip += offset;
        break;
      }
      case OpCode::Loop: {
        std::uint16_t offset = read_short();
        frame->ip -= offset;
        break;
      }
      case OpCode::Call: {
        int arg_count = read_byte();
        if (!call_value(peek(arg_count), arg_count))
          return InterpretResult::RuntimeError;
        refresh();
        break;
      }
      case OpCode::Invoke: {
        std::string method = read_string();
        int arg_count = read_byte();
        if (!invoke(method, arg_count))
          return InterpretResult::RuntimeError;
        refresh();
        break;
      }
      case OpCode::SuperInvoke: {
        std::string method = read_string();
        int arg_count = read_byte();
        auto superclass = std::get<ClassObj>(pop());
        if (!invoke_from_class(superclass, method, arg_count)) {
          return InterpretResult::RuntimeError;
        }
        refresh();
        break;
      }
      case OpCode::Closure: {
        auto fn = std::get<FunctionObj>(read_constant());
        auto closure = std::make_shared<ObjClosure>();
        closure->function = fn;
        closure->upvalues.resize(fn->upvalue_count);
        for (int i = 0; i < fn->upvalue_count; ++i) {
          std::uint8_t is_local = read_byte();
          std::uint8_t index = read_byte();
          if (is_local) {
            closure->upvalues[i] = capture_upvalue(&stack_[frame->slot_base + index]);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        push(closure);
        break;
      }
      case OpCode::CloseUpvalue: {
        close_upvalues(&stack_.back());
        pop();
        break;
      }
      case OpCode::Return: {
        VmValue result = pop();
        close_upvalues(&stack_[frame->slot_base]);
        std::size_t base = frame->slot_base;
        frames_.pop_back();
        if (frames_.empty()) {
          pop();  // the script closure
          return InterpretResult::Ok;
        }
        stack_.resize(base);
        push(std::move(result));
        refresh();
        break;
      }
      case OpCode::Class: {
        auto klass = std::make_shared<ObjClass>();
        klass->name = read_string();
        push(klass);
        break;
      }
      case OpCode::Inherit: {
        if (!std::holds_alternative<ClassObj>(peek(1))) {
          runtime_error("superclass must be a class");
          return InterpretResult::RuntimeError;
        }
        auto superclass = std::get<ClassObj>(peek(1));
        auto subclass = std::get<ClassObj>(peek(0));
        for (const auto& [name, method] : superclass->methods) {
          subclass->methods[name] = method;
        }
        pop();  // subclass
        break;
      }
      case OpCode::Method: {
        std::string name = read_string();
        auto method = std::get<ClosureObj>(peek(0));
        auto klass = std::get<ClassObj>(peek(1));
        klass->methods[name] = method;
        pop();
        break;
      }
    }
  }
}

}  // namespace lumen
