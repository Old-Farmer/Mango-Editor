#include "timer_manager.h"

namespace mango {

static inline bool TimeOut(std::chrono::steady_clock::time_point t,
                           std::chrono::steady_clock::time_point now) {
    return t <= now;
}

static inline size_t HeapLeftChild(size_t i) { return 2 * i + 1; }

static inline size_t HeapRightChild(size_t i) { return 2 * i + 2; }

static inline size_t HeapParent(size_t i) { return (i - 1) / 2; }

void TimerManager::StartTimer(Timer* timer) {
    timer->Start();
    if (timer->heap_index_ != -1) {
        if (timer->heap_index_ == 0) {
            TimerHeapSiftDown(0);
        } else if (timer_heap_[HeapParent(timer->heap_index_)] >
                   timer_heap_[timer->heap_index_]) {
            TimerHeapSiftUp(timer->heap_index_);
        } else {
            TimerHeapSiftDown(timer->heap_index_);
        }
        return;
    }

    timer_heap_.push_back(timer);
    timer->heap_index_ = timer_heap_.size() - 1;
    TimerHeapSiftUp(timer_heap_.size() - 1);
}
void TimerManager::StopTimer(Timer* timer) {
    if (timer->heap_index_ != -1) {
        return;
    }

    MGO_ASSERT(!timer_heap_.empty());
    if (timer_heap_.size() == 1) {
        timer->heap_index_ = -1;
        timer_heap_.clear();
        return;
    }
    size_t index = timer->heap_index_;
    timer_heap_[index] = timer_heap_.back();
    timer_heap_[index]->heap_index_ = index;
    timer->heap_index_ = -1;
    timer_heap_.pop_back();
    if (index == 0) {
        TimerHeapSiftDown(0);
    } else if (timer_heap_[HeapParent(index)] > timer_heap_[index]) {
        TimerHeapSiftUp(index);
    } else {
        TimerHeapSiftDown(index);
    }
}

int64_t TimerManager::NextTimeout() {
    if (timer_heap_.empty()) {
        return -1;
    }

    auto now = std::chrono::steady_clock::now();
    if (TimeOut(timer_heap_[0]->timeout_time_, now)) {
        return 0;
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(
               timer_heap_[0]->timeout_time_ - now)
        .count();
}

void TimerManager::TimerHeapSiftUp(size_t index) {
    MGO_ASSERT(index < timer_heap_.size());

    while (index != 0) {
        size_t parent = HeapParent(index);
        if (timer_heap_[index]->timeout_time_ >=
            timer_heap_[parent]->timeout_time_) {
            break;
        }
        std::swap(timer_heap_[index], timer_heap_[parent]);
        std::swap(timer_heap_[index]->heap_index_,
                  timer_heap_[parent]->heap_index_);
        index = parent;
    }
}
void TimerManager::TimerHeapSiftDown(size_t index) {
    MGO_ASSERT(index < timer_heap_.size());

    while (true) {
        size_t min_index = index;
        if (HeapLeftChild(index) < timer_heap_.size()) {
            if (timer_heap_[min_index]->timeout_time_ >
                timer_heap_[HeapLeftChild(index)]->timeout_time_) {
                min_index = HeapLeftChild(index);
            }
        }
        if (HeapRightChild(index) < timer_heap_.size()) {
            if (timer_heap_[min_index]->timeout_time_ >
                timer_heap_[HeapRightChild(index)]->timeout_time_) {
                min_index = HeapRightChild(index);
            }
        }
        if (min_index == index) {
            break;
        }
        std::swap(timer_heap_[index], timer_heap_[min_index]);
        std::swap(timer_heap_[index]->heap_index_,
                  timer_heap_[min_index]->heap_index_);
        index = min_index;
    }
}

void TimerManager::PopTimer() {
    MGO_ASSERT(!timer_heap_.empty());

    timer_heap_[0]->heap_index_ = -1;
    if (timer_heap_.size() == 1) {
        timer_heap_.clear();
        return;
    }
    timer_heap_[0] = timer_heap_.back();
    timer_heap_[0]->heap_index_ = 0;
    timer_heap_.pop_back();
    TimerHeapSiftDown(0);
}

void TimerManager::Tick() {
    auto now = std::chrono::steady_clock::now();
    while (!timer_heap_.empty()) {
        Timer* timer = timer_heap_[0];
        if (!TimeOut(timer->timeout_time_, now)) {
            break;
        }
        timer->task_();
        if (!timer->Next(now)) {
            PopTimer();
        } else {
            TimerHeapSiftDown(0);
        }
    }
}

}  // namespace mango