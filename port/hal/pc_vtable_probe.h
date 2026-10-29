// A calling-convention probe must not dereference an arbitrary caller-saved register.
#ifndef PC_VTABLE_PROBE_H
#define PC_VTABLE_PROBE_H
#include <windows.h>
#include <stdint.h>
static inline bool pc_has_readable_vtable(const void *receiver, const void *table)
{
    if (!receiver || !table) return false;
    MEMORY_BASIC_INFORMATION m;
    if (!VirtualQuery(receiver,&m,sizeof m) || m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return false;
    const DWORD readable=PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
    if (!(m.Protect&readable)) return false;
    const uintptr_t p=(uintptr_t)receiver,base=(uintptr_t)m.BaseAddress;
    if (p<base || m.RegionSize<sizeof(void*) || p-base>m.RegionSize-sizeof(void*)) return false;
    return *(const void *const *)receiver==table;
}
#endif
