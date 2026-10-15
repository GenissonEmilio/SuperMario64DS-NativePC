/* func_0203cc0c at 0x0203cc0c -- allocates from the default heap via
 * Heap::Allocate(unsigned) (mangled names referenced directly from C). */

extern void* data_020a0ea0;
extern void* _ZN5mHeap6Heap_t8AllocateEj(void* heap, unsigned int size);

void* func_0203cc0c(unsigned int size)
{
    return _ZN5mHeap6Heap_t8AllocateEj(data_020a0ea0, size);
}
