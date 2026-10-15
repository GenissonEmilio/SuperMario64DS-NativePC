/* Runtime asset handles resolved through the overlay 0 file table. */
enum sinit_ov006_0212fc7c_AssetId {
    ASSET_RINGA_BMD = 0x021a,
    ASSET_RINGB_BMD = 0x021b,
    ASSET_RINGC_BMD = 0x021c
};

extern void __cxa_vec_ctor(void* a, int b, int c, void* d, void* e);
extern void func_020731dc(void *object, void* b, void* node);
extern int func_02017acc(void* a, int b);
extern int TrmpLine_Destroy(void);
extern int TrmpLine_Reset(void);
extern int TrmpLine_CleanupAll(void);
extern int func_02017ab4(void);
extern int data_ov006_02140990[];
extern int data_ov006_021408cc[];
extern int data_ov006_02140850[];
extern int data_ov006_0214092c[];
extern int data_ov006_02140858[];
extern int data_ov006_02140944[];
extern int data_ov006_02140848[];
extern int data_ov006_02140950[];

void __sinit_ov006_0212fc7c(void){
  __cxa_vec_ctor(data_ov006_02140990, 4, 0x32c, (void*)TrmpLine_Reset, (void*)TrmpLine_Destroy);
  func_020731dc(0, (void*)TrmpLine_CleanupAll, data_ov006_021408cc);
  func_02017acc(data_ov006_02140850, ASSET_RINGA_BMD);
  func_020731dc(data_ov006_02140850, (void*)func_02017ab4, data_ov006_0214092c);
  func_02017acc(data_ov006_02140858, ASSET_RINGB_BMD);
  func_020731dc(data_ov006_02140858, (void*)func_02017ab4, data_ov006_02140944);
  func_02017acc(data_ov006_02140848, ASSET_RINGC_BMD);
  func_020731dc(data_ov006_02140848, (void*)func_02017ab4, data_ov006_02140950);
}
