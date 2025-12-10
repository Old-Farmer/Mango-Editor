#include "buffer_manager.h"

namespace mango {

BufferManager::BufferManager()
    : list_head_(nullptr, false), list_tail_(nullptr, false) {
    list_head_.prev_ = nullptr;
    list_tail_.next_ = nullptr;
    list_head_.next_ = &list_tail_;
    list_tail_.prev_ = &list_head_;
}

Buffer* BufferManager::AddBuffer(Buffer&& buffer) {
    int64_t buffer_id = buffer.id();
    auto [iter, inserted] = buffers_.emplace(buffer_id, std::move(buffer));
    MGO_ASSERT(inserted);
    iter->second.AppendToList(&list_tail_);
    return &iter->second;
}

void BufferManager::RemoveBuffer(Buffer* buffer) {
    MGO_ASSERT(buffer);
    buffer->RemoveFromList();
    int64_t id = buffer->id();
    for (const auto& callback_store : on_buffer_removes_) {
        if (callback_store.handler != nullptr) {
            callback_store.handler(buffer);
        }
    }
    MGO_ASSERT(buffers_.count(id) == 1);
    buffers_.erase(id);
}

BufferManager::HandlerID BufferManager::AddOnBufferRemoveHandler(OnBufferRemove handler) {
    for (size_t i = 0; i < on_buffer_removes_.size(); i++) {
        if (on_buffer_removes_[i].handler == nullptr) {
            on_buffer_removes_[i].handler = std::move(handler);
            return {i, on_buffer_removes_[i].generation};
        }
    }
    on_buffer_removes_.push_back({std::move(handler), 0});
    return {on_buffer_removes_.size() - 1, 0};
}

void BufferManager::RemoveOnBufferRemoveHandler(BufferManager::HandlerID id) {
    if (on_buffer_removes_.size() <= id.id) {
        return;
    }

    if (on_buffer_removes_[id.id].handler == nullptr) {
        return;
    }
    if (on_buffer_removes_[id.id].generation != id.generation) {
        return;
    }
    on_buffer_removes_[id.id].handler = nullptr;
    on_buffer_removes_[id.id].generation++;
}

Buffer* BufferManager::Begin() {
    if (list_head_.next_ == &list_tail_) {
        return nullptr;
    }
    return list_head_.next_;
}

Buffer* BufferManager::End() { return &list_tail_; }

Buffer* BufferManager::FindBuffer(const std::string& name) {
    for (auto b = Begin(); b != End(); b = b->next_) {
        if (b->Name() == name) {
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