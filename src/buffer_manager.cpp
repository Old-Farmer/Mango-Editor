#include "buffer_manager.h"

#include <cassert>

namespace mango {

Buffer* BufferManager::AddBuffer(Buffer buffer) {
    int64_t buffer_id = buffer.id();
    auto [iter, inserted] = buffers_.emplace(buffer_id, std::move(buffer));
    assert(inserted);
    iter->second.AppendToList(buffer_list_tail_);
    return &iter->second;
}
void BufferManager::RemoveBuffer(Buffer* buffer) {
    buffer->RemoveFromList();
    int64_t id = buffer->id();
    buffers_.erase(id);
}
Buffer* BufferManager::FirstBuffer() { return buffer_list_head_.next_; }
Buffer* BufferManager::LastBuffer() {
    if (buffer_list_tail_ == &buffer_list_head_) {
        return nullptr;
    }
    return buffer_list_tail_;
}
Buffer* BufferManager::FindBuffer(Path& path) {
    for (auto b = FirstBuffer(); b != nullptr; b = b->next_) {
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