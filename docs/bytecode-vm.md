# Bytecode compiler and stack VM

Lumen ships two execution engines behind the `--engine` flag. The default
`tree` engine walks the AST directly (see [evaluator.md](evaluator.md)); the
`bytecode` engine compiles the AST to a flat instruction stream and runs it on a
stack-based virtual machine. Both engines produce identical observable behaviour
on the entire v1 golden corpus, which the corpus test enforces by running every
program through both.

## Pipeline

```
source -> lexer -> parser -> AST -> compiler -> Chunk -> VM
```

The compiler (`src/bytecode/compiler.cpp`) consumes the same AST the
tree-walker uses, so the front end is shared. It emits a top-level `<script>`
function whose `Chunk` holds the bytecode, a constant pool, and a per-byte line
table for diagnostics.

## Instruction set

The VM is stack based: instructions pop their operands off the value stack and
push their result. The opcodes (`src/bytecode/opcode.h`) cover constants and
literals (`Constant`, `Nil`, `True`, `False`), variable access across all three
storage classes (`GetLocal`/`SetLocal`, `GetGlobal`/`DefineGlobal`/`SetGlobal`,
`GetUpvalue`/`SetUpvalue`), arithmetic and comparison (`Add`, `Subtract`,
`Multiply`, `Divide`, `Negate`, `Equal`, `Greater`, `Less`, `Not`), control flow
(`Jump`, `JumpIfFalse`, `Loop`), calls and returns (`Call`, `Return`), closures
(`Closure`, `CloseUpvalue`), and the object model (`Class`, `Inherit`, `Method`,
`GetProperty`, `SetProperty`, `GetSuper`, plus the `Invoke`/`SuperInvoke` method
fast paths).

## Locals, globals, and upvalues

The compiler resolves each variable reference at compile time:

- Locals live in stack slots relative to the call frame and are reached by slot
  index. Block scoping is tracked with a scope-depth counter; leaving a scope
  pops or closes the locals declared in it.
- Free variables captured by a nested function become upvalues. The compiler
  walks the enclosing function chain, marks the captured local, and records
  whether each upvalue refers to an enclosing local or an enclosing upvalue.
- Everything else is a global, looked up by name in a hash map.

## Closures and upvalues at runtime

An upvalue is *open* while the variable it captures is still on the stack, in
which case it points directly at that slot, so the closure sees live updates.
When the owning frame returns, `close_upvalues` copies the value inline and the
upvalue becomes *closed*, keeping the closure valid afterwards. Open upvalues
are de-duplicated so two closures capturing the same variable share one cell.
Because open upvalues hold raw pointers into the value stack, the stack is
reserved once at a fixed capacity and never reallocated.

## Classes, methods, and super

`Class` pushes a fresh class object; `Method` binds a compiled closure into its
method table; `Inherit` copies the superclass method table into the subclass.
Property reads check instance fields first, then fall back to binding a method
to the receiver. `super.m()` is compiled to load `this` and the captured
superclass and dispatch through `SuperInvoke`, so calls resolve up the chain one
level at a time, matching the tree-walker.

## Performance

Compiling away environment lookups and the exception-based return path is where
the VM wins. Measured locally (Apple silicon, release build), full-size
benchmarks:

| benchmark                 | tree engine | bytecode engine | speedup |
|---------------------------|-------------|-----------------|---------|
| fib(30)                   | 25.41 s     | 0.32 s          | ~79x    |
| mandelbrot(40,40,20)      | 10.8 ms     | 5.2 ms          | ~2.1x   |
| bubble_sort(1000)         | 8.9 ms      | 8.9 ms          | ~1.0x   |

`fib` is call dominated, exactly the cost the VM eliminates. `bubble_sort` is
bound by instance allocation, which both engines share, so it barely moves. Run
the comparison with `make build` then
`./build/lumen_bench --engine=tree` and `--engine=bytecode`.
