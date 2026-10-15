extern int sMgCoins;
extern void Coin_Destroy(void);
extern void func_ov004_020b67e4(void);
extern int data_ov004_020bfa28;
extern void MgCoins_Cleanup(void);
extern void __cxa_vec_ctor(void* a, int b, int c, void* d, void* e);
extern void func_020731dc(void *object, void* b, void* c);
void __sinit_ov004_020b9ad0(void) {
    __cxa_vec_ctor(&sMgCoins, 0x14, 0x24, (void*)func_ov004_020b67e4, (void*)Coin_Destroy);
    func_020731dc(0, (void*)MgCoins_Cleanup, &data_ov004_020bfa28);
}
