#pragma once

#include <chrono>
#include <vector>
#include <optional>

namespace cactus {

typedef std::chrono::nanoseconds Duration;
typedef std::chrono::steady_clock::time_point TimePoint;

class TimerHeap;
class FiberImpl;

class Timer {
public:
    Timer(TimerHeap* heap) : heap_(heap) {
    }

    ~Timer();

    void Activate(TimePoint deadline);
    void Deactivate();
    bool IsActive() const;

    TimePoint GetDeadline();

    virtual void OnExpired() = 0;

private:
    TimerHeap* const heap_ = nullptr;
    int heap_position_ = -1;

    friend class TimerHeap;
};

class TimerHeap {
public:
    Timer* GetMin();

private:
    struct HeapEntry {
        TimePoint deadline;
        Timer* timer;
    };

    std::vector<HeapEntry> heap_;

    void SiftUp(size_t index);
    void SiftDown(size_t index);

    friend class Timer;
};

}  // namespace cactus
