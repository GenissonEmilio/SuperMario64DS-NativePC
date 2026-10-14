//cpp
// @symbol _ZN3IRQ21GameCardIREQMCHandlerEv
/* IRQ::GameCardIREQMCHandler -- the game-card IRQ handler, arm9
 * 0x020610ac..0x020610fc. Sets the data_020a89a0 flag, runs the callback
 * func_02061128 registers in data_020a89a4, and drops into func_020610fc's
 * IPC send-and-hang terminal path when it returns nonzero.
 *
 * leftovers: none.
 */
typedef int (*CardCallback)(void);
extern "C" CardCallback data_020a89a4;
extern "C" int data_020a89a0;
extern "C" int func_020610fc(void);

namespace IRQ {

void GameCardIREQMCHandler()
{
    CardCallback callback = data_020a89a4;
    int result = 1;
    data_020a89a0 = result;
    if (callback) result = callback();
    if (result == 0) return;
    func_020610fc();
}

}
