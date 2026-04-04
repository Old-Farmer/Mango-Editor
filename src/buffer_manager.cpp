#include "buffer_manager.h"

#include "editor_event_manager.h"

namespace mango {

BufferManager::BufferManager(EditorEventManager* editor_event_manager)
    : list_head_(nullptr, false),
      list_tail_(nullptr, false),
      editor_event_manager_(editor_event_manager) {
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
    int64_t id = buffer->id();
    editor_event_manager_->EmitEvent(EditorEvent::kBufferRemoved, buffer);
    buffer->RemoveFromList();
    MGO_ASSERT(buffers_.count(id) == 1);
    buffers_.erase(id);
}

Buffer* BufferManager::Begin() {
    if (list_head_.next_ == &list_tail_) {
        return nullptr;
    }
    return list_head_.next_;
}

Buffer* BufferManager::End() { return &list_tail_; }

Buffer* BufferManager::FindBuffer(const Path& path) {
    for (auto b = Begin(); b != End(); b = b->next_) {
        if (b->path() == path) {
            return b;
        }
    }
    return nullptr;
}

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
