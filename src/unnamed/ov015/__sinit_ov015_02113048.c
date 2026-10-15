/* Runtime asset handles resolved through the overlay 0 file table. */
enum sinit_ov015_02113048_AssetId {
    ASSET_BK_DOSSUNBAR_L_BMD = 0x058b,
    ASSET_BK_DOSSUNBAR_L_KCL = 0x058c,
    ASSET_BK_DOSSUNBAR_S_BMD = 0x058d,
    ASSET_BK_DOSSUNBAR_S_KCL = 0x058e
};

extern void func_02017acc();
extern void func_020731dc();
extern void func_02017b4c();
extern void func_02017ab4();
extern void SharedFilePtr_Destruct_Clsn();

typedef struct { int a, b; } S8;

extern S8 gBkDossunbarSModelFile, gBkDossunbarSCollisionFile, gBkDossunbarLModelFile, gBkDossunbarLCollisionFile;
extern S8 data_ov015_021149c8, data_ov015_021149d4, data_ov015_021149e0, data_ov015_021149bc;

extern S8 data_ov015_02114a24[7];
extern S8 data_ov015_021144b0, data_ov015_021144f8, data_ov015_021144c8, data_ov015_021144c0, data_ov015_021144a8, data_ov015_021144b8, data_ov015_021144a0;

extern S8 data_ov015_021149ec[7];
extern S8 data_ov015_02114500, data_ov015_021144f0, data_ov015_021144e0, data_ov015_021144d0, data_ov015_02114508, data_ov015_021144d8, data_ov015_021144e8;

void __sinit_ov015_02113048(void)
{
    func_02017acc(&gBkDossunbarSModelFile, ASSET_BK_DOSSUNBAR_S_BMD);
    func_020731dc(&gBkDossunbarSModelFile, func_02017ab4, &data_ov015_021149c8);
    func_02017b4c(&gBkDossunbarSCollisionFile, ASSET_BK_DOSSUNBAR_S_KCL);
    func_020731dc(&gBkDossunbarSCollisionFile, SharedFilePtr_Destruct_Clsn, &data_ov015_021149d4);
    func_02017acc(&gBkDossunbarLModelFile, ASSET_BK_DOSSUNBAR_L_BMD);
    func_020731dc(&gBkDossunbarLModelFile, func_02017ab4, &data_ov015_021149e0);
    func_02017b4c(&gBkDossunbarLCollisionFile, ASSET_BK_DOSSUNBAR_L_KCL);
    func_020731dc(&gBkDossunbarLCollisionFile, SharedFilePtr_Destruct_Clsn, &data_ov015_021149bc);

    data_ov015_02114a24[0] = data_ov015_021144b0;
    data_ov015_02114a24[1] = data_ov015_021144f8;
    data_ov015_02114a24[2] = data_ov015_021144c8;
    data_ov015_02114a24[3] = data_ov015_021144c0;
    data_ov015_02114a24[4] = data_ov015_021144a8;
    data_ov015_02114a24[5] = data_ov015_021144b8;
    data_ov015_02114a24[6] = data_ov015_021144a0;

    data_ov015_021149ec[0] = data_ov015_02114500;
    data_ov015_021149ec[1] = data_ov015_021144f0;
    data_ov015_021149ec[2] = data_ov015_021144e0;
    data_ov015_021149ec[3] = data_ov015_021144d0;
    data_ov015_021149ec[4] = data_ov015_02114508;
    data_ov015_021149ec[5] = data_ov015_021144d8;
    data_ov015_021149ec[6] = data_ov015_021144e8;
}
