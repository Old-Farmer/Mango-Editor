#pragma once

#include <chrono>
#include <functional>
#include <vector>

#include "utils.h"

namespace mango {

using Task = std::function<void()>;

class TimerManager;

class Timer {
    friend TimerManager;

   public:
    Timer(Task task) : task_(std::move(task)) {}
    virtual ~Timer() = default;

   public:
    bool IsTimingOn() { return heap_index_ != -1; }

   protected:
    // Timer moves to it's next state.
    // true if it should check again at follwing ticks,
    // false if it should be removed by the timer manager.
    virtual bool Next(std::chrono::steady_clock::time_point now) = 0;

    // Start the timer(Set the timeout_time_ and init state)
    virtual void Start() = 0;

    std::chrono::steady_clock::time_point timeout_time_;
    Task task_;
    int64_t heap_index_ = -1;
};

class SingleTimer : public Timer {
   public:
    SingleTimer(std::chrono::milliseconds timeout, Task task)
        : Timer(std::move(task)), interval_(timeout) {}

   protected:
    virtual bool Next(std::chrono::steady_clock::time_point now) override {
        (void)now;
        return false;
    }

    virtual void Start() override {
        auto now = std::chrono::steady_clock::now();
        timeout_time_ = now + interval_;
    }

   private:
    std::chrono::milliseconds interval_;
};

class LoopTimer : public Timer {
   public:
    // LoopTimer will do task at every intervals, when the timer reach intervals
    // end, it loops back.
    LoopTimer(std::vector<std::chrono::milliseconds> intervals, Task task)
        : Timer(std::move(task)),
          intervals_(intervals),  // Copy
          interval_i_(intervals.size() == 1 ? 0 : 1) {
        MGO_ASSERT(!intervals.empty());
    }

   protected:
    virtual bool Next(std::chrono::steady_clock::time_point now) override {
        timeout_time_ = now + intervals_[interval_i_];
        interval_i_++;
        if (static_cast<size_t>(interval_i_) > intervals_.size() - 1) {
            interval_i_ = 0;
        }
        return true;
    }

    virtual void Start() override {
        auto now = std::chrono::steady_clock::now();
        timeout_time_ = now + intervals_[0];
        interval_i_ = intervals_.size() == 1 ? 0 : 1;
    }

   private:
    std::vector<std::chrono::milliseconds> intervals_;
    int interval_i_;
};

// A class to manager timer.
// This class don't manage timers lifetime.
// Every methods of this class won't new or delete any timer.
class TimerManager {
   public:
    MGO_DEFAULT_CONSTRUCT_DESTRUCT(TimerManager);
    MGO_DELETE_COPY(TimerManager);
    MGO_DELETE_MOVE(TimerManager);

    // Start monitoring a timer, If the timer has already been monitored,
    // restart it.
    void StartTimer(Timer* timer);

    // Stop monitoring a timer, If the timer has already been removed, nothing
    // happens. Now it's safe to delete a timer.
    void StopTimer(Timer* timer);

    void Tick();

    // -1, means no timer
    // 0, means some timers have already timed out.
    // > 0, means still need n ms for the oldest timer to timeout.
    int64_t NextTimeout();

   private:
    void TimerHeapSiftUp(size_t index);
    void TimerHeapSiftDown(size_t index);
    void PopTimer();
    std::vector<Timer*> timer_heap_;  // a Min heap of timer
};

}  // namespace mango