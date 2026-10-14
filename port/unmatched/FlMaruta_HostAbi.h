/* PORT_HOST_ABI (PORTSYNC3). Force-included ahead of src/game/actors/
 * d_a_obj_fl_maruta.cpp. The TU declares `void func_ov022_02112654(char *c);`
 * with C++ linkage and then DEFINES it inside `extern "C"`; mwccarm accepts the
 * pair, MSVC reports C2733 ("cannot overload a function with extern C linkage").
 * Declaring the name extern "C" first makes the TU's own plain redeclaration
 * inherit C linkage, which is the one function the ROM has at that address.
 * Nothing in src/ or include/ changes. */
#ifdef __cplusplus
extern "C" void func_ov022_02112654(char *c);
#endif
