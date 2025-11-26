#include "timer_manager.h"

namespace mango {

void TimerManager::AddSingleTimer(std::chrono::milliseconds timeout,
                                  Task task) {
    timers_.push_back(std::make_unique<SingleTimer>(timeout, std::move(task)));
}

LoopTimer* TimerManager::AddLoopTimer(
    std::vector<std::chrono::milliseconds> intervals, Task task) {
    auto timer =
        std::make_unique<LoopTimer>(std::move(intervals), std::move(task));
    LoopTimer* timer_ptr = timer.get();
    timers_.push_back(std::move(timer));
    return timer_ptr;
}

static inline bool TimeOut(std::chrono::steady_clock::time_point t,
                           std::chrono::steady_clock::time_point now) {
    return t <= now;
}

void TimerManager::Tick() {
    auto now = std::chrono::steady_clock::now();
    for (auto iter = timers_.begin(); iter != timers_.end();) {
        if (TimeOut((*iter)->timeout_time_, now)) {
            (*iter)->task_();
            if (!(*iter)->Next(now)) {
                iter = timers_.erase(iter);
            } else {
                iter++;
            }
        } else {
            iter++;
        }
    }
}

}  // namespace mango