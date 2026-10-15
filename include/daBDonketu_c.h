#ifndef DABDONKETU_C_H
#define DABDONKETU_C_H

#include "types.h"
#include "daOts_c.h"

/* daBDonketu_c in the ROM's RTTI, and daDonketu_c's sibling rather than its subclass: both
 * derive from daOts_c, which owns all five of the members this header used to
 * restate. daBDonketu_c_classInit is byte-for-byte daDonketu_c_classInit apart from the vtable it
 * stores last.
 *
 * SIZE 0x400, the literal in daBDonketu_c_classInit's fBase_c::operator new. The base ends
 * at 0x398.
 *
 * SM64DS RTTI names the implementation daBDonketu_c. The reconstructed
 * factory daBDonketu_c_classInit (historical alias
 * BigBully_Spawn) constructs it for the BOSS_DONKETU
 * registry profile.
 */
struct daBDonketu_c : daOts_c {
    u8  pad_398[0x62];
    u16 mSecretSoundCounter;        /* 0x3fa -- Sound::PlaySecretSound advances it */
    u8  mStarID;                    /* 0x3fc */
    u8  mTrackStarID;                /* 0x3fd -- dActor_c::TrackStar result */
    u8  mNumBulliesKilled;          /* 0x3fe -- daDonketu_c increments it (found via mBigBullyID); 0xff skips the small-Bully phase */
    u8  pad_3ff[0x1];

    #ifdef _MSC_VER
    /* host two-arm destructor spelling; ruling in include/ModelBase.h */
    virtual void Destructor1();   /* D1 */
    virtual void Destructor0();   /* D0 */
    ~daBDonketu_c() {}   /* no slot */
    #else
    virtual ~daBDonketu_c() {}
    #endif

    /* methods */
    int Behavior();
    int InitResources();
    int Render();
    virtual int UpdateRunState();
    virtual void UpdateDeathState();
};

#ifndef SM64DS_PLATFORM_PC
/* ROM layout under mwccarm; host ABI divergence is tracked separately. */
typedef char daBDonketu_c_size_must_be_0x400[sizeof(daBDonketu_c) == 0x400 ? 1 : -1];
#endif

#endif /* DABDONKETU_C_H */
