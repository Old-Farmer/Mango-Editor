#pragma once

#include <chrono>
#include <functional>
#include <vector>

#include "utils.h"

namespace mango {

using Task = std::function<void()>;

struct Timer {
    std::chrono::steady_clock::time_point timeout_time;
    Task task;

    std::vector<std::chrono::milliseconds> intervals;
    int interval_i;

    bool single;
};

class TimerManager {
   public:
    MGO_DEFAULT_CONSTRUCT_DESTRUCT(TimerManager);
    MGO_DELETE_COPY(TimerManager);
    MGO_DELETE_MOVE(TimerManager);

    void AddTimer(std::chrono::milliseconds timeout, Task task);
    void AddTimer(std::vector<std::chrono::milliseconds> intervals, Task task);

    void Tick();

   private:
    std::vector<Timer> timers_;
};

}  // namespace mango