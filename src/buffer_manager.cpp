#include "buffer_manager.h"

#include <cassert>

namespace mango {

BufferManager::BufferManager()
    : list_head_(nullptr), list_tail_(nullptr) {
    list_head_.prev_ = nullptr;
    list_tail_.next_ = nullptr;
    list_head_.next_ = &list_tail_;
    list_tail_.prev_ = &list_head_;
}

Buffer* BufferManager::AddBuffer(Buffer buffer) {
    int64_t buffer_id = buffer.id();
    auto [iter, inserted] = buffers_.emplace(buffer_id, std::move(buffer));
    assert(inserted);
    iter->second.AppendToList(&list_tail_);
    return &iter->second;
}
void BufferManager::RemoveBuffer(Buffer* buffer) {
    buffer->RemoveFromList();
    int64_t id = buffer->id();
    assert(buffers_.count(id) == 1);
    buffers_.erase(id);
}

Buffer* BufferManager::FirstBuffer() {
    if (list_head_.next_ == &list_tail_) {
        return nullptr;
    }
    return list_head_.next_;
}

Buffer* BufferManager::LastBuffer() {
    if (list_head_.next_ == &list_tail_) {
        return nullptr;
    }
    return list_tail_.prev_;
}

Buffer* BufferManager::FindBuffer(Path& path) {
    for (auto b = FirstBuffer(); b != LastBuffer(); b = b->next_) {
        if (b->path().ThisPath() == path.ThisPath()) {
            return b;
        }
    }
    return nullptr;
}
Buffer* BufferManager::FindBuffer(int64_t id) {
    auto iter = buffers_.find(id);
    if (iter == buffers_.end()) {
        return nullptr;
    }
    return &iter->second;
}

}  // namespace mango