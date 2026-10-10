#pragma once
#include <algorithm>
#include <cmath>

namespace quiet {
// Pure state machine. "owned" means a successful key injection, not an RPC
// acknowledgement: hotkeys cannot read Discord's real deafen state.
class Engine {
public:
    bool owned = false, ended = false, skippedStart = false;
    float threshold = 60;
    void begin(float start, float target) {
        threshold = std::clamp(target, 1.f, 99.9f);
        ended = false;
        skippedStart = !std::isfinite(start) || start >= threshold;
    }
    template<class Send> bool restore(Send&& send) {
        if (!owned) return true;
        if (!send()) return false;
        owned = false;
        return true;
    }
    template<class Send> void tick(float percent, bool enabled, bool allowed, Send&& send) {
        if (!enabled || !allowed) { restore(send); return; }
        if (ended || skippedStart || owned || !std::isfinite(percent)) return;
        if (percent >= threshold && send()) owned = true;
    }
    template<class Send> void finish(Send&& send) { ended = true; restore(send); }
};
}
