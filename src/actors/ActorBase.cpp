//cpp
/* leftovers:
 * - Source order stays highest address first. Reordering moves the run.
 * - The destructors stay extern "C". A real destructor stores the
 *   vptr at addend 8. The ROM pools hold the slot array at addend 0.
 * - InitResources stays a free function. As a method it emits
 *   _ZTV7fBase_c a second time.
 * - decl_common.h stays out. Its spellings of these list heads
 *   disagree with the calls in this file.
 * - Heap locals stay void*. Virtual34 and Virtual38 still read
 *   the flags word at +4 and the heap top at +8.
 * - func_0203* and data_020a4b* keep their linker names. They are
 *   the list operations and the four global list heads.
 *
 * fBase_c, the root of the actor hierarchy. arm9 .text
 * 0x020433b8..0x020440e8.
 */
#include "fBase_c.h"
#include "Heap.h"

struct fBaseActorInfo {
    u32 unk_000;
    u16 behaviorPriority;
    u16 renderPriority;
};

/* Keep the list-head and pointer signatures used here local: decl_common.h
 * gives incompatible integer declarations for some of these symbols. */
extern "C" {

/* The ABI destructor variants below destroy both process-list nodes. */
extern "C" void _ZN11fLiNdBaPr_cD1Ev(void *node);

/* Intrusive-list operations on the four global list heads below. All four take
   (list head, node); the legacy files spelled the heads `int`, `char` and
   `int[]` in different files and passed `&x` or `x` accordingly. Unified on the
   array spelling, so every call site passes the bare name and every reloc is
   the same addend-0 reference it always was. */
extern int  func_0203b3c0(void *list, void *node);
extern int  func_0203b27c(void *list, void *node);
extern int  func_0203b20c(void *list, void *node);
extern int  func_0203b244(void *list, void *node);
extern int  func_0204405c(void *list, void *node);
extern int  func_0203b438(void *root, void *node, void *parent);
extern int  func_0203b2ec(void *l, void *n, void *p);

extern void *data_020a4b68;
extern int  data_020a4b6c[];
extern int  data_020a4b78[];
extern int  data_020a4b88[];
extern int  data_020a4b98[];
extern int  data_020a4ba8[];
extern int  data_02099f24[];
extern u32  data_02099e70;
extern u32  data_020a4b60;
extern u16  data_020a4b54;
extern u8   data_020a4b48;
extern void *data_020a4b64;
extern fBaseActorInfo **data_020a4bb8;

extern void func_02044334(void *p);
extern int func_0204335c(void *self);
extern int  func_0204424c(char *p);
extern void *func_0206e2f8(void *p, int fill, unsigned int size);

/* Returns the parent actor. Declared once, up here, because the
 * caller is emitted above the definition. */
extern void *func_02043810(void *p);

/* The class's own vtable, as DATA. Named, not defined: see the destructor note
   in the file banner for why this reference has to be addend 0. config/arm9
   carries two symbols at 0x02099edc -- `_ZTV7fBase_c` and the older
   placeholder `data_02099edc` -- and the legacy files used both. This is the
   one that says what it is. */
extern void *_ZTV7fBase_c[];

/* Heap's methods come from include/Heap.h. The heap locals stay `void *`
   (and are cast at each call), deliberately: Virtual34 and Virtual38 both
   reach two of its fields by raw offset (`+4` flags word, `+8` top-of-heap)
   rather than by member, and reconciling those onto named members is a
   per-function codegen change this round has no reason to risk.
   include/Heap.h itself declares InitializeSolidHeapAsDefault as returning
   `void *`. Memory::Deallocate and data_020a0eac come from include/fBase_c.h,
   which needs them for its inline operator delete. */
extern void *_ZN6Memory8AllocateEjiP4Heap(unsigned int size, int align, void *heap);

}

/* Each list helper keeps its own namespace so the local node types
 * do not collide. The functions stay extern "C" under their addresses. */

// @symbol func_0204405c
namespace Absorb0204405c {
struct N {
    struct N *prev;
    struct N *next;
    int pad8;
    volatile unsigned short c;
};
struct L {
    struct N *head;
    struct N *tail;
};

extern "C" int func_0204405c(void *list, void *nodep)
{
    struct L *l = (struct L *)list;
    struct N *n = (struct N *)nodep;
    struct N *node;
    struct N *next;
    node = l->head;
    if (n == 0)
        return 0;
    if (node == 0)
        return func_0203b244(l, n);
    if (node->c > n->c)
        return func_0203b2ec(l, n, 0);
    goto enter;
advance:
    node = next;
enter:
    next = node->next;
    if (next == 0)
        goto done;
    if (next->c <= n->c)
        goto advance;
done:
    return func_0203b2ec(l, n, node);
}
}

// @symbol func_02043fdc
namespace Absorb02043fdc {
class Obj { public: int dummy; };
typedef void (Obj::*PMF)();

struct Node {
    int pad;        // 0x0
    Node *next;     // 0x4
    Obj *obj;       // 0x8
};

struct Thing {
    Node *head;     // 0x0
    int pad;        // 0x4
    PMF callback;   // 0x8
};

extern "C" void *func_02043fdc(Thing *thiz)
{
    Node *node;
    if (thiz->callback == 0) return (void *)1;
    node = thiz->head;
    while (node != 0) {
        Node *next;
        data_020a4b68 = node;
        next = node->next;
        (node->obj->*thiz->callback)();
        node = next;
    }
    data_020a4b68 = node;
    return (void *)1;
}
}

// @symbol func_02043f98
namespace Absorb02043f98 {
struct Node {
    void* x0;
    struct Node* next;  /* 0x4 */
    int* x8;            /* 0x8 */
};

extern "C" struct Node* func_02043f98(struct Node** head, int key) {
    struct Node* node = *head;
    while (node != 0) {
        int b = (int)(node->x8[1] != key);
        if (b == 0) return node;
        node = node->next;
    }
    return 0;
}
}

// @symbol func_02043f4c
namespace Absorb02043f4c {
struct Inner { char pad[0xc]; unsigned short id; };
struct Node { char pad[4]; struct Node* next; struct Inner* inner; };

extern "C" struct Node* func_02043f4c(struct Node** pp, int key, struct Node* alt) {
    struct Node* n = alt ? alt->next : *pp;
    while (n != 0) {
        int b = (int)(n->inner->id != key);
        if (b == 0) return n;
        n = n->next;
    }
    return 0;
}
}

// @symbol _ZN7fBase_cC2Ev
fBase_c::fBase_c() : manager(this)
{
    uniqueID = data_02099e70;
    data_02099e70++;
    param1 = data_020a4b60;
    actorID = data_020a4b54;
    unk_012 = data_020a4b48;

    func_0203b438(data_020a4b6c, &manager.sceneNode, data_020a4b64);

    fBaseActorInfo *info = data_020a4bb8[actorID];
    manager.SetBehaviorPriority(info->behaviorPriority);
    manager.SetRenderPriority(info->renderPriority);

    fBase_c *parent = (fBase_c *)func_02043810(this);
    if (parent != 0) {
        if ((parent->pauseFlags & 1) || (parent->pauseFlags & 2))
            pauseFlags |= 2;
        if ((parent->pauseFlags & 4) || (parent->pauseFlags & 8))
            pauseFlags |= 8;
    }
}

/* Written D1, D0, D2 so reverse emission lands D2 at 0x02043d48,
 * D0 at 0x02043d78, D1 at 0x02043dbc. Each stores the vptr, then
 * destroys renderNode (0x38) before behaviorNode (0x28). D0 also
 * returns the object to the actor heap. */
// @symbol _ZN7fBase_cD1Ev
extern "C" fBase_c *_ZN7fBase_cD1Ev(fBase_c *self)
{
    *(int *)self = (int)_ZTV7fBase_c;
    _ZN11fLiNdBaPr_cD1Ev(&self->manager.renderNode);
    _ZN11fLiNdBaPr_cD1Ev(&self->manager.behaviorNode);
    return self;
}

// @symbol _ZN7fBase_cD0Ev
extern "C" fBase_c *_ZN7fBase_cD0Ev(fBase_c *self)
{
    *(int *)self = (int)_ZTV7fBase_c;
    _ZN11fLiNdBaPr_cD1Ev(&self->manager.renderNode);
    _ZN11fLiNdBaPr_cD1Ev(&self->manager.behaviorNode);
    _ZN6Memory10DeallocateEPvP4Heap(self, data_020a0eac);
    return self;
}

// @symbol _ZN7fBase_cD2Ev
extern "C" fBase_c *_ZN7fBase_cD2Ev(fBase_c *self)
{
    *(int *)self = (int)_ZTV7fBase_c;
    _ZN11fLiNdBaPr_cD1Ev(&self->manager.renderNode);
    _ZN11fLiNdBaPr_cD1Ev(&self->manager.behaviorNode);
    return self;
}

/* Run the guard, then the action when allowed, and always notify the after hook. */
// @symbol _ZN7fBase_c7ProcessEMS_FivEMS_FbvEMS_FvjE
int fBase_c::Process(ProcessFunction action, BeforeProcessFunction before,
                     AfterProcessFunction after)
{
    int result = (this->*before)();
    u32 status;
    if (result != 0) {
        result = (this->*action)();
        if (result == -1) status = 3;
        else if (result == 1) status = 2;
        else status = 1;
    } else {
        status = 0;
    }
    (this->*after)(status);
    return result;
}

/* vtable slot 0. Default initialization succeeds without loading resources.
 *
 * THE KEY FUNCTION, AND THE REASON THIS ONE IS NOT A METHOD. See the file
 * banner: a real `s32 fBase_c::InitResources()` anywhere makes that TU emit
 * _ZTV7fBase_c and collide with the gap object's copy of the ROM's own
 * vtable. Its DECLARATION in the class is required and harmless -- removing it
 * would delete slot 0 and shift the other seventeen. */
// @symbol _ZN7fBase_c13InitResourcesEv
extern "C" int _ZN7fBase_c13InitResourcesEv(void)
{
    return 1;
}

/* vtable slot 1. Allow initialization by default. */
// @symbol _ZN7fBase_c19BeforeInitResourcesEv
bool fBase_c::BeforeInitResources()
{
    return 1;
}

/* vtable slot 2. Only acts on VS_SUCCESS (2): unlink from the pending list,
 * then either flag the actor as deferred (when the global at data_02099f24
 * reads 3) or link it into both the behaviour and render lists and mark it
 * alive. */
// @symbol _ZN7fBase_c18AfterInitResourcesEj
void fBase_c::AfterInitResources(u32 vfSuccess)
{
    if (vfSuccess != 2)
        return;
    func_0203b27c(data_020a4b88, &manager.behaviorNode);
    volatile int *p = data_02099f24;
    bool b = (p[0] == 3);
    if (b) {
        unk_010 = 1;
        return;
    }
    func_0204405c(data_020a4b78, &manager.behaviorNode);
    func_0204405c(data_020a4b98, &manager.renderNode);
    aliveState = 1;
}

/* vtable slot 3. Default cleanup succeeds without releasing resources. */
// @symbol _ZN7fBase_c16CleanupResourcesEv
s32 fBase_c::CleanupResources()
{
    return 1;
}

/* vtable slot 4. Wait for pending lifecycle work and remaining scene children. */
// @symbol _ZN7fBase_c22BeforeCleanupResourcesEv
int fBase_c::BeforeCleanupResources()
{
    int v = (int)lifecycleState;
    if (v != 0) {
        if (func_0204424c((char *)v) == 0)
            goto ret0;
    }
    if (manager.sceneNode.child == 0)
        goto ret1;
ret0:
    return 0;
ret1:
    return 1;
}

/* On success, unlink the actor, release its heap and pending lifecycle work,
 * then call the virtual complete-object destructor (slot 16) and deallocate.
 * This function is enrolled with the rest of the production TU. */
// @symbol _ZN7fBase_c21AfterCleanupResourcesEj
void fBase_c::AfterCleanupResources(u32 vfSuccess)
{
    if (vfSuccess != 2)
        return;
    func_0203b3c0(data_020a4b6c, &manager.sceneNode);
    func_0203b27c(data_020a4ba8, &manager.behaviorNode);
    if (heap)
        ((Heap *)heap)->_Destroy();
    if (lifecycleState)
        func_02044334(lifecycleState);
    this->~fBase_c();   /* vtable+0x40 = slot 16 = D1 */
    _ZN6Memory10DeallocateEPvP4Heap(this, data_020a0eac);
}

/* vtable slot 6. The default update succeeds without doing any work. */
// @symbol _ZN7fBase_c8BehaviorEv
s32 fBase_c::Behavior()
{
    return 1;
}

/* vtable slot 7. Skips the tick once the actor is marked for death, or when
 * bit 1 of the spawn-flag byte is set. */
// @symbol _ZN7fBase_c14BeforeBehaviorEv
int fBase_c::BeforeBehavior()
{
    if (shouldBeKilled != 0)
        goto ret0;
    if ((pauseFlags & 2) == 0)
        goto ret1;
ret0:
    return 0;
ret1:
    return 1;
}

/* vtable slot 8. Base does nothing; leaf classes override. */
// @symbol _ZN7fBase_c13AfterBehaviorEj
void fBase_c::AfterBehavior(u32 vfSuccess)
{
    u32 unused = vfSuccess;
}

/* vtable slot 9. The default render succeeds without drawing anything. */
// @symbol _ZN7fBase_c6RenderEv
s32 fBase_c::Render()
{
    return 1;
}

/* vtable slot 10. The render twin of BeforeBehavior, on bit 3 instead of 1. */
// @symbol _ZN7fBase_c12BeforeRenderEv
int fBase_c::BeforeRender()
{
    if (shouldBeKilled != 0)
        goto ret0;
    if ((pauseFlags & 8) == 0)
        goto ret1;
ret0:
    return 0;
ret1:
    return 1;
}

/* vtable slot 11. Base does nothing; leaf classes override. */
// @symbol _ZN7fBase_c11AfterRenderEj
void fBase_c::AfterRender(u32 vfSuccess)
{
    u32 unused = vfSuccess;
}

/* vtable slot 12 (vtable+0x30), fired by MarkForDestruction. Base does
 * nothing; leaf classes override to release or notify. */
// @symbol _ZN7fBase_c16OnPendingDestroyEv
void fBase_c::OnPendingDestroy()
{
}

// @symbol func_02043880
/* The scene-graph list maintainer: reconciles one actor's list membership with
 * its state and its parent's flags. Unnamed in config, and it is not an
 * fBase_c member by any evidence -- but it is INSIDE the class's contiguous
 * run, between MarkForDestruction and OnPendingDestroy, and it reads the
 * fBase_c layout directly (state at 0x0e, flags at 0x13, the two list nodes
 * at 0x28 and 0x38), so nothing else can own it.
 *
 * The legacy file reached that layout through three local structs called
 * `Node`, `LNode` and `Obj`. All three names are far too generic to survive in
 * a TU that includes the project headers, so all three are PREFIXED here -- but
 * only renamed. The field accesses are left exactly as they were, offset for
 * offset, because raw-cast versus named-member is a per-FUNCTION codegen choice
 * (a named member folds into `[rN,#imm]`, a cast makes the compiler materialise
 * the address into a scratch register and reuse it) and these 0x240 bytes were
 * recovered against the casts.
 *
 * For the record, since restating a class by hand is exactly what a TU rebuild
 * is supposed to retire, here is what `Obj` maps onto in include/fBase_c.h --
 * so the next pass can try the real members one at a time and measure:
 *     0x0e state  = aliveState        0x12 pad12 = unk_012
 *     0x0f dirty  = shouldBeKilled    0x13 flags = pauseFlags
 *     0x10 f10    = unk_010           0x18 list  = manager.sceneNode.child
 *     0x11 f11    = unk_011
 * and `LNode` is SceneNode seen through its 0x0c next pointer and 0x10
 * owner fields -- so the loop below walks this actor's scene-graph children and
 * marks each one for destruction. */
struct ActorBase_PListNode {
    ActorBase_PListNode *prev;
    ActorBase_PListNode *next;
    void *owner;
    u16 c;
    u16 e;
};

struct ActorBase_ChildLink {
    char pad[0xc];
    ActorBase_ChildLink *next;
    void *obj;
};

struct ActorBase_Raw {
    char pad0[0xe];
    u8 state;                   /* 0x0e */
    u8 dirty;                   /* 0x0f */
    u8 f10;                     /* 0x10 */
    u8 f11;                     /* 0x11 */
    u8 pad12;                   /* 0x12 */
    u8 flags;                   /* 0x13 */
    int pad14;                  /* 0x14 */
    ActorBase_ChildLink *list;  /* 0x18 */
    char pad1c[0xc];
    ActorBase_PListNode n28;    /* 0x28 */
    ActorBase_PListNode n38;    /* 0x38 */
};

#define ACTORBASE_FLAGP(o) ((u8 *)((char *)(o) + 0x13))
#define ACTORBASE_NODEP(o, OFF) ((ActorBase_PListNode *)((char *)(o) + (OFF)))

extern "C" int func_02043880(ActorBase_Raw *o)
{
    if (o->dirty != 0) {
        ActorBase_ChildLink *it;
        int b = 0;
        o->dirty = b;
        if (o->state == 1) b = 1;
        if (b != 0) {
            func_0203b27c(data_020a4b78, &o->n28);
            func_0203b27c(data_020a4b98, &o->n38);
        } else {
            func_0203b27c(data_020a4b88, &o->n28);
        }
        func_0203b20c(data_020a4ba8, &o->n28);
        o->state = 2;
        for (it = o->list; it != 0; it = it->next) {
            ((fBase_c *)it->obj)->MarkForDestruction();
        }
    } else {
        u32 state;
        int b;
        ActorBase_Raw *other = (ActorBase_Raw *)func_02043810(o);
        if (other != 0) {
            {
                u8 f = other->flags;
                int t = f & 1;
                if (t == 0) {
                    t = f & 2;
                    if (t == 0) goto Lelse1;
                }
                {
                    u8 *q = ACTORBASE_FLAGP(o);
                    *q |= 2;
                }
                goto Lafter1;
            Lelse1:
                if (o->flags & 2) {
                    u8 *q = ACTORBASE_FLAGP(o);
                    *q &= ~2;
                }
            Lafter1:;
            }
            {
                u8 f = other->flags;
                int t = f & 4;
                if (t == 0) {
                    t = f & 8;
                    if (t == 0) goto Lelse2;
                }
                {
                    u8 *q = ACTORBASE_FLAGP(o);
                    *q |= 8;
                }
                goto Lafter2;
            Lelse2:
                if (o->flags & 8) {
                    u8 *q = ACTORBASE_FLAGP(o);
                    *q &= ~8;
                }
            Lafter2:;
            }
        }
        state = o->state;
        b = (int)((state == 1) ? (char *)1 : (char *)0);
        if (b != 0) {
            {
                ActorBase_PListNode *n = ACTORBASE_NODEP(o, 0x28);
                int c = (int)((n->e != n->c) ? (char *)1 : (char *)0);
                if (c != 0) {
                    ActorBase_PListNode *m;
                    func_0203b27c(data_020a4b78, &o->n28);
                    m = ACTORBASE_NODEP(o, 0x28);
                    m->c = m->e;
                    func_0204405c(data_020a4b78, m);
                }
            }
            {
                ActorBase_PListNode *n = ACTORBASE_NODEP(o, 0x38);
                int c = (int)((n->e != n->c) ? (char *)1 : (char *)0);
                if (c != 0) {
                    ActorBase_PListNode *m;
                    func_0203b27c(data_020a4b98, &o->n38);
                    m = ACTORBASE_NODEP(o, 0x38);
                    m->c = m->e;
                    func_0204405c(data_020a4b98, m);
                }
            }
        } else {
            int b2 = (int)((state == 2) ? (char *)1 : (char *)0);
            if (b2 == 0) {
                if (o->f11 != 0) {
                    o->f11 = 0;
                    func_0203b244(data_020a4b88, &o->n28);
                } else if (o->f10 != 0) {
                    o->f10 = 0;
                    func_0204405c(data_020a4b78, &o->n28);
                    func_0204405c(data_020a4b98, &o->n38);
                    o->state = 1;
                }
            }
        }
    }
    return 1;
}

/* Non-virtual. Idempotent, and a no-op once the actor is already dying;
 * otherwise sets the flag and fires slot 12. */
// @symbol _ZN7fBase_c18MarkForDestructionEv
void fBase_c::MarkForDestruction()
{
    if (shouldBeKilled != 0)
        return;
    unsigned char b = (aliveState == 2);
    if (b != 0)
        return;
    shouldBeKilled = 1;
    OnPendingDestroy();   /* vtable+0x30 = slot 12 */
}

/* Return the parent scene node's actor, or null at the root. */
// @symbol func_02043810
extern "C" void *func_02043810(void *base)
{
    fBase_c::SceneNode *parent = ((fBase_c *)base)->manager.sceneNode.parent;
    if (parent)
        return parent->owner;
    return 0;
}

/* vtable slot 13 (vtable+0x34). Builds the actor's dedicated heap. Tries the
 * requested size first, then an unbounded solid heap, then a right-sized
 * retry, calling OnHeapCreated (slot 15) inside each attempt and marking the
 * actor for destruction if none of them works.
 *
 * `this->v15()` in the legacy shadow is slot 15, which the real class names
 * OnHeapCreated -- so this now reads as the virtual it is. The heap's flags
 * word (+4) and top (+8) are still reached by raw offset: see the note on the
 * Heap note above. */
// @symbol _ZN7fBase_c9Virtual34Ejj
int fBase_c::Virtual34(u32 a, u32 b)
{
    void *h = 0;
    u32 avail;

    if (heap != 0)
        return 1;

    if (a != 0) {
        h = Heap::InitializeSolidHeapAsDefault(a, (Heap *)b, 0x20);
        if (h != 0) {
            u32 flagA = (*(u32 *)((char *)h + 4)) & 0x10;
            if (flagA != 0)
                _ZN6Memory8AllocateEjiP4Heap(0x10, 4, 0);
            int res = OnHeapCreated();
            u32 okA;
            if (flagA != 0) {
                okA = (u32)res;
            } else {
                void *allocRes = _ZN6Memory8AllocateEjiP4Heap(0x10, 4, 0);
                okA = (allocRes != 0) ? (u32)res : 0;
            }
            Heap::RestoreFromTemporary();
            if (okA == 0) {
                ((Heap *)h)->_Destroy();
                h = 0;
            } else {
                u32 topA = *(u32 *)((char *)h + 8);
                avail = topA - ((Heap *)h)->MaxAllocationUnitSize();
                avail = (avail + 0x1f) & ~0x1f;
                if (a == avail) {
                    ((Heap *)h)->ResizeToFit();
                    heap = h;
                    return 1;
                }
            }
        }
    }

    if (h == 0) {
        h = Heap::InitializeSolidHeapAsDefault((u32)-1, (Heap *)b, 0x20);
        u32 flagB = (*(u32 *)((char *)h + 4)) & 0x10;
        if (flagB != 0)
            _ZN6Memory8AllocateEjiP4Heap(0x10, 4, 0);
        a = (u32)OnHeapCreated();
        if (flagB == 0) {
            void *allocRes2 = _ZN6Memory8AllocateEjiP4Heap(0x10, 4, 0);
            if (allocRes2 == 0)
                a = 0;
        }
        Heap::RestoreFromTemporary();
        if (a == 0) {
            ((Heap *)h)->_Destroy();
            MarkForDestruction();
            return 0;
        }
        u32 topB = *(u32 *)((char *)h + 8);
        avail = topB - ((Heap *)h)->MaxAllocationUnitSize();
        avail = (avail + 0x1f) & ~0x1f;
    }

    if (h == 0)
        goto fail;

    {
        u32 topH = *(u32 *)((char *)h + 8);
        void *h2 = 0;
        u32 availInH = topH - ((Heap *)h)->MaxAllocationUnitSize();
        u32 needed = ((availInH + 0xf) & ~0xf) + 0x30;
        if (needed < ((Heap *)b)->MaxAllocationUnitSize()) {
            h2 = Heap::InitializeSolidHeapAsDefault(avail, (Heap *)b, 0x20);
        }
        if (h2 != 0) {
            if ((u32)h2 < (u32)h) {
                ((Heap *)h)->_Destroy();
                h = 0;
                int res3 = OnHeapCreated();
                u32 okC = (u32)res3;
                Heap::RestoreFromTemporary();
                if (okC == 0) {
                    ((Heap *)h2)->_Destroy();
                    h2 = h;
                }
            } else {
                Heap::RestoreFromTemporary();
                ((Heap *)h2)->_Destroy();
                h2 = 0;
            }
        }
        if (h2 != 0) {
            ((Heap *)h2)->ResizeToFit();
            heap = h2;
            return 1;
        }
    }

    if (h != 0) {
        ((Heap *)h)->ResizeToFit();
        heap = h;
        return 1;
    }

fail:
    MarkForDestruction();
    return 0;
}

/* vtable slot 14 (vtable+0x38). The short form of Virtual34: one attempt, no
 * right-sizing retry.
 *
 * Heap is reached through include/Heap.h's real methods, the same
 * non-virtual direct calls Virtual34 makes. */
// @symbol _ZN7fBase_c9Virtual38Ejj
int fBase_c::Virtual38(u32 a, u32 b)
{
    if (heap != 0)
        return 1;
    if (a != 0) {
        void *h = Heap::InitializeSolidHeapAsDefault(a, (Heap *)b, 0x20);
        if (h != 0) {
            int ok;
            int flag = (*(int *)((char *)h + 4)) & 0x10;
            if (flag != 0)
                _ZN6Memory8AllocateEjiP4Heap(0x10, 4, 0);
            ok = OnHeapCreated();
            if (flag == 0) {
                if (_ZN6Memory8AllocateEjiP4Heap(0x10, 4, 0) == 0)
                    ok = 0;
            }
            ((Heap *)h)->MaxAllocationUnitSize();
            Heap::RestoreFromTemporary();
            if (ok == 0) {
                ((Heap *)h)->_Destroy();
            } else {
                heap = h;
                return 1;
            }
        }
    }
    MarkForDestruction();
    return 0;
}

/* vtable slot 15 (vtable+0x3c), fired by Virtual34/Virtual38 once the actor's
 * heap exists. The default hook accepts the heap. */
// @symbol _ZN7fBase_c13OnHeapCreatedEv
bool fBase_c::OnHeapCreated()
{
    return 1;
}

/* Actor-heap allocation, cleared before construction. The configured ABI takes
 * unsigned; the class's size_t operator-new overload forwards here because
 * the compiler requires size_t for an in-class operator new. */
// @symbol _ZN7fBase_cnwEj
extern "C" void *_ZN7fBase_cnwEj(unsigned int size)
{
    void *p = _ZN6Memory8AllocateEjiP4Heap(size, -4, data_020a0eac);
    if (!p)
        return 0;
    func_0206e2f8(p, 0, size);
    return p;
}

/* ROM ordinal 0 -- the run's backward neighbour, absorbed (reverse
 * emission puts the lowest address last). Body as recovered. */

// @symbol func_020433b8
extern "C" {
void func_020433b8(char* self)
{
    func_0204335c(self);
    if (*(u8*)(self + 0xf) != 0) return;
    if (*(u8*)(self + 0x10) != 0) return;
    {
        volatile u8* q = (u8*)(self + 0xe);
        int b = q[0] == 0 ? 1 : 0;
        if (b == 0) return;
    }
    {
        volatile int* p = data_02099f24;
        int c = p[0] == 2 ? 1 : 0;
        if (c) {
            *(u8*)(self + 0x11) = 1;
            return;
        }
    }
    func_0203b244(data_020a4b88, self + 0x28);
}
}
