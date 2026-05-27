#include "bytecode/gc.h"

#include "bytecode/vm.h"
#include "bytecode/vm_value.h"

namespace lumen {

std::size_t g_gc_freed_count = 0;

GarbageCollector::~GarbageCollector() {
  Obj* obj = objects_;
  while (obj != nullptr) {
    Obj* next = obj->next;
    delete obj;
    obj = next;
  }
}

void GarbageCollector::track(Obj* obj) {
  obj->next = objects_;
  objects_ = obj;
  ++live_count_;
  ++allocations_since_gc_;
}

void GarbageCollector::mark(Obj* obj) {
  if (obj == nullptr || obj->marked)
    return;
  obj->marked = true;
  gray_stack_.push_back(obj);
}

namespace {

// Marks the heap object inside a value (no-op for scalars and natives).
void mark_value_obj(GarbageCollector& gc, const VmValue& v) {
  if (std::holds_alternative<ObjClosure*>(v)) {
    gc.mark(std::get<ObjClosure*>(v));
  } else if (std::holds_alternative<ObjClass*>(v)) {
    gc.mark(std::get<ObjClass*>(v));
  } else if (std::holds_alternative<ObjInstance*>(v)) {
    gc.mark(std::get<ObjInstance*>(v));
  } else if (std::holds_alternative<ObjBoundMethod*>(v)) {
    gc.mark(std::get<ObjBoundMethod*>(v));
  }
}

}  // namespace

void GarbageCollector::mark_value_roots() {
  // Roots come from the VM: the value stack, every frame's closure, the open
  // upvalue list, and the globals table.
  for (const VmValue& v : vm_.stack_)
    mark_value_obj(*this, v);
  for (const auto& frame : vm_.frames_)
    mark(frame.closure);
  for (ObjUpvalue* uv : vm_.open_upvalues_)
    mark(uv);
  for (const auto& [name, value] : vm_.globals_)
    mark_value_obj(*this, value);
}

void GarbageCollector::blacken(Obj* obj) {
  switch (obj->type) {
    case ObjType::Upvalue:
      mark_value_obj(*this, static_cast<ObjUpvalue*>(obj)->closed);
      break;
    case ObjType::Closure: {
      auto* closure = static_cast<ObjClosure*>(obj);
      for (ObjUpvalue* uv : closure->upvalues)
        mark(uv);
      break;
    }
    case ObjType::Class: {
      auto* klass = static_cast<ObjClass*>(obj);
      for (const auto& [name, method] : klass->methods)
        mark(method);
      break;
    }
    case ObjType::Instance: {
      auto* instance = static_cast<ObjInstance*>(obj);
      mark(instance->klass);
      for (const auto& [name, value] : instance->fields)
        mark_value_obj(*this, value);
      break;
    }
    case ObjType::BoundMethod: {
      auto* bound = static_cast<ObjBoundMethod*>(obj);
      mark_value_obj(*this, bound->receiver);
      mark(bound->method);
      break;
    }
  }
}

void GarbageCollector::trace_references() {
  while (!gray_stack_.empty()) {
    Obj* obj = gray_stack_.back();
    gray_stack_.pop_back();
    blacken(obj);
  }
}

void GarbageCollector::sweep() {
  Obj* previous = nullptr;
  Obj* obj = objects_;
  while (obj != nullptr) {
    if (obj->marked) {
      obj->marked = false;  // reset for the next cycle
      previous = obj;
      obj = obj->next;
    } else {
      Obj* unreached = obj;
      obj = obj->next;
      if (previous != nullptr) {
        previous->next = obj;
      } else {
        objects_ = obj;
      }
      delete unreached;
      --live_count_;
      ++g_gc_freed_count;
    }
  }
}

void GarbageCollector::collect() {
  ++collections_;
  mark_value_roots();
  trace_references();
  sweep();
  allocations_since_gc_ = 0;
}

}  // namespace lumen
