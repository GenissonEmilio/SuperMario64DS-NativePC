#pragma once

#include <string>

namespace sm64ds::discord {

struct Activity {
    std::string details;
    std::string state;
    long long start_timestamp = 0;
};

std::string make_activity_json(const Activity &activity, unsigned long process_id);

class Presence {
public:
    Presence();
    ~Presence();

    Presence(const Presence &) = delete;
    Presence &operator=(const Presence &) = delete;

    // pipe_prefix: the pipe name up to the index; null is Discord's own
    // discord-ipc- pipes (a test points it at a stand-in).
    bool connect(const std::string &application_id, const char *pipe_prefix = nullptr);
    bool update(const Activity &activity);
    void disconnect();
    bool connected() const;
    // Why the last connect did not work ("Discord is not running", ...).
    const std::string &error() const { return error_; }

private:
    void *pipe_;
    std::string error_;
};

} // namespace sm64ds::discord
