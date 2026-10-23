#include "timer.h"

#include "debug.h"
#include "scheduler.h"

#include <glog/logging.h>

namespace cactus {

static_assert(sizeof(TimePoint) == sizeof(int64_t));

Timer* TimerHeap::GetMin() {
    if (heap_.empty()) {
        return nullptr;
    }

    return heap_[0].timer;
}

Timer::~Timer() {
    if (IsActive()) {
        Deactivate();
    }
}

TimePoint Timer::GetDeadline() {
    DCHECK(heap_position_ != -1);
    return heap_->heap_[heap_position_].deadline;
}

void Timer::Activate(TimePoint deadline) {
    heap_->heap_.push_back(TimerHeap::HeapEntry{deadline, this});
    heap_position_ = heap_->heap_.size() - 1;
    heap_->SiftUp(heap_position_);
}

void Timer::Deactivate() {
    if (heap_position_ == -1) {
        return;
    }

    size_t position = heap_position_;
    heap_position_ = -1;

    if (position != heap_->heap_.size() - 1) {
        heap_->heap_[position] = heap_->heap_.back();
        heap_->heap_[position].timer->heap_position_ = position;
        heap_->heap_.pop_back();

        heap_->SiftDown(position);
        heap_->SiftUp(position);
    } else {
        heap_->heap_.pop_back();
    }
}

bool Timer::IsActive() const {
    return heap_position_ != -1;
}

void TimerHeap::SiftUp(size_t index) {
    while (index > 0) {
        size_t parent = (index - 1) / 2;
        if (heap_[index].deadline > heap_[parent].deadline) {
            break;
        }

        std::swap(heap_[index], heap_[parent]);
        heap_[index].timer->heap_position_ = index;
        heap_[parent].timer->heap_position_ = parent;

        index = parent;
    }
}

void TimerHeap::SiftDown(size_t index) {
    while (true) {
        size_t smaller = index;

        for (size_t child : {index * 2 + 1, index * 2 + 2}) {
            if (child < heap_.size() && heap_[smaller].deadline > heap_[child].deadline) {
                smaller = child;
            }
        }

        if (smaller != index) {
            std::swap(heap_[index], heap_[smaller]);
            heap_[index].timer->heap_position_ = index;
            heap_[smaller].timer->heap_position_ = smaller;
            index = smaller;
        } else {
            break;
        }
    }
}

}  // namespace cactus
