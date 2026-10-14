/* hal/portsync3_runtime.cpp -- the few runtime symbols the main sync made the
 * port define by hand. Includes nothing.
 *
 * __register_global_object: mwccarm's C++ runtime call that records a
 * function-local static's destructor so the DS runtime can run it at exit.
 * Main's TUs spell the call out (src/actors/daDoor_c.cpp, daTrs_c.cpp,
 * dMg3DHeyhoObjAdapter_c.cpp). The DS game never exits and the host never
 * runs these destructors (the objects are file-scope data the savestate span
 * owns), so the registration has nothing to do. MSVC's own thread-safe-static
 * machinery is not involved: the TUs construct the object by hand and only
 * call this to register it.
 */
extern "C" void __register_global_object(void *object, void *destructor, void *node)
{
    (void)object;
    (void)destructor;
    (void)node;
}
