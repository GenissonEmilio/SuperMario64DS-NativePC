# SM64DS port UDP relay

A small server that lets two people play the SM64DS PC port together over the
internet without either of them touching their router.

## Why this exists

Two players behind ordinary home routers cannot send packets straight to each
other. Both routers block anything that arrives out of the blue. The usual fix
is port forwarding, which most people will not do and some cannot do at all.

So neither side waits for a connection. Both sides dial **out** to this relay,
which sits on a public box with one open port. Because each player opened the
conversation, their own router is happy to let the answers back in. The relay
notices that two players quoted the same session code, pairs them up, and from
then on passes their packets along.

It is deliberately dumb. It never looks inside a packet it forwards, never
stores anything on disk, and never speaks to anyone who has not spoken first.
Version 2 (below) adds exactly one kind of looking: in a version 2 session it
checks each datagram's first four bytes and its length against a fixed table,
and writes the sender's seat number into one byte. Payloads are still never
read beyond that, logged, or stored.

## The wire contract

Frozen. The game side and this relay were built against exactly this, and
neither may drift from it without the other.

**HELLO** (client to relay), 16 bytes:

| offset | size | field                                        |
|--------|------|----------------------------------------------|
| 0      | 4    | magic `SMRC`                                 |
| 4      | 1    | version, currently 1                         |
| 5      | 1    | role: 0 parent, 1 child                      |
| 6      | 2    | reserved, send 0                             |
| 8      | 8    | session code, ASCII, right padded with NUL   |

**HELLO-ACK** (relay to client), 16 bytes:

| offset | size | field                                        |
|--------|------|----------------------------------------------|
| 0      | 4    | magic `SMRA`                                 |
| 4      | 1    | version, currently 1                         |
| 5      | 1    | status: 0 ok, 1 session full, 2 bad request  |
| 6      | 2    | reserved, always 0                           |
| 8      | 8    | the code, echoed back                        |

Rules both sides rely on:

- A client repeats its HELLO about once a second until it gets an ACK. That
  same repetition keeps the hole in its router open, so it never stops.
- A repeat HELLO from an endpoint the relay already knows is harmless. It is
  answered again and the pairing is left exactly as it was.
- A session is one code with one parent and up to three children.
- Once paired, every datagram that is not a HELLO is forwarded **verbatim**:
  parent to all children, child to the parent only. Never child to child.
- The relay does not read, rewrite, or reorder payloads. They are opaque.

Two details the game side has to respect, both a consequence of the contract
above rather than choices made here:

- A forwarded packet must not be exactly 16 bytes starting with `SMRC`, or the
  relay will read it as a HELLO. Any other size or any other first four bytes
  is fine.
- The reserved field is not policed. Whatever arrives in it is ignored, and
  zeros are always sent back.

## Version 2: hub sessions, seats, and a kind table

Version 1 above is unchanged and still served exactly as it always was: a
0.5.x game keeps pairing through this relay. Version 2 is for 1.0 games. It
fixes two things version 1 cannot do: through a version 1 relay a joining
player's voice, chat and ghost reach only the host, and the host cannot tell
which joiner sent them (every datagram arrives from the relay's address). In
version 2 the relay knows every seat, sends those kinds to everyone, and
stamps who sent them.

**HELLO v2** (client to relay), 16 bytes, the same length as version 1:

| offset | size | field                                                   |
|--------|------|---------------------------------------------------------|
| 0      | 4    | magic `SMRC`                                            |
| 4      | 1    | version, 2                                              |
| 5      | 1    | role: 0 parent (VS), 1 child (VS), 2 member (hub)       |
| 6      | 1    | want_seat: parent 0, child 1..15, member 0..15, or 255 (any free seat; child or member) |
| 7      | 1    | flags: bit 0 = this endpoint will send voice (informational today) |
| 8      | 8    | session code, ASCII, right padded with NUL              |

**ACK v2** (relay to client), 16 bytes:

| offset | size | field                                                   |
|--------|------|---------------------------------------------------------|
| 0      | 4    | magic `SMRA`                                            |
| 4      | 1    | version, 2                                              |
| 5      | 1    | status (below)                                          |
| 6      | 1    | the seat the relay will stamp on this endpoint's datagrams |
| 7      | 1    | reserved, 0                                             |
| 8      | 8    | the code, echoed back                                   |

Status: 0 ok, 1 full (the session, or the relay), 2 bad request, 3 version
mismatch (the code is already a version 1 session, or the reverse), 4 seat
taken, 5 wrong session kind (a parent or child into a hub code, or a member
into a VS code).

Rules both sides rely on:

- A code is a version 1 or a version 2 session by its first HELLO and never
  mixes. A version 2 HELLO for a version 1 code gets status 3. A version 1
  HELLO for a version 2 code gets a version 1 ACK with status 1 (full),
  because version 1 has no status 3; an old game logs "session is FULL" and
  plays solo, which is a clean refusal rather than a desync.
- A version 2 session is a STAR or a HUB, by its first HELLO's role. A star is
  the VS shape: seat 0 is the parent, seats 1..15 the children (up to
  `SM64DS_RELAY_MAX_CHILDREN`). A hub has seats 0..15, all members, and nobody
  hosts: it survives anyone leaving.
- A child names its seat (the lobby plan's slot); a child or member asking for
  255 gets the lowest free seat, and the ACK says which. A seat held by
  another address is status 4. The game must use the seat the ACK names: that
  is the number the relay writes on its datagrams.
- A repeat HELLO from an endpoint the relay knows, for the same code, role and
  seat (or 255), is the keepalive: re-ACKed, nothing changes. Asking for
  another seat, role or code moves the endpoint (the old seat is freed).
- Hub members send a HELLO every second. A hub seat unheard for 10 seconds is
  freed; a star seat keeps the 90 second rule.

**Forwarding in a version 2 session.** Every datagram that is not a HELLO is
classified by its first four bytes and its exact length against this table.
Anything else is dropped and counted by reason. Nothing is ever forwarded that
is not in the table.

| kind     | tag    | length        | in a VS star                        | in a hub                      | cap/s |
|----------|--------|---------------|-------------------------------------|-------------------------------|-------|
| lockstep | `MP2L` | 0x90 or 0x214 | parent to children, child to parent | dropped                       | 75    |
| ghost    | `SYNG` | 44            | dropped                             | every other member            | 35    |
| identity | `SYNI` | 24..64        | every other seat                    | every other member            | 3     |
| chat     | `SYNT` | 12..132       | every other seat                    | every other member            | 8     |
| voice    | `SYNV` | 176 or 340    | every other seat                    | every other member            | 30    |
| ping     | `SYNP` | 12            | every other seat                    | every other member            | 6     |
| pong     | `SYNQ` | 12            | only the seat named at byte 6       | only the seat named at byte 6 | 6     |
| clock    | `SYNK` | 24            | child to parent                     | dropped                       | 4     |
| sync v4  | `SYN1` | 56            | parent to children, child to parent | dropped                       | 35    |

**Seat stamping.** Before forwarding, the relay overwrites one byte with the
sender's seat: byte 6 of `MP2L` (the packet's slot field), byte 5 of every
`SYN` kind. Whatever the sender wrote there is gone, so no client can speak as
another seat. No other byte changes.

**Rates.** A version 2 seat gets 240 packets a second at its source address
(`SM64DS_RELAY_RATE_PPS_V2`), and inside that each kind has its own cap (the
table's last column, one second deep), so a flood of one kind cannot eat the
budget of another. The per-kind allowances belong to the sender's address,
not to its seat: asking for another seat, session or version does not refill
them. One exception to the table's caps: the host of a VS star (seat 0)
may send 150 `MP2L` a second instead of 75 (`SM64DS_RELAY_HOST_MP2L_PPS`),
because it sends every aggregate and republishes it while a round is open; at
75 a paced 16-player match lost up to 13 percent of the host's lockstep
traffic. Version 1 seats keep their 120.

**The session budget.** Those limits are per sender, and a hub copies each
datagram to up to 15 others, so a version 2 session also has a budget of its
own: at most 15000 copies a second leave the relay for one session
(`SM64DS_RELAY_SESSION_FWD_PPS`, one second deep). A datagram that would go
over it is dropped whole, never sent to some members and not others. The
number is sized from the worst room the game can legitimately make, a full
16-seat hub with every seat talking at once (about 11200 copies a second); a
normal room with a few talkers is about half that, and a 16-player VS match
far less. It exists so that sixteen sockets sending at every cap cannot make
one room cost more than the busiest real room does. Version 1 sessions have
no session budget and behave as they always did.

**A 1.0 game against an old relay.** A version 1 relay reads a version 2
HELLO as a bad request: it answers status 2 in a version 1 ACK and then
ignores that address for 10 seconds. A 1.0 game that falls back to version 1
on that answer should expect its first version 1 HELLOs to go unanswered for
those 10 seconds (or send them from a fresh socket).

## Arena sessions: a referee inside the relay

Arena mode is the VS netcode where every console runs its own arena world and
its own Yoshi with no input delay, and everyone else is drawn slightly in the
past. Anything two players could disagree about is decided by a REFEREE that
lives in this relay (`referee.py`, loaded by `relay.py` from its own folder):
which star wakes next, who broke a marker, who got a contested star, whether
a hit landed, the score, the clock and the winner. There is one referee per
session, so there can never be two winners.

**Joining.** A version 2 HELLO with role 3 opens or joins an arena session.
It is hub-shaped: seats 0 to 15, nobody hosts, a HELLO every second keeps the
seat, 10 seconds of silence frees it. A hub or star code refuses an arena
HELLO with status 5 and the reverse. A relay older than this file answers an
arena HELLO with status 2 (bad request); the game takes that as "this relay
has no arena mode".

**What an arena session forwards.** Its own kind table, `ARENA_KINDS`:

| Kind   | Bytes | Route                                                  |
|--------|-------|--------------------------------------------------------|
| `SYNS` | 48    | every other seat, after the referee range-checks it    |
| `SYNO` | 32    | every other seat, after the referee range-checks it    |
| `SYNE` | 24    | the referee only (a claim), never forwarded            |
| `SYNR` | 272   | from the referee only; a seat sending one is struck    |
| `SYNP` | 12/20 | 12: a peer ping, as a hub; 20 to 0xFE: the referee clock |
| `SYNQ` | 12/20 | 12: a peer pong, as a hub; 20: from the referee only   |
| `SYNI` `SYNT` `SYNV` | as a hub | every other seat                     |

`SYNG`, `MP2L`, `SYNK` and `SYN1` do not exist in an arena session. Byte 5 of
everything is the relay-stamped sender seat; the referee's own frames carry
0xFE there. A state frame whose fields are out of range (a position past
32768 units, an animation id of 194 or more, a state index past the 78
player states, and so on) is dropped instead of forwarded.

**How the referee decides.** Claims carry the console's referee-clock stamp
of the moment it touched. A stamp is clamped into the window from the
claim's arrival minus that seat's cap (half its best round trip plus 25 ms,
never more than 150) up to its arrival, so nobody can claim from the future
or back-date further than their own one-way time. Claims are decided in
stamp order once the referee clock is past the stamp plus the largest cap of
the other players, which is when no earlier claim can still arrive. A hit is
checked against the attacker's and the victim's OWN reported positions at
the frames involved (lag compensation, at most 200 ms back). The full rule
set, every byte layout and every number is `arena_contract.py`, which writes
`arena_contract.json` and the game's `arena_wire.h`.

**Not an amplifier.** The referee sends nothing to a seat until the seat has
proven it receives at its address: its clock ping must echo the exact referee
time of a pong the referee sent it, and the referee clock starts at a random
point per session, so a sender with a forged source address cannot echo one.
Until then the only thing the seat gets back is a pong the size of its ping.

**Cost.** One more Python module on the box, a 10 ms clock that runs only
while an arena session exists, and per state frame a fixed-size parse.
Measured on a local copy (a desktop PC shared with other work, so the
numbers are noisy): a full 16-seat arena at 30 Hz delivered all 7200 copies
a second with no drops for 17.6 to 18.3% of one core, against 5.4 to 10.7%
for the same traffic as a plain hub on the same relay; with four voice
talkers 37.8%; a 4-seat arena 0.2%. The live box is roughly half as fast
per copy as that PC.

**Phase A limits.** Classic mode only (king of the hill and Luigi rooms stay
on lockstep); the eat claims are provisional; speed strikes are logged and
`SM64DS_REFEREE_KICK=1` turns kicking on (phase B).

## Limits, and what happens when you hit one

Every one of these is enforced, and every one has a test.

| Limit                        | Value | On breach                                  |
|------------------------------|-------|--------------------------------------------|
| Forwarded payload            | 700 B | Larger datagrams are dropped in silence     |
| Rate per endpoint            | 120/s | Excess dropped; a 120 packet burst is fine  |
| Sessions at once             | 256   | A new code gets status 1                    |
| Endpoints per session        | 4     | 1 parent + 3 children; the next gets status 1 |
| Idle before a seat is dropped| 90 s  | The player must HELLO again to get it back  |
| Malformed HELLO              |       | One status 2 reply, then that address is ignored for 10 s |
| Rate per v2 endpoint         | 240/s | Excess dropped; per-kind caps inside it (the kind table) |
| Seats per v2 hub             | 16    | The next member gets status 1               |
| Idle before a hub seat is freed | 10 s | The member's next HELLO seats it again   |
| v2 datagram not in the table |       | Dropped, counted under its reason           |
| Copies sent per v2 session   | 15000/s | Excess dropped whole, counted as `drop_session_cap` |

120 packets a second is generous for 30 frames a second of lockstep play plus
retries. A player sending more than that is either broken or malicious, and
either way the fix is the same.

The relay never sends more bytes to an unknown address than it just received
from it. That is what stops it being used to bounce amplified traffic at
somebody else, and it is measured in the test suite rather than assumed.

## Install

The kit is this directory. Copy it to the server and run the script there:

    scp -r relay/ youruser@your.server:/tmp/sm64ds-relay-kit
    ssh youruser@your.server
    sudo bash /tmp/sm64ds-relay-kit/deploy.sh

`deploy.sh` echoes every step as it goes. It:

1. checks python3 is present and new enough, and that relay.py compiles,
2. creates a locked down `sm64ds-relay` account if it is not already there,
3. copies relay.py into `/home/sm64ds-relay/sm64ds-relay/`,
4. writes the systemd unit, reloads, enables and starts it,
5. checks the service is up, shows the listening socket and the last log lines,
6. sends one HELLO to itself and reports the answer.

Run it again any time. It only changes what actually differs, so rerunning it
is the normal way to push a new relay.py. `--dry-run` shows what it would do
and changes nothing. `--port`, `--user` and `--idle` override the defaults.

Requirements on the server: python3 (3.7 or newer) and systemd. No packages to
install, no dependencies, nothing written to disk at runtime.

## Firewall

**Only UDP 41234 needs to be open.** Nothing else. The relay makes no outbound
connections of its own and needs no other port.

`deploy.sh` deliberately does not change firewall rules. Run whichever of these
matches the box:

ufw:

    sudo ufw allow 41234/udp comment 'SM64DS relay'
    sudo ufw status verbose

raw iptables (add the ip6tables line too if the box has IPv6):

    sudo iptables -A INPUT -p udp --dport 41234 -j ACCEPT
    sudo ip6tables -A INPUT -p udp --dport 41234 -j ACCEPT
    sudo iptables -L INPUT -n --line-numbers | grep 41234

If the host also sits behind a cloud provider firewall or security group,
allow UDP 41234 inbound there as well or nothing will ever arrive.

## Verify

    ss -ulnp | grep 41234
    journalctl -u sm64ds-relay -n 20
    systemctl status sm64ds-relay

Watch two players connect live:

    journalctl -u sm64ds-relay -f

From any machine that has this kit, against the public address:

    python3 test_client.py probe --host YOUR.SERVER.IP --port 41234
    python3 test_client.py remote-check --host YOUR.SERVER.IP --port 41234

`probe` sends one HELLO and prints the answer and the round trip. `remote-check`
pairs a parent and a child through the relay and measures 100 round trips, so a
clean run proves the whole path end to end, not just that something is
listening.

## Two players in one VS match through this relay

The game side, one line per machine. The two must share the code and must not
share the role.

    SM64DS_VS_MAP=0 SM64DS_COMMS_RELAY=your.server:41234 \
      SM64DS_COMMS_CODE=SOMECODE SM64DS_COMMS_ROLE=0    walk_window.exe

    SM64DS_VS_MAP=0 SM64DS_COMMS_RELAY=your.server:41234 \
      SM64DS_COMMS_CODE=SOMECODE SM64DS_COMMS_ROLE=1    walk_window.exe

`SM64DS_COMMS_ROLE` takes the role numbers from the datagram table above (0
parent, 1 child) or the words `parent` and `child`; they mean the same thing.
`SM64DS_VS_MAP` is 0 to 3 over the ROM's own four VS maps. A host name works
here as well as an address.

It worked when both logs carry these, which the game writes into
`playlog/play_*.log` unless `SM64DS_NO_PLAYLOG=1` is set:

    [comms:loopback] open(mode=2) as parent, ... via RELAY ..., code 'SOMECODE'
    [comms:relay] paired as parent on code 'SOMECODE'; the relay will forward
    [comms:conductor] session up after N turns: ... players=2

Any `[comms:loopback]` line saying NOT installing is the game refusing before
it ever reached the relay, and the line names the knob it refused. The whole
path is asserted end to end by `port/tools/vs_online_proof.py`.

## Why it does not run at a round trip per frame

The match is lockstep: both machines run the same frame from the same pair of
inputs, which is what keeps two copies of the game telling the same story with
no guessing and no rewinding. Done the obvious way that means a frame cannot
finish until the other player's input for that same frame has crossed the wire
and come back, so **the frame rate is the round trip**. Through this relay that
is around 90 ms, which is about 11 frames a second against the game's 30, and
it feels exactly as bad as it sounds.

The fix is an **input delay**. Each machine runs frame R from the inputs that
were exchanged N frames ago, so the wire has N whole frames to deliver a packet
instead of a fraction of one. As long as **both machines use the same N**, they
still play the identical sequence of inputs in the identical order -- nothing
is predicted, nothing is thrown away, and the two games cannot drift apart.
That "same N" is not a suggestion; see the next section for what happens
otherwise and how the game now guarantees it. The only cost is that a button
press lands N frames later, and N frames of that is far less noticeable than
the whole game running at a third speed.

**It is on by default and the recipe above needs no extra knob.** The game
picks the depth from how it is connected: 5 frames through a relay, 2 on a
direct connection, 0 on this machine's own loopback where there is no round
trip to hide. The relay number is measured rather than derived -- 4 is what the
arithmetic gives on a good hour and it is the wrong answer on an ordinary bad
one, where 5 takes the stalls from 44 in 360 frames down to 5. On the shape of
connection this relay presents, going past 5 buys nothing; on a connection that
swings much more wildly it still can, which is what the knob is for.

### The number is set on the host, and only on the host

**Both machines must run the same depth, and different depths break the game.**
Not "run out of sync in time" -- actually break it: the two copies start
playing different matches, positions drift apart after a couple of seconds, and
**both machines still report a perfectly healthy connection the whole time**.
Nothing in either log says anything is wrong. It is the nastiest failure this
thing can produce, so the game no longer lets you cause it by hand:

- **The host decides.** The host's depth travels inside the reply that lets the
  other player in, and the joining machine adopts it and says so in its log:
  `the parent runs input delay 5 and this end had 2; ADOPTING 5`.
- **Setting it on the joining machine does nothing.** The knob is read, then
  overruled by the host, and the log line says it lost. Set it on the host.
- **Old copies of the game are refused, loudly.** A build from before this
  existed has no depth to send and would default to none, so the two would
  desync. They now will not pair at all, and the log says why:
  `REFUSING a peer speaking wire version 1; this build speaks 2`. If you see
  that, one of you is on an older build -- update and try again.

  That version number is the **game's** own, carried inside the packets this
  relay forwards without looking at them. It is not the HELLO version in the
  wire contract above, which is still 1 and unchanged. **Nothing about this
  needs the relay redeployed** -- an unmodified relay carries both generations
  perfectly well, which is precisely why the two games have to refuse each
  other themselves.

`SM64DS_COMMS_INPUT_DELAY=<0..8>` **on the host** overrides the default, and
there are two reasons to reach for it:

- **Your pair is further apart than the default covers.** The signal is
  `starved` on the line the game writes when the session ends, in
  `playlog/play_*.log`:

      [comms:loopback] closed after 600 rounds; indelay=5 starved=8 sent=...

  A handful over a long session is nothing -- a starved frame waits only for
  the part of the round trip that ran past the budget, not for a whole one. A
  number that climbs steadily means frames are routinely waiting on the wire.
  Raise **the host's** number by one and look again; the other machine follows
  on its own. The rule is `N >= round trip in ms / 33.3`, plus a frame of
  headroom, and more headroom on a jumpy connection than on a merely slow one --
  a steady 120 ms is covered by a depth, a connection that swings between 80 and
  400 ms wants more. `python3 test_client.py remote-check --host YOUR.SERVER.IP`
  measures the round trip and prints the N it implies.
- **You want the old behaviour back to measure it.** `=0` on the host is
  stop-and-wait exactly as it was, on both machines.

`port/tools/vs_pace.py` measures the whole thing end to end: solo, a pair
through a relay on this machine, and a pair through the live relay, with and
without the input delay, reporting the frame-time distribution for each.

## Rollback

    sudo systemctl disable --now sm64ds-relay

That stops it and stops it coming back on boot. To remove it completely:

    sudo rm /etc/systemd/system/sm64ds-relay.service
    sudo systemctl daemon-reload
    sudo rm -rf /home/sm64ds-relay/sm64ds-relay

## Configuration

Set in the unit file, visible with `systemctl cat sm64ds-relay`:

| Variable               | Default   | Meaning                            |
|------------------------|-----------|------------------------------------|
| `SM64DS_RELAY_PORT`    | 41234     | UDP port to listen on              |
| `SM64DS_RELAY_BIND`    | 0.0.0.0   | address to bind (`::` for IPv6)    |
| `SM64DS_RELAY_IDLE_S`  | 90        | seconds before an idle seat is dropped |
| `SM64DS_RELAY_STATS_S` | 60        | seconds between counter lines in the log |
| `SM64DS_RELAY_MAX_CHILDREN` | 3    | children one VS session holds (the live box runs 15) |
| `SM64DS_RELAY_RATE_PPS` | 120      | per-address packet budget of a version 1 seat |
| `SM64DS_RELAY_RATE_PPS_V2` | 240   | per-address packet budget of a version 2 seat |
| `SM64DS_RELAY_HUB_IDLE_S` | 10     | seconds before a quiet hub seat is freed |
| `SM64DS_RELAY_SESSION_FWD_PPS` | 15000 | copies a second one version 2 session may make the relay send |
| `SM64DS_RELAY_HOST_MP2L_PPS` | 150 | `MP2L` a second the host (seat 0) of a version 2 VS star may send |
| `SM64DS_REFEREE_KICK` | off | `1` = the arena referee kicks a seat at 3 strikes in 10 s |
| `SM64DS_REFEREE_SEED` | random | tests only: a fixed arena match seed and clock base |

After editing the unit: `sudo systemctl daemon-reload && sudo systemctl restart
sm64ds-relay`. Changing the port means changing the firewall rule to match, and
telling the game side.

## Reading the log

Everything goes to the journal. Session lifecycle only, never payloads, and
client addresses are cut down before they are written:

    session ABCD1234 create by 203.0.x.x#51820
    session ABCD1234 join 203.0.x.x#51820 as parent (1/4)
    session ABCD1234 join 198.51.x.x#40311 as child (2/4)
    session ABCD1234 leave 198.51.x.x#40311 (idle)
    session ABCD1234 expire

A version 2 session names its shape and its seats:

    session W0000001 create by 203.0.x.x#51820 (v2 hub)
    session W0000001 join 203.0.x.x#51820 as member seat 0 (1/16) v2
    session W0000001 refused 198.51.x.x#40311: seat 3 taken (v2 status 4)
    session W0000001 leave 203.0.x.x#51820 seat 0 (idle)

and once a minute a counter line:

    stats sessions=1 endpoints=2 rx=41233 fwd=41180 ack=53 drop_rate=0
    drop_size=0 drop_unpaired=0 drop_ignored=0 bad=0 full=0 expired=0
    rss_kb=14208

What the counters mean: `rx` datagrams received, `fwd` datagrams passed on,
`ack` HELLO answers sent, `drop_rate` dropped for exceeding the packet rate,
`drop_size` dropped for being over 700 bytes, `drop_unpaired` from an address
with no session, `drop_ignored` from an address inside its 10 second penalty,
`bad` malformed HELLOs answered, `full` refusals for a full session or a full
relay, `expired` seats dropped for going quiet, `rss_kb` the relay's own memory.

The line then carries `reset`, socket errors the OS reported and the relay
ignored (see "Running it on Windows" below), and the version 2 counters:
`v2_sessions` and `hubs` open now; HELLO refusals `seat_taken`, `vmix` (a code of the other version) and
`wrong_kind`; drops by reason, `drop_unknown` (a tag not in the table),
`drop_length` (a length the table does not list for that tag), `drop_route`
(a kind the session's shape does not carry, or a parent's clock report),
`drop_cap` (over that kind's cap), `drop_target` (nobody to send it to: a pong
for an empty seat, a parent alone), `drop_session_cap` (over the session
budget); and per kind `TAG=rx/fwd/drop`, where
`fwd` counts copies sent:

    reset=0 v2_sessions=1 hubs=1 seat_taken=0 vmix=0 wrong_kind=0 drop_unknown=0
    drop_length=0 drop_route=0 drop_cap=0 drop_target=0 drop_session_cap=0
    MP2L=0/0/0
    SYNG=720/10800/0 SYNI=0/0/0 SYNT=0/0/0 SYNV=236/3540/0 SYNP=96/1440/0
    SYNQ=0/0/0 SYNK=0/0/0 SYN1=0/0/0

## Running it on Windows (local tests)

The deployed relay runs on Linux. Local test rungs run it on Windows, where
one thing differs: when a player's game exits, the relay's next forward to
that player's closed port comes back as an ICMP port-unreachable, and Windows
reports that as an error on the relay's socket (WSAECONNRESET). Left alone,
that error stops Python's asyncio loop receiving on the socket for good: every
seat then times out on every screen. The relay handles it twice over:

- At startup it turns the report off for its socket (`SIO_UDP_CONNRESET`);
  the startup log says `windows: udp connreset off`.
- If an error still arrives, on any platform, it is logged once, counted as
  `reset=` in the stats line, and receiving carries on.

`SM64DS_RELAY_KEEP_CONNRESET=1` skips the first step. It exists only so the
suite can prove the second one on its own; never set it anywhere else.

## Troubleshooting

**Service will not start.** `systemctl status sm64ds-relay` and
`journalctl -u sm64ds-relay -n 50`. The usual cause is the port already being
in use: `ss -ulnp | grep 41234` will name the other process.

**Players cannot connect but the service is up.** Almost always the firewall
or a cloud security group. Check from outside with
`python3 test_client.py probe --host YOUR.SERVER.IP`. No answer with a healthy
service means the packet is not reaching the box.

**One player connects, the other does not.** Look for two `join` lines with the
same code in the log. If only one appears, the other player never reached the
relay, or typed a different code. If you see a `refused` line, the session was
already full.

**Players connect then drop after a minute and a half.** Their client stopped
sending its keepalive HELLOs; the 90 second idle expiry then dropped the seat.
Look for `leave ... (idle)`.

**It stops relaying under fast play.** Check `drop_rate` in the counter line.
If it is climbing, something is sending more than the packet rate allows
(`rate_pps` on the startup line, 120 by default).

**Voice chat sessions need a raised packet rate.** A seat with proximity voice
chat on sends about 117 packets a second: up to 60 lockstep input records, 30
state snapshots, 2 RTT probes and 25 voice datagrams. That is three packets
under the stock 120 budget, which is not headroom, and what the relay does when
a seat goes over is drop -- silently, so it looks like a desync rather than a
limit. Set

    Environment=SM64DS_RELAY_RATE_PPS=240

in the unit file before hosting voice sessions and restart. The relay says so
itself on startup while the rate is still at the pre-voice number. Voice
datagrams are 340 bytes, well inside the 700 byte cap, so nothing else about
the relay's contract moves.

**A player who dropped out cannot get back in for a minute or so.** If their
router hands them a different outside port when they reconnect, the relay sees
a brand new player, and their old seat is still held by the address that went
quiet. They get status 1 until that seat times out, which takes up to 90
seconds. Waiting clears it; so does using a fresh session code. This is the
price of never letting a stranger take over a seat by quoting the code.

## Security posture

- Payloads are never logged or interpreted. A version 1 session never
  inspects them at all; a version 2 session reads only the first four bytes
  and the length (to classify) and, for a pong, byte 6 (the seat to send it
  to), and writes one byte (the sender's seat).
- In a version 2 session nothing is forwarded that is not in the kind table,
  and no client can claim another seat: the relay stamps it.
- The relay speaks only to addresses that spoke to it first.
- In an arena session the referee reads fixed-size kinds only, range-checks
  every field, keeps every table bounded, trusts no client for anything it
  can check (the score is its own ledger), and sends nothing to a seat that
  has not echoed one of its clock pongs.
- It never sends an unknown address more bytes than it just received, so it
  cannot be used to amplify traffic at a third party.
- A malformed sender gets one short answer and is then ignored for 10 seconds.
- It runs as an unprivileged account with no login, no capabilities, a
  read-only view of the filesystem and a restricted system call set.
- No credentials, keys, or secrets exist anywhere in this kit. A session code
  is not a secret: it is a meeting point, and anyone who guesses one can join
  that session as a spare child. Codes should be random enough not to be
  guessed by accident.

## Tests

The suite runs entirely locally. It spawns relay instances on ports derived
from its own process id, so several runs can go at once without colliding.

    python3 test_client.py selftest
    python3 test_client.py soak --minutes 10

`selftest` covers pairing and a thousand datagrams each way with round trip
times and loss, star routing including that one child never sees another's
packets, a second child joining a live session, two sessions on one relay never
crossing, the 700 byte drop, the rate cap engaging and releasing, idle expiry
and recovery by re-HELLO, malformed HELLO handling with the bytes in and out
counted, re-HELLO idempotence, and the session and endpoint caps.

`soak` runs two paired endpoints trading packets and reports loss and the
relay's memory at the start and the end.

    python3 test_relay_v2.py

is the version 2 suite. Its first half drives relay.py in process with its own
clock: every HELLO status, every drop reason, every route and stamp for every
kind in both session shapes, the two per-address budgets, hub and star idle
expiry, version 1 sessions staying blind, and a fuzz pass of random datagrams
that checks nothing outside the kind table ever leaves the relay. Its second
half runs the relay as a subprocess over real sockets: a 16-seat hub where
every member hears the other fifteen under their own seats, a spoofed seat,
oversize and flood datagrams, a version 1 session beside a version 2 one, and
a member whose process dies mid-session while the others keep hearing each
other (on Windows twice: with the OS report turned off, and left on), and
sixteen sockets flooding one hub held to the session budget while the hub
beside them hears everything.

    python3 test_referee.py              (unit + contention vectors + fuzz)
    python3 test_referee.py fuzz 150000

is the arena suite: simulated consoles through relay.py and referee.py in
process, covering the contract copies, the handshake and the no-amplifier
rule, every arena route, a whole match, contention, hits, resends and
snapshots, leaving, strikes, and a fuzz pass whose gate is zero violations
(nothing forwarded outside the table, no malformed or unearned referee
frame, bounded tables, a conserved ledger, one MATCH_OVER per match).
`arena_vectors.json` is its two-console star races, shared with the game's
fairness rig. `python3 arena_contract.py --check` proves the JSON and the C
header match their generator.
