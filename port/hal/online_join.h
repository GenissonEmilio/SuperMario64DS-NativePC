/* ONLINE JOIN (run after093, lane JOIN1): the in-game "put me in a server" handshake with the launcher.

   The launcher stays open behind the game as the helper that talks to the lobby. The game asks it for a
   server through two small files in <save folder>/online/ (the folder the game's own save lives in):

     join_request.json  game -> launcher   {"v":1,"mode":"adventure","file":<0..2>,"nick":"<name>","stamp":<ms>}
     join_plan.json     launcher -> game   {"v":1,"stamp":<same>,"ok":true,"server":<n>,"plan":{...}}
                                        or {"v":1,"stamp":<same>,"ok":false,"why":"<one plain sentence>"}

   <ms> is milliseconds since 1970 (UTC). The plan object carries exactly what the launcher-started game
   gets in its environment today and nothing secret: "relay", "code", "kind" (world), optional "seat",
   optional "name", "color", "shoes". The game polls the plan file every 250 ms for 20 s, ignores a plan
   whose stamp is not its request's, deletes the plan file once read, and applies the plan THE WAY THE
   ENV PATH DOES: it sets the same SM64DS_* names the launcher sets for an Adventure Online start and starts
   the game again at its front door (the successor the launcher's session loop already adopts), which boots
   straight into the chosen file with the hub installed. That restart is now the refusal path: the join is
   made in this process when it can be (SEAMJOIN1, join_in_place in hal/online_join.cpp; the game heap is
   carved at the online size on every boot, hub_game_heap_extra), and restarts only when it cannot.

   Usage from a screen: online_join_begin(file, nick), then online_join_step() once per frame until it
   returns something other than kJoinWaiting. kJoinEntering means the successor has been started and this
   process is quitting: draw nothing new, return to the caller's quit path. kJoinFailed: show
   online_join_sentence(). online_join_cancel() withdraws a wait. */
#ifndef PORT_HAL_ONLINE_JOIN_H
#define PORT_HAL_ONLINE_JOIN_H

namespace port {

enum OnlineJoinState {
    kJoinIdle = 0,     /* nothing asked */
    kJoinWaiting,      /* request written, waiting for the launcher's answer */
    kJoinEntering,     /* plan applied, successor started, this process is quitting */
    kJoinFailed,       /* no server: online_join_sentence() says why */
    kJoinCancelled     /* online_join_cancel() was called */
};

/* Write the request. file 0..2, nick any text (cleaned to printable ASCII, 1..16 chars, a fallback name when empty).
   Returns false (and kJoinFailed with a sentence) when the folder cannot be written. */
bool online_join_begin(int file, const char *nick);
OnlineJoinState online_join_step();
void online_join_cancel();
OnlineJoinState online_join_state();
/* the plain sentence for kJoinFailed ("" otherwise) */
const char *online_join_sentence();
/* seconds left of the 20 s wait while kJoinWaiting, else 0 */
int online_join_seconds_left();
/* JOINFLOW1: remove every name a join sets from this process (a refused restart, or a cancel on the Joining plate) */
void online_join_clear_env();

}  // namespace port

#endif
