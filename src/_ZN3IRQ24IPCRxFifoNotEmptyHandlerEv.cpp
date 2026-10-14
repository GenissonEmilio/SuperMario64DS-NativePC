//cpp
// @symbol _ZN3IRQ24IPCRxFifoNotEmptyHandlerEv
/* IRQ::IPCRxFifoNotEmptyHandler -- the PXI recv-fifo IRQ handler, arm9
 * 0x0205b864..0x0205b988. func_0205bad8's PXI init clears the callback
 * table and registers this on IRQ 0x40000; func_0205ba64 registers one
 * callback per channel. On a fifo error the handler clears it and retries;
 * an empty read returns; a command word dispatches through
 * data_020a7fc8[channel], and an unhandled non-reply command is echoed
 * back to the ARM7 with its reply bit set.
 *
 * leftovers: none.
 */
#include "types.h"

namespace IRQ {

u32 Disable(void);
void Restore(u32 saved);

}

extern "C" {
/* Channel callbacks, 32 entries; written by func_0205ba64. */
extern void (*data_020a7fc8[])(u32 idx, u32 data, u32 flag);
}

typedef union {
    u32 raw;
    struct {
        u32 idx  : 5;   /* callback table slot; 0 is unused */
        u32 flag : 1;   /* set on reply words */
        u32 data : 26;
    } f;
} IpcCmd;

#define FIFOCNT  (*(volatile u16 *)0x4000184)
#define FIFORECV (*(volatile u32 *)0x4100000)
#define FIFOSEND (*(volatile u32 *)0x4000188)

#define FIFOCNT_ERROR      0x4000  /* set on a recv-empty read or send-full write */
#define FIFOCNT_ERROR_CLEAR 0xc000 /* clears the error and flushes the send fifo */
#define FIFOCNT_RECV_EMPTY 0x0100
#define FIFOCNT_SEND_FULL  0x0002

namespace IRQ {

void IPCRxFifoNotEmptyHandler(void)
{
    IpcCmd cmd;
    int code;
    u32 saved;
    for (;;) {
        if (FIFOCNT & FIFOCNT_ERROR) {
            FIFOCNT = FIFOCNT | FIFOCNT_ERROR_CLEAR;
            code = -3;
        } else {
            saved = Disable();
            if (FIFOCNT & FIFOCNT_RECV_EMPTY) {
                Restore(saved);
                code = -4;
            } else {
                cmd.raw = FIFORECV;
                Restore(saved);
                code = 0;
            }
        }
        if (code == -4)
            return;
        if (code == -3)
            continue;
        {
            void (*fn)(u32, u32, u32);
            if (cmd.f.idx == 0)
                continue;
            fn = data_020a7fc8[cmd.f.idx];
            if (fn != 0) {
                fn(cmd.f.idx, cmd.f.data, cmd.f.flag);
                continue;
            }
            if (cmd.f.flag)
                continue;
            cmd.raw = cmd.raw | 0x20;
            if (FIFOCNT & FIFOCNT_ERROR) {
                FIFOCNT = FIFOCNT | FIFOCNT_ERROR_CLEAR;
                continue;
            }
            saved = Disable();
            if (FIFOCNT & FIFOCNT_SEND_FULL) {
                Restore(saved);
                continue;
            }
            FIFOSEND = cmd.raw;
            Restore(saved);
        }
    }
}

}
