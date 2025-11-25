#include "timer_manager.h"

namespace mango {

void TimerManager::AddTimer(std::chrono::milliseconds timeout, Task task) {
    timers_.push_back({std::chrono::steady_clock::now() + timeout,
                       std::move(task),
                       {},
                       0,
                       true});
}

void TimerManager::AddTimer(std::vector<std::chrono::milliseconds> intervals,
                            Task task) {
    MGO_ASSERT(!intervals.empty());
    timers_.push_back({std::chrono::steady_clock::now() + intervals[0],
                       std::move(task), intervals, intervals.size() > 1 ? 1 : 0,
                       false});
}

void TimerManager::Tick() {
    auto now = std::chrono::steady_clock::now();
    for (auto iter = timers_.begin(); iter != timers_.end();) {
        if (iter->timeout_time <= now) {
            iter->task();
            if (iter->single) {
                iter = timers_.erase(iter);
            } else {
                iter->timeout_time = now + iter->intervals[iter->interval_i];
                iter->interval_i++;
                if (static_cast<size_t>(iter->interval_i) >
                    iter->intervals.size() - 1) {
                    iter->interval_i = 0;
                }
                iter++;
            }
        } else {
            iter++;
        }
    }
}

}  // namespace mango