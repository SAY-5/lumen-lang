#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lumen {

// Object kinds tracked by the collector. Functions and natives are owned by
// shared_ptr (rooted by the constant pool, never cyclic) and are not GC
// objects; everything that can form a reference cycle at runtime is.
enum class ObjType : std::uint8_t {
  Closure,
  Upvalue,
  Class,
  Instance,
  BoundMethod,
};

// Intrusive base for every garbage-collected object. The collector links all
// live objects through `next` and flips `marked` during the mark phase.
struct Obj {
  explicit Obj(ObjType type) : type(type) {
  }
  virtual ~Obj() = default;

  ObjType type;
  bool marked = false;
  Obj* next = nullptr;
};

// Global sentinel: incremented every time the collector frees an object.
// Tests observe it to prove that an unreachable cycle was actually reclaimed.
extern std::size_t g_gc_freed_count;

class VM;

// A tri-color mark-and-sweep collector. White = unmarked (candidate for
// sweeping), gray = marked but children not yet traced (held on the worklist),
// black = marked with children traced. The collector owns every Obj it
// allocates and frees the unreachable ones on collect().
class GarbageCollector {
 public:
  explicit GarbageCollector(VM& vm) : vm_(vm) {
  }
  ~GarbageCollector();

  // Allocates a T (deriving from Obj), tracks it, and may trigger a collection
  // first when the allocation threshold is crossed.
  template <typename T, typename... Args>
  T* allocate(Args&&... args);

  // Marks reachable objects from the VM roots and frees the rest.
  void collect();

  // Marks a single object gray (public so value-marking helpers can reach it).
  void mark(Obj* obj);

  // Returns true if an automatic collection is due (threshold crossed).
  bool should_collect() const {
    return allocations_since_gc_ >= threshold_;
  }

  std::size_t live_count() const {
    return live_count_;
  }
  std::size_t collections() const {
    return collections_;
  }

  // Allocations between automatic collections. Lower values stress the
  // collector; the VM uses a modest threshold so long programs stay bounded.
  void set_threshold(std::size_t n) {
    threshold_ = n;
  }

 private:
  void track(Obj* obj);
  void mark_value_roots();
  void trace_references();
  void blacken(Obj* obj);
  void sweep();

  VM& vm_;
  Obj* objects_ = nullptr;        // intrusive list head of all live objects
  std::vector<Obj*> gray_stack_;  // the gray worklist during a collection
  std::size_t live_count_ = 0;
  std::size_t allocations_since_gc_ = 0;
  std::size_t threshold_ = 1024;
  std::size_t collections_ = 0;
};

template <typename T, typename... Args>
T* GarbageCollector::allocate(Args&&... args) {
  T* obj = new T(std::forward<Args>(args)...);
  track(obj);
  return obj;
}

}  // namespace lumen
