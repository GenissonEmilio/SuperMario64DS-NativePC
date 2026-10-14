int func_ov004_020ae5c4(void *a, int b, int c, int d, int e, int f, int g);

#ifdef _MSC_VER
/* The one caller (func_ov006_020d1ba0.cpp) hands over two dScMgAmida_c::Point
   values. The cartridge's compiler passes a class with a copy constructor by
   address, which is the `int *` pair below. MSVC passes it by value, the
   eight bytes inline on the stack, so on the PC build the pointers were the
   pen's x and y themselves and the first read faulted at x + 4 (crash C6/C9,
   Mario's Slides: the line a player draws is classified, then this runs).
   Same body, taking the two points the way the host caller sends them. */
typedef struct { int x, y; } Amida2580Pt;

int func_ov006_020d2580(char *this, Amida2580Pt p1, Amida2580Pt p2) {
    int hi, lo;
    *(unsigned char*)(this + 0x4709) = 0;
    *(unsigned char*)(this + 0x4708) = 1;
    hi = p1.y;
    lo = p1.x;
    *(int*)(this + 0x46f8) = lo;
    *(int*)(this + 0x46fc) = hi;
    func_ov004_020ae5c4(this, p1.x, p1.y, p2.x, p2.y,
                        *(int*)(this + 0x53e4), 1);
    *(unsigned char*)(this + 0x4708) = 0;
    return *(unsigned char*)(this + 0x4709);
}
#else
int func_ov006_020d2580(char *this, int *p1, int *p2) {
    int hi, lo;
    *(unsigned char*)(this + 0x4709) = 0;
    *(unsigned char*)(this + 0x4708) = 1;
    hi = p1[1];
    lo = p1[0];
    *(int*)(this + 0x46f8) = lo;
    *(int*)(this + 0x46fc) = hi;
    func_ov004_020ae5c4(this, p1[0], p1[1], p2[0], p2[1],
                        *(int*)(this + 0x53e4), 1);
    *(unsigned char*)(this + 0x4708) = 0;
    return *(unsigned char*)(this + 0x4709);
}

#endif
