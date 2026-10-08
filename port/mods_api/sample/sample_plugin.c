/* sample_plugin.c -- a small, complete plugin for the SM64DS PC port (plugin
   API v1, see ../sm64ds_mod.h). It
     * logs when it loads and reads one player setting ("tint"),
     * counts frames and logs the count now and then,
     * logs every level load,
     * edits Mario's model (or the BMD in the "file" setting) as the game loads it (his first palette becomes one
       colour, so the change is easy to see),
     * wraps one actor's spawn function (the coin, or the id in the "actor" setting) with one that logs the first spawns and
       then calls the original.
   Build: see README.md. Not a template for anything clever, a template for the
   shape. */
#include <stdlib.h>
#include <string.h>

#include "../sm64ds_mod.h"

static const SM64DSModApi *g_api;
static unsigned g_frames;
static unsigned g_levels;
static unsigned g_coins;
static unsigned g_actor = 288;                      /* 288 = COIN */
static unsigned short g_tint = 0x03E0;              /* BGR555 green */
static SM64DSActorFactory g_prev_coin;

#ifdef SAMPLE_TEST_FAULT_FRAME
/* build option for testing the game's guard: crash on that frame */
#define TEST_FAULT_FRAME SAMPLE_TEST_FAULT_FRAME
#endif

static void __cdecl on_frame(void *user)
{
    (void)user;
    ++g_frames;
    if (g_frames == 1 || g_frames % 60 == 0)
        g_api->log(g_api, "frame count %u", g_frames);
#ifdef TEST_FAULT_FRAME
    if (g_frames == TEST_FAULT_FRAME)
        *(volatile int *)0 = 1;
#endif
}

static void __cdecl on_level(int level_id, void *user)
{
    (void)user;
    ++g_levels;
    g_api->log(g_api, "level %d loaded (load #%u, after %u frames)", level_id, g_levels, g_frames);
}

/* A BMD model (Mario's by default): header word at 0x1c is the palette count, 0x20 the offset of
   the palette records (name offset, data offset, byte size; 16 bytes each).
   Colours are 16-bit BGR555. Edit in place, return the same size. */
static uint32_t __cdecl on_mario(const char *rom_path, uint8_t **data, uint32_t size, void *user)
{
    uint8_t *d = *data;
    uint32_t npal, palo, dp, sz, i;
    (void)user;
    if (size < 0x30) return size;
    memcpy(&npal, d + 0x1c, 4);
    memcpy(&palo, d + 0x20, 4);
    if (npal < 1 || palo + 16 > size) return size;
    memcpy(&dp, d + palo + 4, 4);
    memcpy(&sz, d + palo + 8, 4);
    if (dp + sz > size) return size;
    for (i = 0; i + 1 < sz; i += 2)
        memcpy(d + dp + i, &g_tint, 2);
    g_api->log(g_api, "edited %s: first palette (%u colours) set to 0x%04x", rom_path, sz / 2, (unsigned)g_tint);
    return size;
}

static void *__cdecl coin_wrapper(void)
{
    ++g_coins;
    if (g_coins <= 3)
        g_api->log(g_api, "actor %u is being spawned (#%u)", g_actor, g_coins);
    return g_prev_coin();
}

SM64DS_MOD_EXPORT int __cdecl sm64ds_mod_main(const SM64DSModApi *api)
{
    const char *tint;
    if (api->api_version != SM64DS_MOD_API_VERSION || !SM64DS_MOD_API_HAS(api, free))
        return 1;
    g_api = api;
    api->log(api, "hello from %s, game %s, folder %s", api->mod_id, api->game_release, api->mod_folder);

    tint = api->setting(api, "tint");
    if (tint) {
        g_tint = (unsigned short)strtoul(tint, 0, 0);
        api->log(api, "setting tint = %s", tint);
    } else {
        api->log(api, "setting tint is not set, using green");
    }

    api->on_frame(api, on_frame, 0);
    api->on_level_load(api, on_level, 0);
    tint = api->setting(api, "file");                            /* optional: edit another model's palette */
    api->on_file(api, tint ? tint : "data/player/mario_model.bmd", on_mario, 0);
    tint = api->setting(api, "actor");                           /* optional: wrap another actor id */
    if (tint) g_actor = (unsigned)strtoul(tint, 0, 0);
    g_prev_coin = api->replace_actor(api, g_actor, coin_wrapper);
    if (!g_prev_coin) {
        api->log(api, "could not wrap the spawn of actor %u", g_actor);
        return 2;
    }
    api->log(api, "symbol _port_level_id = %p, no_such_symbol = %p", api->symbol(api, "port_level_id"),
             api->symbol(api, "no_such_symbol"));
    return 0;
}
