/* Runtime asset handles resolved through the overlay 0 file table. */
enum sinit_ov098_0213bf9c_AssetId {
    ASSET_YAJIRUSI_L_BMD = 0x0497,
    ASSET_YAJIRUSI_L_KCL = 0x0498,
    ASSET_YAJIRUSI_R_BMD = 0x0499,
    ASSET_YAJIRUSI_R_KCL = 0x049a
};

extern void func_02017acc();
extern void func_020731dc();
extern void func_02017b4c();
extern int gYajirusiLModelFile[];
extern int func_02017ab4[];
extern int data_ov098_0213c820[];
extern int gYajirusiRModelFile[];
extern int data_ov098_0213c844[];
extern int gYajirusiLCollisionFile[];
extern int SharedFilePtr_Destruct_Clsn[];
extern int data_ov098_0213c838[];
extern int gYajirusiRCollisionFile[];
extern int data_ov098_0213c82c[];
void __sinit_ov098_0213bf9c(void)
{
    func_02017acc(gYajirusiLModelFile, ASSET_YAJIRUSI_L_BMD);
    func_020731dc(gYajirusiLModelFile, func_02017ab4, data_ov098_0213c820);
    func_02017acc(gYajirusiRModelFile, ASSET_YAJIRUSI_R_BMD);
    func_020731dc(gYajirusiRModelFile, func_02017ab4, data_ov098_0213c844);
    func_02017b4c(gYajirusiLCollisionFile, ASSET_YAJIRUSI_L_KCL);
    func_020731dc(gYajirusiLCollisionFile, SharedFilePtr_Destruct_Clsn, data_ov098_0213c838);
    func_02017b4c(gYajirusiRCollisionFile, ASSET_YAJIRUSI_R_KCL);
    func_020731dc(gYajirusiRCollisionFile, SharedFilePtr_Destruct_Clsn, data_ov098_0213c82c);
}
