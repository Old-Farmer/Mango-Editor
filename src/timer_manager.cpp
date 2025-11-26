#include "timer_manager.h"

namespace mango {

std::shared_ptr<SingleTimer> TimerManager::AddSingleTimer(
    std::chrono::milliseconds timeout, Task task) {
    auto timer = std::make_shared<SingleTimer>(timeout, std::move(task));
    timers_.push_back(timer);
    return timer;
}

std::shared_ptr<LoopTimer> TimerManager::AddLoopTimer(
    std::vector<std::chrono::milliseconds> intervals, Task task) {
    auto timer =
        std::make_shared<LoopTimer>(std::move(intervals), std::move(task));
    timers_.push_back(timer);
    return timer;
}

static inline bool TimeOut(std::chrono::steady_clock::time_point t,
                           std::chrono::steady_clock::time_point now) {
    return t <= now;
}

void TimerManager::Tick() {
    auto now = std::chrono::steady_clock::now();
    for (auto iter = timers_.begin(); iter != timers_.end();) {
        if ((*iter)->cancelled) {
            iter = timers_.erase(iter);
            continue;
        }

        if (!TimeOut((*iter)->timeout_time_, now)) {
            iter++;
            continue;
        }

        (*iter)->task_();
        if (!(*iter)->Next(now)) {
            iter = timers_.erase(iter);
        } else {
            iter++;
        }
    }
}

}  // namespace mango