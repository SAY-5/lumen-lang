# Evaluator semantics

The tree-walking evaluator lives in `src/eval/`. It walks the AST directly,
holding runtime state in a chain of `Environment` scopes.

## Values

`Value` (`src/eval/value.h`) is a `std::variant` over: `std::monostate` (nil),
`double`, `bool`, `std::string`, `CallablePtr` (functions and native builtins),
`ClassPtr`, and `InstancePtr`. Numbers are IEEE-754 doubles. Truthiness: `nil`
and `false` are falsey; everything else, including `0`, is truthy.

## Environments

An `Environment` maps names to values and points at an enclosing scope. Variable
lookup and assignment walk the chain outward until the name is found, raising a
`RuntimeError` if it is not. Blocks, function bodies, and class method tables
each get their own environment.

## Statements

`print` writes `stringify(value)` plus a newline to an internal buffer that the
driver flushes to stdout (keeping unit tests hermetic). `var` defines a name in
the current scope, defaulting to nil. `if`, `while`, and the desugared `for`
evaluate their condition through `is_truthy`. `return` unwinds the current call
by throwing a `ReturnSignal` caught in the function-call boundary.

## Expressions

Arithmetic and comparison operators require numeric operands and raise a typed
`RuntimeError` otherwise. `+` is overloaded for string concatenation when both
operands are strings. `==` and `!=` compare within a type and are always false
across types. `and`/`or` short-circuit and yield the operand that decided the
result, not a coerced boolean.

## Functions and closures

A `fun` declaration creates a `LumenFunction` capturing the defining
environment. Calling it allocates a fresh environment whose parent is that
captured closure, binds the parameters, and executes the body. Because the
closure is captured by value of the shared pointer, nested functions observe
and mutate the variables of their enclosing call. See
[closure-design.md](closure-design.md).

## Classes

`class` evaluates its superclass (which must itself be a class), then builds a
`ClassValue` holding its methods. Calling a class constructs an `InstanceValue`
and, if present, runs `init`. Method access binds `this` by creating a one-slot
environment in front of the method's closure. `super` resolves the method on the
statically known superclass and binds it to the current `this`. Single
inheritance is supported to arbitrary depth; `super.method()` chains up one
level at a time.

## Errors

Lex and parse errors abort before evaluation with exit code 65. A `RuntimeError`
escaping the program reports `[line N] runtime error: <message>` and exits 70.
