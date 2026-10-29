// Original ROM-free reproduction of the native host's initialization order. MIT.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include "../hal/pc_vtable_probe.h"
extern "C" {
void port_os_lock_words_seed();int func_02057020();void func_02057078(int);
void port_backup_transfer_end(int){}
}
int main()
{
    void *page=VirtualAlloc((void*)0x027f0000,0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(page!=(void*)0x027f0000){std::puts("FAIL: cannot map private fixture page");return 1;}
    int count=0,failed=0;
    auto check=[&](const char *s,bool ok){++count;if(!ok){++failed;std::printf("FAIL %s\n",s);}};
    check("unseeded allocator exposes missing initialization",func_02057020()==-3);
    port_os_lock_words_seed();check("NitroFS receives valid first lock",func_02057020()==0x40);
    port_os_lock_words_seed();check("later ROM pre-main does not reuse live lock",func_02057020()==0x41);
    func_02057078(0x40);check("released lock reusable",func_02057020()==0x40);
    bool sequence=true;for(int id=0x42;id<=0x6f;++id)sequence&=func_02057020()==id;
    check("all remaining real lock IDs preserved",sequence);
    check("reserved IDs never allocated",func_02057020()==-3);
    int table=0;void *object[]={&table,0};
    check("real receiver recognized",pc_has_readable_vtable(object,&table));
    check("unrelated table rejected",!pc_has_readable_vtable(object,object));
    check("null receiver rejected",!pc_has_readable_vtable(0,&table));
    check("arbitrary ECX word never dereferenced",!pc_has_readable_vtable((void*)0x34b6df4b,&table));
    check("read across region boundary rejected",!pc_has_readable_vtable((char*)page+0xfffe,&table));
    VirtualFree(page,0,MEM_RELEASE);
    std::printf("PC boot order: %d checks, %d failures\n",count,failed);return failed?1:0;
}
