# Garbage collection

The bytecode VM owns its runtime heap objects through a tri-color
mark-and-sweep collector (`src/bytecode/gc.h`, `gc.cpp`). It reclaims objects
that are no longer reachable from any root, including objects trapped in
reference cycles that reference counting alone could never free.

## What the collector manages

Objects that can form cycles at runtime are GC owned and referenced by raw
pointer: closures, upvalues, classes, instances, and bound methods. Compiled
functions and native builtins are acyclic and rooted by the constant pool and
the globals table, so they stay `shared_ptr` and are never collected. Every GC
object derives from `Obj`, which carries the tri-color `marked` flag and an
intrusive `next` link threading all live objects into one list.

## Tri-color mark-and-sweep

A collection has two phases:

1. **Mark.** Starting from the roots, every reachable object is marked. The
   algorithm is tri-color: white objects are unmarked (collection candidates),
   gray objects are marked but their children are not yet traced (they sit on a
   worklist), and black objects are marked with all children traced. Roots are
   pushed gray; the worklist is drained by *blackening* each object, which marks
   the objects it directly references (pushing them gray in turn). When the
   worklist empties, every reachable object is black and everything still white
   is garbage.
2. **Sweep.** The intrusive object list is walked; white objects are unlinked
   and deleted, black objects are reset to white for the next cycle.

## Roots

The roots are exactly the VM's live state: the value stack, every call frame's
closure, the open-upvalue list, and the globals table. The collector is a
`friend` of `VM` so it can traverse these directly. Tracing then reaches the
rest of the graph: a closure marks its upvalues, an upvalue marks its closed
value, a class marks its method closures, an instance marks its class and every
field value, and a bound method marks its receiver and method.

## When collection runs

Collection is triggered two ways:

- **Automatically**, when the number of allocations since the last collection
  crosses a threshold. The check happens only at instruction boundaries in the
  dispatch loop, which are GC safe points: every live value is reachable from a
  root and nothing is half-constructed. This keeps long-running programs (for
  example a tight allocation loop) from growing the heap without bound.
- **Explicitly**, via the `gc()` builtin, which forces a full collection and
  returns the number of objects still live. The tree-walking engine also exposes
  `gc()` as a no-op (it relies on reference counting) so programs behave
  identically under both engines.

## Proving cycle collection

`examples/gc_cycle.lum` and the `GC.CollectsUnreachableCycle` test build two
instances that point at each other (`first.link = second; second.link = first`),
let every root to them go out of scope, then call `gc()`. A global sentinel,
`g_gc_freed_count`, is incremented for each object the sweep frees; the test
asserts it grows across the `gc()` call, which can only happen if the collector
reclaimed the cycle. `GC.PreservesReachableObjects` checks the dual: an object
still rooted in globals survives a collection and remains usable.

## Trade-offs

- **Stop-the-world, non-incremental.** Collection runs to completion in one
  pass. This is simple and predictable for a teaching interpreter but pauses
  execution; an incremental or generational collector would shorten pauses at
  the cost of write barriers and added complexity.
- **Allocation-count trigger, not heap-size trigger.** Triggering on a fixed
  allocation count is trivial to reason about but ignores object size. A
  production collector would grow the threshold with the live-heap size to
  amortize collection cost.
- **Mixed ownership.** Keeping functions and natives on `shared_ptr` avoids
  tracing immortal, acyclic objects, but means the heap is not uniformly GC
  managed. A single ownership model would be cleaner at the price of tracing
  objects that never need collecting.
- **Raw-pointer stack.** Open upvalues hold raw pointers into the value stack,
  so the stack is reserved once at a fixed capacity and never reallocated.
