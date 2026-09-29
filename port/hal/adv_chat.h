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
//
// LOG LINES: "[chat] on: carrier=<vs|hub> seat=<n>" once, "[chat] tx id=<n>
// bytes=<n>" per message sent, "[chat] rx seat=<n> id=<n> bytes=<n>" per
// message shown (after dedupe and the caps), and one "[chat] report ..." line
// with every counter at exit.
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

// The game font's character code for a printable ASCII character, or -1 when
// the font has no glyph for it (the cleaner turns those into '?'). The one
// table both the cleaner and the window's glyph atlas read.
int chat_font_code(unsigned char ascii);

// One "[chat] report ..." line to stderr (also printed once at exit).
void chat_report(const char *tag);

}  // namespace port

#endif  // PORT_HAL_ADV_CHAT_H
