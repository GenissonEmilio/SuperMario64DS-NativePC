// MOVERS9: the always-on [syncstat] sync numbers (see coop_stats_log.cpp). One call per frame from coop_actors' frame hook (the world's
// frame-callback table has 8 slots and they are all taken, so this file does not register one of its own).
#ifndef PORT_HAL_COOP_STATS_LOG_H
#define PORT_HAL_COOP_STATS_LOG_H
namespace port {
void coop_stats_log_tick(unsigned now_ms);
}  // namespace port
#endif
