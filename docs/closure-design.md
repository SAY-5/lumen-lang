# Closure capture

Lumen closures capture their defining environment by shared pointer, so an
inner function keeps the enclosing call's locals alive and sees their updates.

## How capture works

When a `fun` declaration executes, the interpreter builds a `LumenFunction`
holding a `shared_ptr<Environment>` to the scope in which it was declared. A
call then runs the body in a new environment chained to that captured scope:

```
counter() call env        <- holds c = 0, and inc (a LumenFunction)
   ^
   | enclosing
inc() call env            <- created per call to inc()
```

Because `inc`'s closure is the `counter` call environment, every call to `inc`
reads and writes the same `c`. Two separate calls to `counter()` produce two
independent environments, hence two independent counters.

## Worked example

```
fun counter() {
  var c = 0;
  fun inc() { c = c + 1; return c; }
  return inc;
}
var n = counter();
print n();  // 1
print n();  // 2
```

The returned `inc` outlives the `counter()` call frame; the shared pointer keeps
that frame's environment alive for as long as `inc` is reachable.

## Reference cycles

The capture creates a cycle: the `counter` environment owns `inc`, and `inc`
owns that same environment through its closure. With reference counting alone
these cycles are only reclaimed at process exit. Collecting them during
execution is the responsibility of the mark-and-sweep collector documented in
[gc-design.md](gc-design.md). The CI sanitizer job therefore runs with
`detect_leaks=0` for the tree-walker while keeping all memory-safety and
undefined-behaviour checks active.
