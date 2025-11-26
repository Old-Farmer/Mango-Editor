#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

#include "utils.h"

namespace mango {

using Task = std::function<void()>;

class TimerManager;

class Timer {
    friend TimerManager;

   public:
    Timer(std::chrono::steady_clock::time_point timeout_time, Task task)
        : timeout_time_(timeout_time), task_(std::move(task)) {}
    virtual ~Timer() = default;

    // Timer moves to it's next state.
    // true if it should check again at follwing ticks,
    // false if it should be removed by the timer manager.
    virtual bool Next(std::chrono::steady_clock::time_point now) = 0;

    std::chrono::steady_clock::time_point timeout_time_;
    Task task_;
};

class SingleTimer : public Timer {
   public:
    SingleTimer(std::chrono::milliseconds timeout, Task task)
        : Timer(std::chrono::steady_clock::now() + timeout, std::move(task)) {}

    virtual bool Next(std::chrono::steady_clock::time_point now) override {
        (void)now;
        return false;
    }
};

class LoopTimer : public Timer {
   public:
    // LoopTimer will do task at every intervals, when the timer reach intervals end,
    // it loops back.
    LoopTimer(std::vector<std::chrono::milliseconds> intervals, Task task)
        : Timer(std::chrono::steady_clock::now() + intervals[0],
                std::move(task)),
          intervals_(intervals),  // Copy
          interval_i_(intervals.size() == 1 ? 0 : 1) {
        MGO_ASSERT(!intervals.empty());
    }

    virtual bool Next(std::chrono::steady_clock::time_point now) override {
        timeout_time_ = now + intervals_[interval_i_];
        interval_i_++;
        if (static_cast<size_t>(interval_i_) > intervals_.size() - 1) {
            interval_i_ = 0;
        }
        return true;
    }

    void Restart() {
        auto now = std::chrono::steady_clock::now();
        timeout_time_ = now + intervals_[0];
        interval_i_ = intervals_.size() == 1 ? 0 : 1;
    }

   private:
    std::vector<std::chrono::milliseconds> intervals_;
    int interval_i_;
};

class TimerManager {
   public:
    MGO_DEFAULT_CONSTRUCT_DESTRUCT(TimerManager);
    MGO_DELETE_COPY(TimerManager);
    MGO_DELETE_MOVE(TimerManager);

    void AddSingleTimer(std::chrono::milliseconds timeout, Task task);
    LoopTimer* AddLoopTimer(std::vector<std::chrono::milliseconds> intervals,
                            Task task);

    void Tick();

   private:
    std::vector<std::unique_ptr<Timer>> timers_;
};

}  // namespace mango