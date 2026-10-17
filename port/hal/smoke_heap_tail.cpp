// port/hal/smoke_heap_tail.cpp -- what the narrow smoke harnesses (smoke_heap
// through smoke_clsn) stopped linking on after the 2026-10 main merge
// (PORTSYNC3), in one place for all of them. Lane INT093F.
//
// Main folded the fourteen SolidHeapAllocator shards into one TU, moved
// Heap's helpers under the ROM's mHeap::Heap_t family, and made src/cstd4.cpp
// the whole runtime-library TU. Three things followed for the harnesses:
//
//   1. (hal/smoke_cstd_abs.cpp) src/cstd4.cpp can no longer be listed raw. It carries the printf core,
//      and that asks for strlen, strchr, strncpy, five leftover one-function
//      sources and a dozen OS data words that no narrow harness has. The
//      harnesses only ever wanted cstd::abs out of it (ExpandingHeapAllocator::
//      MaxAllocatableSize), so the definition is the one hal/heap_globals.cpp
//      used to carry before it was retired: 'x < 0 ? -x : x', the same
//      three-instruction body src/cstd4.cpp has.
//   2. src/engine/heap/Heap.cpp's SolidHeap layer reaches SolidHeapAllocator,
//      so that TU joins the list (it also holds func_0204ebb8, the VDestroy
//      forward that used to be its own file).
//   3. Heap.cpp reads three words the harnesses never defined because the
//      older Heap spelling did not: the byte at 0x020a0e98 (Intact's flag),
//      Memory::tmpHeapPtr at 0x020a0ea8, and the GAME_HEAP_PTR word, which is
//      the game heap pointer (the window build's storage for it is
//      data_020a0eac_c in hal/cxxname_bridge.cpp, which a harness does not link).
//
// Not linked into any target that already defines these (the actor trio takes
// them from smoke_host_globals_actor.cpp and smoke_aliases_actor.cpp, and
// walk_window from hal/cxx_aliases.cpp).

#include "SolidHeapAllocator.h"
#include <new>

// SolidHeapAllocator's constructor under the flat spelling Heap.cpp's
// CreateSolidHeapAllocator asks for: the placement the decorated constructor
// needs, copied from hal/smoke_heap_seam_actor.cpp (same reason, same body).
extern "C" SolidHeapAllocator *_ZN18SolidHeapAllocatorC1EPvj(
    SolidHeapAllocator *self, void *heapEnd, u32 flags)
{ return new (self) SolidHeapAllocator(heapEnd, flags); }


extern "C" {
unsigned char data_020a0e98;
void *data_020a0ea8;
void *GAME_HEAP_PTR;
}
