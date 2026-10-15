/* Runtime asset handles resolved through the overlay 0 file table. */
enum sinit_ov023_02111aa8_AssetId {
    ASSET_FM_BATTAN_BMD = 0x0616,
    ASSET_FM_BATTAN_KCL = 0x0617
};

extern void func_02017acc();
extern void func_020731dc();
extern void func_02017b4c();
extern int data_ov023_02112088[];
extern int func_02017ab4[];
extern int data_ov023_0211209c[];
extern int data_ov023_02112080[];
extern int SharedFilePtr_Destruct_Clsn[];
extern int data_ov023_02112090[];
void __sinit_ov023_02111aa8(void)
{
    func_02017acc(data_ov023_02112088, ASSET_FM_BATTAN_BMD);
    func_020731dc(data_ov023_02112088, func_02017ab4, data_ov023_0211209c);
    func_02017b4c(data_ov023_02112080, ASSET_FM_BATTAN_KCL);
    func_020731dc(data_ov023_02112080, SharedFilePtr_Destruct_Clsn, data_ov023_02112090);
}
