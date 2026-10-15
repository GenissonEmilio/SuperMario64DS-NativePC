/* THREE ROM BODIES THAT ARE IN THE BINARY UNDER THE WRONG SPELLING.
 * Run link100, lane HOSTGEN4.
 *
 * THE SHAPE. src/CountRubbedTiles.c, src/UpdateRubbing.c and
 * src/func_ov074_02121800.c each carry the decomp's `//cpp` marker, so
 * port/CMakeLists.txt compiles them with PROPERTIES LANGUAGE CXX -- lines 7650,
 * 13304 and their siblings -- and each is on a slice already (slice_boo.txt
 * line 426, slice_ov074.txt line 232). Compiled as C++ with no extern "C", each
 * emits the MSVC-decorated name:
 *
 *     ?CountRubbedTiles@@YAXPADH@Z
 *     ?UpdateRubbing@@YAXH@Z
 *     ?func_ov074_02121800@@YAXPAD@Z
 *
 * while their callers are plain .c files that ask for the flat ROM name. So all
 * three bodies have been IN the binary and all three names have been on the
 * unresolved wall at the same time, which is not a shape a census of defined
 * symbols can find: the body is defined, under the other spelling.
 *
 * THE FIRST ATTEMPT WAS A SECOND SLICE ROW and it was wrong. Naming the same
 * source again compiles the same object twice and the link says so plainly:
 *
 *     CountRubbedTiles.c.obj : error LNK2005:
 *         "void __cdecl CountRubbedTiles(char *,int)"
 *         (?CountRubbedTiles@@YAXPADH@Z) already defined in
 *         CountRubbedTiles.c.obj
 *
 * WHY A BRIDGE IS ADMISSIBLE HERE, clause by clause, which is the rule this
 * tree applies to every /alternatename and the one the fastcall-face note says
 * to check rather than assume. Both spellings of each name are __cdecl with NO
 * receiver: the decorated one is YAX..., a free function, not a member. The
 * caller cleans the stack on both sides. And the arity matches at every site,
 * read off the declarations rather than the mangle:
 *
 *   CountRubbedTiles  definition src/CountRubbedTiles.c:7  (char *, int)
 *                        caller     src/Boo_State3.c:2  (char *, int)
 *   UpdateRubbing  definition src/UpdateRubbing.c:11 (int)
 *                        callers    src/Scene_State3.c:3  (char *)
 *                                   src/Scene_State2.c:3  (char *)
 *   func_ov074_02121800  definition src/func_ov074_02121800.c:7  (char *)
 *                        caller     src/_ZN8Goomboss8BehaviorEv.cpp:111 (char *)
 *
 * One argument each way for the middle one, spelled `int` on the definition and
 * `char *` at both call sites: one four-byte stack slot either way, which is
 * the whole of what the ABI cares about at a cdecl call.
 *
 * THE BETTER END, for whoever wants it: take the three sources through hostgen
 * instead, which wraps a .c source in extern "C" and emits the flat name
 * directly, and retire this file. That was not done here because the decorated
 * spelling is what is in the binary today and nothing has established whether
 * anything else calls it that way; a name bridge changes no code and cannot be
 * wrong about a caller it has not seen.
 */
#pragma comment(linker, "/alternatename:__ZN13dScMgTeresa_c16CountRubbedTilesEi=?_ZN13dScMgTeresa_c16CountRubbedTilesEi@@YAXPADH@Z")
#pragma comment(linker, "/alternatename:__ZN13dScMgTeresa_c13UpdateRubbingEv=?_ZN13dScMgTeresa_c13UpdateRubbingEv@@YAXH@Z")
#pragma comment(linker, "/alternatename:__ZN12daKuriKing_c19func_ov074_02121800Ev=?_ZN12daKuriKing_c19func_ov074_02121800Ev@@YAXPAD@Z")

/* A linker directive lives in a .drectve section, which an object only has if
   it has something to compile. */
extern "C" int port_hostgen4_cxxname_bridges(void) { return 3; }
