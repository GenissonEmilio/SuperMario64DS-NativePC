// IN-GAME TEXT CHAT. Run mp1, lane CHATUI1.
//
// One chat for both kinds of online play: a VS match (the lockstep carrier,
// hal/comms_loopback.cpp, through its aux kinds) and an adventure room (the
// hub, hal/comms_hub.cpp). The wire is design.txt 6.3, 'SYNT' v1:
//
//   0 tag 'SYNT' | 4 version 1 | 5 seat (stamped by the relay / carrier,
//   never trusted from the sender) | 6 flags 0 | 7 text_len 1..120 |
//   8 msg_id u16 | 10 reserved u16 0 | 12 text, text_len bytes of UTF-8
//
// (flags bit0 marks the 12-byte typing frame of the 6.3 addendum, described
// with the bubble below; any other flag bit is dropped and counted.)
//
// Reliability without a stream: every message goes out three times (0, 150
// and 500 ms after the first copy leaves) and a receiver shows it once,
// deduplicated on (seat, msg_id) over the last 64 ids it accepted. A sender
// may send one message every 1.5 s with a burst of 3; a receiver takes at most
// one message a second from any one seat, with the same burst of 3, so an
// honest sender is never cut and a flood is. The relay's 8 packets a second
// per source is the backstop in front of both.
//
// THE TEXT IS OPAQUE. It is never interpreted, and it is not written to the
// log unless a test knob or SM64DS_CHAT_LOG=1 asks for it (the rx line names
// the seat, the id and the size, nothing else). For the screen it is cleaned
// here, once, into printable ASCII the game font can draw: control characters
// (C0, DEL, C1, the Unicode line/paragraph separators, zero-width and
// direction-override marks, the BOM) are stripped; any other character the
// font has no glyph for, and every byte of invalid UTF-8, is drawn as '?'.
//
// WHAT THE PLAYER DOES. T opens a one-line box (only while a chat carrier is
// up; solo, T is an ordinary key and nothing here runs), Enter sends, Esc
// cancels, Backspace deletes. While the box is open the game's own keyboard
// and pad input read released LOCALLY: in VS the console keeps sending its
// pad record every round, with no buttons held, so the lockstep never waits
// on a player who is typing. The last 6 messages show for 10 s each, the
// sender's name in the sender's colour.
//
// TEST KNOBS (all off by default; any of the first four also turns on
// SM64DS_CHAT_LOG). "frame" is the Nth call of chat_tick(), which the window
// makes once per presented frame on both frame loops (counted from the first
// frame a VS transport or the hub is installed). A scripted action whose frame
// passes while the carrier is not live yet fires on the first live frame.
//   SM64DS_CHAT_TEST=<frame>:<text>[;<frame>:<text>...]
//       send <text> at <frame> through the ordinary send path (limiter and
//       all). \xHH in <text> is one raw byte, so a console can send invalid
//       UTF-8 and control characters on purpose; \\ is a backslash.
//   SM64DS_CHAT_TYPE_TEST=<frame>:<text>
//       drive the BOX instead: open it at <frame>, type one character a
//       frame, press Enter after the last one (the typing path end to end).
//   SM64DS_CHAT_FLOOD=<frame>:<n>
//       send n distinct messages at <frame>, skipping the sender's limiter
//       (receivers' and the relay's caps are what is under test).
//   SM64DS_CHAT_SELFTEST=1
//       run the text cleaner's unit cases once and print one verdict line.
//   SM64DS_CHAT_DROP=<0..90>
//       drop that percentage of received chat datagrams before dedupe. On
//       the VS carrier SM64DS_COMMS_DROP is used when this is unset (the
//       ladder's loss rung sets only that one); the hub has its own
//       SM64DS_HUB_LOSS_PCT and is not dropped twice.
//   SM64DS_CHAT_LOG=1
//       add the cleaned text to every rx / tx log line.
//   SM64DS_CHAT_TYPE_HOLD=<frames>
//       with SM64DS_CHAT_TYPE_TEST: keep the box open that many frames after
//       the last character before the Enter (a very large value keeps it
//       open until the console exits or is killed).
//   SM64DS_CHAT_TYPE_ESC=1
//       with SM64DS_CHAT_TYPE_TEST: cancel with Esc instead of sending.
//   SM64DS_CHAT_TYPING_FLOOD=<frame>:<n>
//       send n typing frames at <frame> straight to the carrier, then four
//       malformed ones (a flag bit that does not exist, a 13-byte typing
//       frame, text_len set on a typing frame, state 7); receivers' caps and
//       checks are what is under test.
//
// LOG LINES: "[chat] on: carrier=<vs|hub> seat=<n>" once, "[chat] tx id=<n>
// bytes=<n>" per message sent, "[chat] rx seat=<n> id=<n> bytes=<n>" per
// message shown (after dedupe and the caps), and one "[chat] report ..." line
// with every counter at exit. The bubble adds "[chat] typing tx state=<0|1>
// t=<ms>" per typing frame sent, "[chat] typing seat=<n> on|off (<why>)
// t=<ms>" when a seat's dots start or stop, and "[chat] bubble seat=<n>
// bytes=<n> t=<ms>" / "[chat] bubble seat=<n> gone t=<ms>" (t is the
// machine's QPC clock in ms, the same across processes on one PC).
//
// PORT_HOST_ABI: host plumbing like the carriers. Nothing in src/ knows it
// exists, it reads no game state beyond the VS slot name, and it writes none.

#ifndef PORT_HAL_ADV_CHAT_H
#define PORT_HAL_ADV_CHAT_H

namespace port {

enum : int {
    kChatTextMax   = 120,   // UTF-8 bytes on the wire
    kChatShowLines = 6,
    kChatShowMs    = 10000,
    kChatNameMax   = 24,
};

// Once per presented frame, from both frame loops. Sends what is due,
// receives and cleans what arrived, ages the overlay. Returns at once when no
// chat carrier is up (solo).
void chat_tick();

// A carrier that can carry chat is installed and live.
bool chat_available();

// The box is open: game input reads released locally.
bool chat_typing();

// Open the box. False (and nothing happens) when chat is not available.
bool chat_open();

// Keys while the box is open: VK_RETURN sends, VK_ESCAPE cancels, VK_BACK
// deletes one character. Anything else is ignored (characters arrive through
// chat_char_ansi).
void chat_key(int vk);

// One WM_CHAR from the (ANSI) game window: a byte in the system code page,
// converted here to UTF-8. Control characters are ignored; a character that
// would take the line past kChatTextMax bytes is refused.
void chat_char_ansi(unsigned char b);

// What the overlay draws, oldest line first. Every string is printable ASCII.
struct ChatViewLine {
    char     name[kChatNameMax + 1];
    char     text[kChatTextMax + 1];
    unsigned rgb;          // 0xFFRRGGBB, the sender's colour
    int      alpha;        // 0..255, fades out over the last second
};
struct ChatView {
    int          n;
    ChatViewLine line[kChatShowLines];
    bool         open;
    char         box[kChatTextMax + 1];   // the typed line, cleaned
    int          box_bytes;               // its size on the wire
    bool         caret;                   // blink phase
};
void chat_view(ChatView *out);

// THE BUBBLE OVER A HEAD (run mp1, lane BUBBLE1). Per seat: the newest line
// that seat sent, for kChatBubbleMs (fading over the last second; a newer line
// replaces it), and whether that seat has its box open. The typing state
// travels as a TYPING FRAME (design.txt 6.3 addendum): a 'SYNT' with flags
// bit0 set, exactly 12 bytes, text_len 0, bytes 8..9 a typing sequence (never
// a chat msg_id), byte 10 the state (1 box open, 0 box closed), byte 11 0.
// The sender sends state 1 when the box opens and every second while it stays
// open, and state 0 twice (0 and 150 ms) when it closes without a message; a
// sent message ends the typing by itself. A receiver shows the dots until the
// state 0, a line from that seat, 3 s without a refresh, or the seat leaving.
// Typing frames have their own receive cap (4 a second per seat) so they
// never spend the one real messages are taken from. The window's name-tag pass
// (tests/nametag.h) draws the bubble; this file owns the state.
enum : int {
    kChatBubbleMs  = 5000,
    kChatTypingMs  = 3000,
};
struct ChatBubble {
    bool     typing;        // the dots
    int      dots;          // 1..3, cycling every 0.4 s
    bool     has_text;
    char     text[kChatTextMax + 1];   // printable ASCII, the log's cleaning
    int      alpha;         // 0..255 for the text, fades out over the last 1 s
    unsigned rgb;           // 0xFFRRGGBB, the sender's colour
};
// False when that seat has nothing over its head. seat is the carrier's seat:
// the VS slot, or the hub seat in an adventure room.
bool chat_bubble(int seat, ChatBubble *out);

// This console's own seat on the chat carrier, -1 when chat is off.
int chat_my_seat();

// The game font's character code for a printable ASCII character, or -1 when
// the font has no glyph for it (the cleaner turns those into '?'). The one
// table both the cleaner and the window's glyph atlas read.
int chat_font_code(unsigned char ascii);

// A gray note with no name (run mp1, lane PSTAR1: "<name> got BOB S1").
void chat_system_line(const char *text);

// One "[chat] report ..." line to stderr (also printed once at exit).
void chat_report(const char *tag);

}  // namespace port

#endif  // PORT_HAL_ADV_CHAT_H
