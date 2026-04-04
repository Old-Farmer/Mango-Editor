#pragma once

#include <unordered_map>

#include "buffer.h"
#include "utils.h"

namespace mango {

class Buffer;
class Path;
class EditorEventManager;

using OnBufferRemove = std::function<void(const Buffer*)>;

class BufferManager {
   public:
    BufferManager(EditorEventManager* editor_event_manager);
    ~BufferManager() = default;
    MGO_DELETE_COPY(BufferManager);
    MGO_DELETE_MOVE(BufferManager);

    Buffer* AddBuffer(Buffer&& buffer);
    void RemoveBuffer(Buffer* buffer);

    Buffer* Begin();
    Buffer* End();  // exclusive
    // By path.
    Buffer* FindBuffer(const Path& path);
    // By buffer name.
    Buffer* FindBuffer(const std::string& name);
    // By id.
    Buffer* FindBuffer(int64_t id);

   private:
    std::unordered_map<int64_t, Buffer> buffers_;
    Buffer list_head_;
    Buffer list_tail_;

    EditorEventManager* editor_event_manager_;
};

}  // namespace mango
