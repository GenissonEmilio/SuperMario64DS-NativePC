// port/hal/smoke_cstd_abs.cpp -- cstd::abs for the narrow smoke harnesses,
// lane INT093F. src/cstd4.cpp is the whole runtime-library TU since main's
// merge (printf core, hand-asm primitives) and cannot be listed raw on a
// harness; ExpandingHeapAllocator::MaxAllocatableSize is the one thing the
// harnesses want from it. This is the body hal/heap_globals.cpp carried before
// PORTSYNC3 retired it, and the one src/cstd4.cpp has.

namespace cstd {
int abs(int x) { return x < 0 ? -x : x; }
}
