# Lumen

Small object-oriented interpreted language. C++20 implementation with a hand-written lexer, recursive-descent parser, AST, and tree-walking evaluator. Optional bytecode compiler + stack VM and a mark-and-sweep garbage collector.

## What it does

Lumen runs programs like this:

```
class Counter {
  init(start) { this.count = start; }
  inc() { this.count = this.count + 1; }
  value() { return this.count; }
}

var c = Counter(10);
c.inc(); c.inc(); c.inc();
print c.value();  // 13

fun makeAdder(x) {
  fun add(y) { return x + y; }
  return add;
}
var add5 = makeAdder(5);
print add5(7);    // 12
```

Features: numbers, strings, booleans, nil, variables with lexical scoping, `if`/`while`/`for`, functions with closures, classes with methods, single inheritance, `this`, `super`, `print`.

## Build

```
make build
./build/lumen examples/counter.lum
./build/lumen                 # REPL
```

## Test

```
make test          # GoogleTest unit + e2e
make fuzz-smoke    # libFuzzer parser harness, 5k iters
make bench         # writes bench/results/bench_local.json
make bench-regress # fails if any benchmark drifts >30%
```

Sanitizers:

```
make asan
make tsan
```

## CLI

```
lumen <file>                  # run a .lum file
lumen                         # REPL
lumen --engine=tree <file>    # tree-walking interpreter (default)
lumen --engine=bytecode <file># compile to bytecode and run on the stack VM
```

## Architecture

```
src/
  lexer/      hand-written single-pass lexer, no regex
  parser/     recursive-descent parser with precedence climbing
  eval/       tree-walking interpreter, environment chain, classes, GC
  bytecode/   AST to bytecode compiler + stack VM
  repl/       interactive loop
```

The grammar is documented in [docs/grammar.md](docs/grammar.md). AST shape in [docs/ast-shape.md](docs/ast-shape.md). Evaluator semantics in [docs/evaluator.md](docs/evaluator.md). Closure capture in [docs/closure-design.md](docs/closure-design.md). Bytecode VM in [docs/bytecode-vm.md](docs/bytecode-vm.md). GC in [docs/gc-design.md](docs/gc-design.md).

## How this fits with the rest of the portfolio

The rest of my C++ portfolio is systems and trading infrastructure: `ledgercore` (double-entry payments ledger), `orderbook-sim` and `orderbook-fix` (matching engines), `mdfeed-handler` and `mdfeed-itch` (UDP market data feed handlers), `inference-router` (TCP request router), `columnstore` (AVX2 columnar engine), `raftkv` (distributed consensus). None of those is a language implementation.

Lumen is the language project. It exists to demonstrate the parts that don't show up in the other repos: lexing, parsing, AST design, tree-walking evaluation, lexical scoping with closure capture, OO semantics including method dispatch through an inheritance chain, a bytecode compiler with a stack VM, and a mark-and-sweep garbage collector. The interpreter is one ~4k LOC C++20 codebase plus a GoogleTest suite covering each language feature.

## Performance

See `bench/results/bench_local.json` for committed numbers. The bench harness reports ops/sec, wall-clock, and peak RSS for three programs: `fib(30)`, `bubble_sort(1000)`, `mandelbrot(40,40,20)`. Re-run with `make bench`. The bench-regress gate fires at 30% drift.

## License

MIT. See [LICENSE](LICENSE).
